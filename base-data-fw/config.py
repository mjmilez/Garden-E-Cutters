"""
config.py

Shared configuration for the Watermelon Hub Raspberry Pi base station.
Adjust these values to match your hardware setup.
"""

# ── UART serial connection to ESP32 ────────────────────────────────
# On Raspberry Pi 4, the primary UART is /dev/ttyS0 (mini UART)
# or /dev/ttyAMA0 (PL011) depending on your config.
# If using a USB-to-serial adapter for testing, it will be /dev/ttyUSB0.
SERIAL_PORT = "/dev/ttyS0"
SERIAL_BAUD = 115200

# ── Frame protocol (must match uart_bridge.h on ESP32) ─────────────
FRAME_START = 0xAA

# Message types
MSG_CUT_RECORD = 0x01
MSG_STATUS     = 0x02
MSG_LOG_LINE   = 0x03

# Status codes
STATUS_SHEAR_CONNECTED    = 0x01
STATUS_SHEAR_DISCONNECTED = 0x02
STATUS_TRANSFER_START     = 0x03
STATUS_TRANSFER_DONE      = 0x04
STATUS_TRANSFER_ERROR     = 0x05

# ── Database ───────────────────────────────────────────────────────
DB_PATH = "hub_data.db"

# ── Web server ─────────────────────────────────────────────────────
WEB_HOST = "0.0.0.0"
WEB_PORT = 80