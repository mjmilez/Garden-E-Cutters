"""
app.py

Watermelon Hub — Raspberry Pi web server.

Endpoints:
    GET  /                      Serve Chris's frontend (static/index.html)
    GET  /api/points            All GPS points as JSON
    GET  /api/latest            Latest N points (default 50)
    GET  /api/status            Transfer state + point count
    GET  /api/export            Download all points as CSV
    POST /api/points/delete     Soft-delete points by ID list
    POST /api/points/restore    Restore soft-deleted points by ID list
    GET  /api/points/deleted    List of soft-deleted points (trash)
    POST /api/clear             Delete all points (testing only)
    POST /api/cuts              Dev endpoint for inserting cuts
    
Background tasks:
    - UART receiver thread: Listens for incoming CSV data from ESP32, parses it, and stores in SQLite.
    - Purge thread: Periodically deletes points older than a certain age (48 hours) to prevent unbounded growth.

The UART receiver runs in a background daemon thread, listening for
file transfers from the ESP32. Received CSV data is parsed and stored
in SQLite. Flask reads from the same database.
"""

import argparse
import io
import csv
import logging
import threading
import time

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

# ── Background purge thread ────────────────────────────────────────

PURGE_INTERVAL_SECONDS = 3600   # Run purge every hour
PURGE_AFTER_HOURS = 48          # Purge points older than this (48 hours)

def _purge_loop():
    """Periodically purge expired soft-deleted points."""
    while True:
        time.sleep(PURGE_INTERVAL_SECONDS)
        try:
            purged = database.purge_expired_deleted(hours=PURGE_AFTER_HOURS)
            if purged > 0:
                log.info("Purge cycle: removed %d expired point(s)", purged)
        except Exception as e:
            log.error("Purge cycle error: %s", e)


def start_purge_thread():
    """Launch the background purge thread as a daemon."""
    thread = threading.Thread(
        target=_purge_loop,
        daemon=True,
        name="purge-expired",
    )
    thread.start()
    log.info("Purge thread started (every %ds, threshold %dh)",
             PURGE_INTERVAL_SECONDS, PURGE_AFTER_HOURS)
    

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
    """Download all active GPS points as a CSV file."""
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

@app.route("/api/points/delete", methods=["POST"])
def api_delete_points():
    """
    Soft-delete one or more points.
    
    Request body (JSON):
    { "ids": [1, 3, 5]  # List of point IDs to soft-delete}
    
    Response:
    { "status": "ok", "deleted": "3" }
    """
    data = request.get_json(silent=True)
    if not data or "ids" not in data:
        return jsonify({"status": "error", "message": "Missing 'ids' in request body"}), 400
    
    ids = data["ids"]
    if not isinstance(ids, list) or not all(isinstance(i, int) for i in ids):
        return jsonify({"status": "error", "message": "'ids' must be a list of integers"}), 400
    
    if len(ids) == 0:
        return jsonify({"status": "ok", "deleted": "0"})
    
    affected = database.soft_delete_points(ids)
    return jsonify({"status": "ok", "deleted": affected})

@app.route("/api/points/restore", methods=["POST"])
def api_restore_points():
    """
    Restore one or more soft-deleted points.
    
    Request body (JSON):
    { "ids": [1, 3, 5]  # List of point IDs to restore}
    
    Response:
    { "status": "ok", "restored": "3" }
    """
    data = request.get_json(silent=True)
    if not data or "ids" not in data:
        return jsonify({"status": "error", "message": "Missing 'ids' in request body"}), 400
    
    ids = data["ids"]
    if not isinstance(ids, list) or not all(isinstance(i, int) for i in ids):
        return jsonify({"status": "error", "message": "'ids' must be a list of integers"}), 400
    
    if len(ids) == 0:
        return jsonify({"status": "ok", "restored": "0"})
    
    affected = database.restore_points(ids)
    return jsonify({"status": "ok", "restored": affected})

@app.route("/api/points/deleted", methods=["GET"])
def api_deleted_points():
    """
    List of soft-deleted points (the "trash").
    
    Response includes the deleted_at timestamp 
    to help identify old points that may need purging.
    """
    deleted = database.get_deleted_points()
    return jsonify(deleted)

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

@app.route("/api/cuts", methods=["POST"])
def add_cut():
    data = request.get_json()
    if not data:
        return jsonify({"error": "No JSON body provided"}), 400

    lat = data.get('lat')
    lng = data.get('lng')
    timestamp = data.get('timestamp')  # optional

    if lat is None or lng is None:
        return jsonify({"error": "lat and lng are required"}), 400

    try:
        new_id = db.insert_cut(float(lat), float(lng), timestamp)
        return jsonify({"success": True, "id": new_id}), 201
    except Exception as e:
        return jsonify({"error": str(e)}), 500


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
        
    start_purge_thread()

    log.info("Starting web server on %s:%d", config.WEB_HOST, args.port)
    app.run(host=config.WEB_HOST, port=args.port, debug=False)


if __name__ == "__main__":
    main()