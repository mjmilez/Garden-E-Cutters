"""
database.py

SQLite database for storing watermelon cut events.

The schema matches the CutEvent struct from the ESP32 firmware.
CSV lines received from the ESP32 are parsed and inserted here.
The Flask API queries this database to serve /api/cuts.
"""

import sqlite3
import threading
import logging
from config import DB_PATH

log = logging.getLogger("database")


def get_connection():
    """
    Create a new SQLite connection for the current thread.
    SQLite connections can't be shared across threads,
    so each caller gets its own.
    """
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    """
    Create the cuts table if it doesn't exist.
    Called once at startup.
    """
    conn = get_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS cuts (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            sequence_id     INTEGER,
            timestamp       INTEGER,
            latitude        REAL,
            longitude       REAL,
            force           REAL,
            fix_type        INTEGER,
            received_at     DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()
    log.info("Database initialized: %s", DB_PATH)


def insert_cut(sequence_id, timestamp, latitude, longitude, force, fix_type=0):
    """
    Insert a single cut event into the database.
    Returns the row ID of the inserted record.
    """
    conn = get_connection()
    cursor = conn.execute(
        """
        INSERT INTO cuts (sequence_id, timestamp, latitude, longitude, force, fix_type)
        VALUES (?, ?, ?, ?, ?, ?)
        """,
        (sequence_id, timestamp, latitude, longitude, force, fix_type),
    )
    conn.commit()
    row_id = cursor.lastrowid
    conn.close()
    log.debug("Inserted cut: seq=%d lat=%.6f lon=%.6f force=%.2f",
              sequence_id, latitude, longitude, force)
    return row_id


def get_all_cuts():
    """
    Retrieve all cut events, ordered by timestamp descending.
    Returns a list of dicts matching the JSON format Chris's frontend expects:
        { id, lat, lon, force, timestamp }
    """
    conn = get_connection()
    rows = conn.execute(
        "SELECT sequence_id, timestamp, latitude, longitude, force "
        "FROM cuts ORDER BY timestamp DESC"
    ).fetchall()
    conn.close()

    return [
        {
            "id": row["sequence_id"],
            "timestamp": row["timestamp"],
            "lat": row["latitude"],
            "lon": row["longitude"],
            "force": row["force"],
        }
        for row in rows
    ]


def get_cut_count():
    """Return the total number of stored cuts."""
    conn = get_connection()
    count = conn.execute("SELECT COUNT(*) FROM cuts").fetchone()[0]
    conn.close()
    return count


def clear_all_cuts():
    """Delete all cut records. Useful for testing/reset."""
    conn = get_connection()
    conn.execute("DELETE FROM cuts")
    conn.commit()
    conn.close()
    log.info("All cuts cleared from database")