"""
uart_receiver.py

Listens on the serial port for framed messages from the ESP32.

Frame format (mirrors uart_bridge.h):
    [0xAA] [type] [len_lo] [len_hi] [payload...] [checksum]

The ESP32 sends three message types:
    0x01  MSG_CUT_RECORD   - structured binary (not used with current shears)
    0x02  MSG_STATUS        - 1-byte status code (connect/disconnect/transfer)
    0x03  MSG_LOG_LINE      - raw CSV bytes from the shears GPS log file

For MSG_LOG_LINE, the receiver accumulates bytes into a buffer.
When MSG_STATUS(TRANSFER_DONE) arrives, the buffer is parsed as CSV
and each row is inserted into the SQLite database.

Runs as a background daemon thread so the Flask server can start
on the main thread.
"""

import serial
import struct
import threading
import logging
import io
import csv

import config
import database

log = logging.getLogger("uart_rx")

# ── Receiver state ─────────────────────────────────────────────────

_csv_buffer = bytearray()
_shear_connected = False
_transfer_active = False
_lock = threading.Lock()


def get_status():
    """Return current connection/transfer state for the API."""
    with _lock:
        return {
            "shear_connected": _shear_connected,
            "transfer_active": _transfer_active,
            "buffer_bytes": len(_csv_buffer),
        }


# ── CSV parsing ────────────────────────────────────────────────────

def _parse_and_store_csv(raw_bytes):
    """
    Parse the accumulated CSV bytes and insert each row into the database.

    Expected CSV format from the shears (gps_points.csv):
        sequence_id,timestamp,latitude,longitude,force,fix_type
        1,1700000000,29.6516,-82.3248,45.2,3
        2,1700000060,29.6518,-82.3250,38.7,3
        ...

    The first line may be a header row; skip it if non-numeric.
    """
    text = raw_bytes.decode("utf-8", errors="replace")
    reader = csv.reader(io.StringIO(text))

    inserted = 0
    for row in reader:
        # Skip empty lines
        if not row or len(row) < 5:
            continue

        # Skip header row
        try:
            float(row[2])
        except (ValueError, IndexError):
            log.debug("Skipping header/invalid row: %s", row)
            continue

        try:
            sequence_id = int(row[0])
            timestamp = int(row[1])
            latitude = float(row[2])
            longitude = float(row[3])
            force = float(row[4])
            fix_type = int(row[5]) if len(row) > 5 else 0

            database.insert_cut(sequence_id, timestamp,
                                latitude, longitude, force, fix_type)
            inserted += 1

        except (ValueError, IndexError) as e:
            log.warning("Failed to parse CSV row: %s — %s", row, e)

    log.info("Parsed CSV: %d rows inserted into database", inserted)
    return inserted


# ── Frame reader ───────────────────────────────────────────────────

def _read_frame(ser):
    """
    Read a single framed message from the serial port.
    Blocks until a valid frame is received.

    Returns (msg_type, payload) or (None, None) on error.
    """
    # Synchronize: scan for start byte
    while True:
        byte = ser.read(1)
        if len(byte) == 0:
            continue
        if byte[0] == config.FRAME_START:
            break

    # Read header: type (1 byte) + length (2 bytes little-endian)
    header = ser.read(3)
    if len(header) < 3:
        log.warning("Incomplete header (got %d bytes)", len(header))
        return None, None

    msg_type = header[0]
    payload_len = header[1] | (header[2] << 8)

    if payload_len > 200:
        log.warning("Payload length %d exceeds max, discarding", payload_len)
        return None, None

    # Read payload
    payload = ser.read(payload_len) if payload_len > 0 else b""
    if len(payload) < payload_len:
        log.warning("Incomplete payload (expected %d, got %d)",
                    payload_len, len(payload))
        return None, None

    # Read and verify checksum
    cksum_byte = ser.read(1)
    if len(cksum_byte) == 0:
        log.warning("Missing checksum byte")
        return None, None

    # Checksum is XOR of all bytes from type through payload
    computed = 0
    for b in header:
        computed ^= b
    for b in payload:
        computed ^= b

    if computed != cksum_byte[0]:
        log.warning("Checksum mismatch: computed=0x%02X received=0x%02X",
                    computed, cksum_byte[0])
        return None, None

    return msg_type, payload


# ── Message handlers ───────────────────────────────────────────────

def _handle_status(payload):
    """Handle a status message from the ESP32."""
    global _shear_connected, _transfer_active, _csv_buffer

    if len(payload) < 1:
        return

    code = payload[0]

    with _lock:
        if code == config.STATUS_SHEAR_CONNECTED:
            _shear_connected = True
            log.info("Shear connected")

        elif code == config.STATUS_SHEAR_DISCONNECTED:
            _shear_connected = False
            _transfer_active = False
            log.info("Shear disconnected")

        elif code == config.STATUS_TRANSFER_START:
            _transfer_active = True
            _csv_buffer = bytearray()
            log.info("Transfer started, buffer cleared")

        elif code == config.STATUS_TRANSFER_DONE:
            _transfer_active = False
            log.info("Transfer done, %d bytes received", len(_csv_buffer))
            # Parse the complete CSV and store in database
            if len(_csv_buffer) > 0:
                _parse_and_store_csv(bytes(_csv_buffer))
                _csv_buffer = bytearray()

        elif code == config.STATUS_TRANSFER_ERROR:
            _transfer_active = False
            _csv_buffer = bytearray()
            log.warning("Transfer error, buffer discarded")


def _handle_log_line(payload):
    """Handle raw CSV data from the ESP32."""
    with _lock:
        _csv_buffer.extend(payload)
    log.debug("Buffered %d bytes (total: %d)", len(payload), len(_csv_buffer))


# ── Main receiver loop ─────────────────────────────────────────────

def _receiver_loop(port, baud):
    """
    Main loop: open serial port, read frames, dispatch to handlers.
    Runs forever in a daemon thread.
    """
    log.info("Opening serial port %s @ %d baud", port, baud)

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
                msg_type, payload = _read_frame(ser)

                if msg_type is None:
                    continue

                if msg_type == config.MSG_STATUS:
                    _handle_status(payload)
                elif msg_type == config.MSG_LOG_LINE:
                    _handle_log_line(payload)
                elif msg_type == config.MSG_CUT_RECORD:
                    log.info("Received binary cut record (not implemented)")
                else:
                    log.warning("Unknown message type: 0x%02X", msg_type)

        except serial.SerialException as e:
            log.error("Serial error: %s — retrying in 3s", e)
            import time
            time.sleep(3)
        except Exception as e:
            log.error("Unexpected error: %s — retrying in 3s", e)
            import time
            time.sleep(3)


def start(port=None, baud=None):
    """
    Start the UART receiver in a background daemon thread.
    Call this from app.py before starting the Flask server.
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
    log.info("UART receiver thread started")