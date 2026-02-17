"""
app.py

Watermelon Hub — Raspberry Pi web server.

Serves:
    /api/cuts       GET     All cut events as JSON (Chris's frontend polls this)
    /api/status     GET     ESP32 connection state and transfer status
    /api/export     GET     Download all cuts as CSV
    /api/clear      POST    Clear all cuts from database (for testing)
    /               GET     Serves index.html from static/

The UART receiver runs in a background thread, listening for data
from the ESP32 over the serial port. Received CSV data is parsed
and stored in SQLite. The Flask API reads from the same database.

Usage:
    python3 app.py
    python3 app.py --port 8080                  (custom web port)
    python3 app.py --serial /dev/ttyUSB0        (custom serial port)
    python3 app.py --no-serial                  (skip UART, for testing)
"""

import argparse
import logging
import io
import csv

from flask import Flask, jsonify, send_from_directory, Response

import config
import database
import uart_receiver

# ── Logging setup ──────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)-10s] %(levelname)s  %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("app")

# ── Flask app ──────────────────────────────────────────────────────

app = Flask(__name__, static_folder="static")


@app.route("/")
def index():
    """Serve Chris's frontend."""
    return send_from_directory(app.static_folder, "index.html")


@app.route("/<path:filename>")
def static_files(filename):
    """Serve any file from the static/ directory."""
    return send_from_directory(app.static_folder, filename)


# ── API endpoints ──────────────────────────────────────────────────

@app.route("/api/cuts", methods=["GET"])
def api_cuts():
    """
    Return all cut events as JSON.

    Response format (matches what Chris's app.js expects):
    [
        { "id": 1, "lat": 29.6516, "lon": -82.3248, "force": 45.2, "timestamp": 1700000000 },
        ...
    ]
    """
    cuts = database.get_all_cuts()
    return jsonify(cuts)


@app.route("/api/status", methods=["GET"])
def api_status():
    """
    Return current system status.
    {
        "shear_connected": true/false,
        "transfer_active": true/false,
        "buffer_bytes": 0,
        "total_cuts": 42
    }
    """
    status = uart_receiver.get_status()
    status["total_cuts"] = database.get_cut_count()
    return jsonify(status)


@app.route("/api/export", methods=["GET"])
def api_export():
    """Download all cuts as a CSV file."""
    cuts = database.get_all_cuts()

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["id", "timestamp", "latitude", "longitude", "force"])
    for cut in cuts:
        writer.writerow([
            cut["id"],
            cut["timestamp"],
            cut["lat"],
            cut["lon"],
            cut["force"],
        ])

    return Response(
        output.getvalue(),
        mimetype="text/csv",
        headers={"Content-Disposition": "attachment; filename=cuts_export.csv"},
    )


@app.route("/api/clear", methods=["POST"])
def api_clear():
    """Clear all cuts from the database. For testing only."""
    database.clear_all_cuts()
    return jsonify({"status": "ok", "message": "All cuts cleared"})


# ── Main entry point ──────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Watermelon Hub Pi Server")
    parser.add_argument("--port", type=int, default=config.WEB_PORT,
                        help="Web server port (default: %d)" % config.WEB_PORT)
    parser.add_argument("--serial", type=str, default=config.SERIAL_PORT,
                        help="Serial port (default: %s)" % config.SERIAL_PORT)
    parser.add_argument("--baud", type=int, default=config.SERIAL_BAUD,
                        help="Serial baud rate (default: %d)" % config.SERIAL_BAUD)
    parser.add_argument("--no-serial", action="store_true",
                        help="Skip UART receiver (for testing without ESP32)")
    args = parser.parse_args()

    # Initialize the database
    database.init_db()

    # Start UART receiver (unless --no-serial)
    if not args.no_serial:
        uart_receiver.start(port=args.serial, baud=args.baud)
    else:
        log.info("UART receiver disabled (--no-serial mode)")

    # Start Flask
    log.info("Starting web server on %s:%d", config.WEB_HOST, args.port)
    app.run(host=config.WEB_HOST, port=args.port, debug=False)


if __name__ == "__main__":
    main()