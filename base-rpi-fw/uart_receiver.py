"""
uart_receiver.py

Receives file transfers from the ESP32 over UART.

Protocol (matches uartFileTransfer.c exactly):

    Frame: [0xAA][TYPE][LEN][PAYLOAD...][CHECKSUM]
    CHECKSUM = XOR of TYPE ^ LEN ^ all payload bytes
    START byte is NOT included in checksum.

    Transfer flow:
        ESP32 → START (payload: fileSize as uint32 LE)
        Pi    → ACK
        ESP32 → DATA  (payload: 1-255 bytes of file content)
        Pi    → ACK
        ... repeat DATA / ACK ...
        ESP32 → END   (no payload)
        Pi    → ACK
        Pi verifies file, parses CSV, stores in DB
        Pi    → COMMIT (payload: 0x00 = success, else failure)
        ESP32 clears its SPIFFS file on COMMIT(0x00)

    Timing:
        ESP32 waits 500ms for each ACK, retries up to 5 times.
        ESP32 waits 2000ms for COMMIT after END is ACKed.
        → Pi must ACK fast (< 500ms) and COMMIT within 2 seconds.

Runs as a background daemon thread. Flask runs on the main thread.
"""

import serial
import struct
import threading
import time
import os
import logging
import io
import csv

import config
import database

log = logging.getLogger("uart_rx")

# ── Shared state (thread-safe via _lock) ───────────────────────────

_transfer_active = False
_last_transfer_ok = None
_total_transfers = 0
_lock = threading.Lock()


def get_status():
    """Return current transfer state for the /api/status endpoint."""
    with _lock:
        return {
            "transfer_active": _transfer_active,
            "last_transfer_ok": _last_transfer_ok,
            "total_transfers": _total_transfers,
        }


# ── Packet building / parsing ──────────────────────────────────────

def _xor_checksum(type_byte, len_byte, payload):
    """
    XOR checksum matching checksumXor() in uartFileTransfer.c.
    Covers: type ^ len ^ each payload byte.
    Does NOT include the 0xAA start byte.
    """
    c = type_byte ^ len_byte
    for b in payload:
        c ^= b
    return c & 0xFF


def _build_packet(pkt_type, payload=b""):
    """
    Build a complete framed packet for sending TO the ESP32.
    Used for ACK and COMMIT.

    ACK  = [0xAA][0x04][0x00][0x04]
    COMMIT(ok)  = [0xAA][0x05][0x01][0x00][0x04]
    COMMIT(fail)= [0xAA][0x05][0x01][0x01][0x05]
    """
    pkt_len = len(payload)
    chk = _xor_checksum(pkt_type, pkt_len, payload)
    return bytes([config.START_BYTE, pkt_type, pkt_len]) + payload + bytes([chk])


def _send_ack(ser):
    """Send ACK to ESP32. Must be called quickly (< 500ms window)."""
    pkt = _build_packet(config.TYPE_ACK)
    ser.write(pkt)
    ser.flush()
    log.debug("→ ACK")


def _send_commit(ser, status):
    """Send COMMIT to ESP32. 0x00 = success, anything else = failure."""
    pkt = _build_packet(config.TYPE_COMMIT, bytes([status]))
    ser.write(pkt)
    ser.flush()
    log.info("→ COMMIT (0x%02X %s)", status, "OK" if status == 0x00 else "FAIL")


def _read_packet(ser):
    """
    Read one framed packet from the serial port.

    Returns (pkt_type, payload_bytes) on success.
    Returns (None, None) on timeout or invalid frame.

    The serial port timeout (set in _receiver_loop) controls how long
    we block waiting for data. Typically 1 second.
    """
    # Scan for start byte, skip any noise / garbage
    while True:
        b = ser.read(1)
        if len(b) == 0:
            return None, None      # timeout, no data
        if b[0] == config.START_BYTE:
            break

    # Read header: [type][len]  (2 bytes)
    hdr = ser.read(2)
    if len(hdr) != 2:
        log.warning("Incomplete header (got %d/2 bytes)", len(hdr))
        return None, None

    pkt_type = hdr[0]
    pkt_len = hdr[1]

    if pkt_len > config.MAX_PAYLOAD:
        log.warning("Payload length %d exceeds max %d", pkt_len, config.MAX_PAYLOAD)
        return None, None

    # Read payload + checksum  (pkt_len + 1 bytes)
    body = ser.read(pkt_len + 1)
    if len(body) != pkt_len + 1:
        log.warning("Incomplete body (expected %d, got %d)", pkt_len + 1, len(body))
        return None, None

    payload = body[:pkt_len]
    received_chk = body[pkt_len]

    expected_chk = _xor_checksum(pkt_type, pkt_len, payload)
    if received_chk != expected_chk:
        log.warning("Checksum mismatch: got 0x%02X expected 0x%02X (type=0x%02X len=%d)",
                     received_chk, expected_chk, pkt_type, pkt_len)
        return None, None

    return pkt_type, bytes(payload)


# ── CSV parsing ────────────────────────────────────────────────────

def _parse_and_store_csv(raw_bytes):
    """
    Parse the received file as CSV and insert rows into the database.

    CSV format from shears (gps_points.csv):
        utc_time,latitude,longitude,fix_quality,num_satellites,hdop,altitude,geoid_height
        123456.00,29.6516,-82.3248,1,8,1.2,30.5,0.0
        ...

    The first row may be a header — detected by checking if the first
    field parses as a number. If not, it's a header and we skip it.

    Returns the number of rows successfully inserted.
    """
    try:
        text = raw_bytes.decode("utf-8", errors="replace")
    except Exception as e:
        log.error("Failed to decode CSV bytes: %s", e)
        return 0

    reader = csv.reader(io.StringIO(text))
    records = []

    for row in reader:
        # Need at least 8 columns for the full GPS record
        if len(row) < 8:
            # Could be a partial/malformed line, try with fewer fields
            if len(row) < 3:
                continue

        # Header detection: try parsing first field as a number
        try:
            float(row[0])
        except ValueError:
            # First field isn't numeric — this is the header row, skip it
            log.debug("Skipping header row: %s", row)
            continue

        try:
            record = {
                "utc_time":       row[0],
                "latitude":       float(row[1]) if len(row) > 1 else 0.0,
                "longitude":      float(row[2]) if len(row) > 2 else 0.0,
                "fix_quality":    int(row[3])   if len(row) > 3 else 0,
                "num_satellites": int(row[4])   if len(row) > 4 else 0,
                "hdop":           float(row[5]) if len(row) > 5 else 0.0,
                "altitude":       float(row[6]) if len(row) > 6 else 0.0,
                "geoid_height":   float(row[7]) if len(row) > 7 else 0.0,
            }
            records.append(record)
        except (ValueError, IndexError) as e:
            log.warning("Skipping malformed row %s: %s", row, e)

    if records:
        inserted = database.insert_points_batch(records)
        log.info("CSV parsed: %d rows inserted into database", inserted)
        return inserted
    else:
        log.warning("No valid data rows found in CSV")
        return 0


def _save_raw_file(raw_bytes):
    """Save a timestamped backup of the raw received file."""
    os.makedirs(config.RECEIVED_FILES_DIR, exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    filepath = os.path.join(config.RECEIVED_FILES_DIR, f"gps_points_{ts}.csv")
    try:
        with open(filepath, "wb") as f:
            f.write(raw_bytes)
        log.info("Raw backup saved: %s (%d bytes)", filepath, len(raw_bytes))
    except Exception as e:
        log.error("Failed to save raw file: %s", e)


# ── Main receiver loop (runs in daemon thread) ─────────────────────

def _receiver_loop(port, baud):
    """
    Main loop: open serial port, handle file transfers.

    State machine:
        IDLE → recv START → send ACK → RECEIVING
        RECEIVING → recv DATA → send ACK → RECEIVING
        RECEIVING → recv END → send ACK → verify → send COMMIT → IDLE
    """
    global _transfer_active, _last_transfer_ok, _total_transfers

    log.info("UART receiver starting on %s @ %d baud", port, baud)

    while True:
        try:
            ser = serial.Serial(
                port=port,
                baudrate=baud,
                timeout=1.0,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
            )
            log.info("Serial port opened: %s", port)

            while True:
                pkt_type, payload = _read_packet(ser)

                if pkt_type is None:
                    continue    # timeout — keep listening

                # ── START: new file transfer ──────────────────────
                if pkt_type == config.TYPE_START:
                    if len(payload) < 4:
                        log.warning("START missing fileSize payload, ignoring")
                        continue

                    expected_size = struct.unpack("<I", payload[:4])[0]
                    log.info("══════════════════════════════════════")
                    log.info("  TRANSFER START  (expecting %d bytes)", expected_size)
                    log.info("══════════════════════════════════════")

                    with _lock:
                        _transfer_active = True

                    # ACK the START immediately
                    _send_ack(ser)

                    # ── Inner loop: receive DATA until END ────────
                    file_buffer = bytearray()
                    transfer_ok = False

                    while True:
                        pkt_type2, payload2 = _read_packet(ser)

                        if pkt_type2 is None:
                            # Timeout — ESP32 will retry, keep waiting
                            continue

                        if pkt_type2 == config.TYPE_DATA:
                            file_buffer.extend(payload2)
                            log.debug("  DATA +%d bytes  (%d / %d)",
                                      len(payload2), len(file_buffer), expected_size)
                            # ACK immediately — ESP32 blocks until it gets this
                            _send_ack(ser)

                        elif pkt_type2 == config.TYPE_END:
                            log.info("  END received  (%d / %d bytes)",
                                     len(file_buffer), expected_size)
                            # ACK the END immediately
                            _send_ack(ser)

                            # Now we have up to 2 seconds to verify and COMMIT.
                            # Verify size
                            if len(file_buffer) != expected_size:
                                log.error("  SIZE MISMATCH: got %d, expected %d",
                                          len(file_buffer), expected_size)
                                _send_commit(ser, config.COMMIT_FAIL)
                                transfer_ok = False
                            else:
                                log.info("  Size verified OK (%d bytes)", expected_size)

                                # Save raw backup
                                _save_raw_file(file_buffer)

                                # Parse CSV and store
                                rows = _parse_and_store_csv(file_buffer)

                                if rows > 0:
                                    _send_commit(ser, config.COMMIT_OK)
                                    transfer_ok = True
                                    log.info("══════════════════════════════════════")
                                    log.info("  TRANSFER COMPLETE  (%d GPS points)", rows)
                                    log.info("══════════════════════════════════════")
                                else:
                                    _send_commit(ser, config.COMMIT_FAIL)
                                    transfer_ok = False
                                    log.warning("  TRANSFER FAILED (0 valid CSV rows)")

                            # Update shared state
                            with _lock:
                                _transfer_active = False
                                _last_transfer_ok = transfer_ok
                                _total_transfers += 1

                            break   # back to outer loop, wait for next START

                        elif pkt_type2 == config.TYPE_START:
                            # ESP32 restarted transfer mid-stream.
                            # Could happen if it timed out and retried from scratch.
                            log.warning("  Got new START during transfer — restarting")
                            if len(payload2) >= 4:
                                expected_size = struct.unpack("<I", payload2[:4])[0]
                            file_buffer = bytearray()
                            _send_ack(ser)

                        else:
                            log.warning("  Unexpected packet type 0x%02X during transfer",
                                        pkt_type2)

                # ── Anything else while IDLE ──────────────────────
                else:
                    log.debug("Ignoring packet type 0x%02X (idle)", pkt_type)

        except serial.SerialException as e:
            log.error("Serial error: %s — retrying in 3s", e)
            with _lock:
                _transfer_active = False
            time.sleep(3)

        except Exception as e:
            log.error("Unexpected error: %s — retrying in 3s", e)
            with _lock:
                _transfer_active = False
            time.sleep(3)


# ── Public API ─────────────────────────────────────────────────────

def start(port=None, baud=None):
    """
    Start the UART receiver in a background daemon thread.
    Call from app.py before starting Flask.
    """
    port = port or config.SERIAL_PORT
    baud = baud or config.SERIAL_BAUD

    thread = threading.Thread(
        target=_receiver_loop,
        args=(port, baud),
        daemon=True,
        name="uart-receiver",
    )
    thread.start()
    log.info("UART receiver thread launched (%s @ %d)", port, baud)