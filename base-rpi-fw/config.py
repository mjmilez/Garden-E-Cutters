"""
config.py

Shared configuration for the Watermelon Hub Raspberry Pi base station.

Protocol constants match uartFileTransfer.c on the ESP32.
Frame: [0xAA][TYPE][LEN][PAYLOAD...][XOR checksum]
Bidirectional: Pi sends ACK and COMMIT back to ESP32.
"""

import os

# ── Serial port ────────────────────────────────────────────────────
#
# Pi 4 hardware UART:
#   /dev/serial0   symlink to the primary UART (recommended)
#   /dev/ttyS0     mini UART (default on Pi 4 unless you swap)
#   /dev/ttyAMA0   PL011 (better, enable with dtoverlay in config.txt)
#
# For WSL/laptop testing with USB adapter:  /dev/ttyUSB0
# Override with env var:  HUB_SERIAL_PORT=/dev/ttyUSB0 python3 app.py
#
SERIAL_PORT = os.environ.get("HUB_SERIAL_PORT", "/dev/ttyAMA3")
SERIAL_BAUD = 115200

# ── UART packet types (must match uartFileTransfer.c) ──────────────
START_BYTE  = 0xAA

TYPE_START  = 0x01   # ESP32 → Pi : new file transfer, payload = fileSize (u32 LE)
TYPE_DATA   = 0x02   # ESP32 → Pi : file chunk, payload = raw bytes (1-255)
TYPE_END    = 0x03   # ESP32 → Pi : all chunks sent, no payload
TYPE_ACK    = 0x04   # Pi → ESP32 : acknowledgment, no payload
TYPE_COMMIT = 0x05   # Pi → ESP32 : verification result, payload = 1 byte status

MAX_PAYLOAD = 255    # CHUNK_SIZE in the C code

# COMMIT status codes
COMMIT_OK   = 0x00   # file received and verified
COMMIT_FAIL = 0x01   # verification failed (size mismatch, parse error, etc.)

# ── Database ───────────────────────────────────────────────────────
DB_PATH = os.environ.get("HUB_DB_PATH", "watermelon_hub.db")

# ── Web server ─────────────────────────────────────────────────────
WEB_HOST = "0.0.0.0"
WEB_PORT = int(os.environ.get("HUB_WEB_PORT", "80"))

# ── File storage ───────────────────────────────────────────────────
# Raw CSV backups saved here before parsing
RECEIVED_FILES_DIR = os.environ.get("HUB_FILES_DIR", "received_files")
