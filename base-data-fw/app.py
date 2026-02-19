"""
app.py

Watermelon Hub — Raspberry Pi web server.

Endpoints:
    GET  /               Serve Chris's frontend (static/index.html)
    GET  /api/points     All GPS points as JSON
    GET  /api/latest     Latest N points (default 50)
    GET  /api/status     Transfer state + point count
    GET  /api/export     Download all points as CSV
    POST /api/clear      Delete all points (testing only)

The UART receiver runs in a background daemon thread, listening for
file transfers from the ESP32. Received CSV data is parsed and stored
in SQLite. Flask reads from the same database.
"""

import argparse
import io
import csv
import logging

from flask import Flask, jsonify, Response, request

import config
import database
import uart_receiver

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
log = logging.getLogger("app")

app = Flask(__name__, static_folder="static", static_url_path="")


# ── Routes ─────────────────────────────────────────────────────────

@app.route("/")
def index():
    return app.send_static_file("index.html")


@app.route("/api/points", methods=["GET"])
def api_points():
    """All GPS points, oldest first."""
    points = database.get_all_points()
    return jsonify(points)


@app.route("/api/latest", methods=["GET"])
def api_latest():
    """Latest N points (default 50). Pass ?n=100 to change."""
    n = request.args.get("n", 50, type=int)
    points = database.get_latest_points(n)
    return jsonify(points)


@app.route("/api/status", methods=["GET"])
def api_status():
    """Transfer state and total point count."""
    status = uart_receiver.get_status()
    status["total_points"] = database.get_point_count()
    return jsonify(status)


@app.route("/api/export", methods=["GET"])
def api_export():
    """Download all GPS points as a CSV file."""
    points = database.get_all_points()

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow([
        "id", "utc_time", "latitude", "longitude",
        "fix_quality", "num_satellites", "hdop",
        "altitude", "geoid_height",
    ])
    for p in points:
        writer.writerow([
            p["id"], p["utc_time"], p["latitude"], p["longitude"],
            p["fix_quality"], p["num_satellites"], p["hdop"],
            p["altitude"], p["geoid_height"],
        ])

    return Response(
        output.getvalue(),
        mimetype="text/csv",
        headers={"Content-Disposition": "attachment; filename=gps_export.csv"},
    )


@app.route("/api/clear", methods=["POST"])
def api_clear():
    """Clear all GPS points. For testing only."""
    database.clear_all_points()
    return jsonify({"status": "ok", "message": "All points cleared"})


# ── Backwards-compatible aliases for Chris's existing frontend ─────
# His app.js might still poll /api/cuts — these map to the same data.

@app.route("/api/cuts", methods=["GET"])
def api_cuts_compat():
    """Alias for /api/points (backwards compat with Chris's frontend)."""
    return api_points()


# ── Entry point ────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Watermelon Hub Pi Server")
    parser.add_argument("--port", type=int, default=config.WEB_PORT,
                        help="Web server port (default: %d)" % config.WEB_PORT)
    parser.add_argument("--serial", type=str, default=config.SERIAL_PORT,
                        help="Serial port (default: %s)" % config.SERIAL_PORT)
    parser.add_argument("--baud", type=int, default=config.SERIAL_BAUD,
                        help="Baud rate (default: %d)" % config.SERIAL_BAUD)
    parser.add_argument("--no-serial", action="store_true",
                        help="Skip UART receiver (for testing without ESP32)")
    args = parser.parse_args()

    database.init_db()

    if not args.no_serial:
        uart_receiver.start(port=args.serial, baud=args.baud)
    else:
        log.info("UART receiver disabled (--no-serial mode)")

    log.info("Starting web server on %s:%d", config.WEB_HOST, args.port)
    app.run(host=config.WEB_HOST, port=args.port, debug=False)


if __name__ == "__main__":
    main()