"""
database.py

SQLite wrapper for storing GPS cut/point records.

Schema matches the actual CSV output from Josh's shears firmware:
    utc_time, latitude, longitude, fix_quality, num_satellites, hdop, altitude, geoid_height

The Flask API reads from this database to serve /api/cuts for Chris's frontend.
"""

import sqlite3
import logging

import config

log = logging.getLogger("database")


def get_connection():
    """
    Create a new SQLite connection for the current thread.
    SQLite connections can't be shared across threads, so each caller
    gets its own.
    """
    conn = sqlite3.connect(config.DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    """Create the gps_points table if it doesn't exist."""
    conn = get_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS gps_points (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            utc_time        TEXT    NOT NULL,
            latitude        REAL    NOT NULL,
            longitude       REAL    NOT NULL,
            fix_quality     INTEGER NOT NULL DEFAULT 0,
            num_satellites  INTEGER NOT NULL DEFAULT 0,
            hdop            REAL    NOT NULL DEFAULT 0.0,
            altitude        REAL    NOT NULL DEFAULT 0.0,
            geoid_height    REAL    NOT NULL DEFAULT 0.0,
            received_at     DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()
    log.info("Database initialized: %s", config.DB_PATH)


def insert_point(record):
    """
    Insert one GPS point record.

    record: dict with keys matching CSV columns:
        utc_time, latitude, longitude, fix_quality,
        num_satellites, hdop, altitude, geoid_height
    """
    conn = get_connection()
    conn.execute(
        """INSERT INTO gps_points
           (utc_time, latitude, longitude, fix_quality,
            num_satellites, hdop, altitude, geoid_height)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
        (
            record["utc_time"],
            record["latitude"],
            record["longitude"],
            record["fix_quality"],
            record["num_satellites"],
            record["hdop"],
            record["altitude"],
            record["geoid_height"],
        ),
    )
    conn.commit()
    conn.close()


def insert_points_batch(records):
    """
    Insert multiple GPS points in a single transaction.
    Much faster than calling insert_point() in a loop.
    """
    if not records:
        return 0

    conn = get_connection()
    conn.executemany(
        """INSERT INTO gps_points
           (utc_time, latitude, longitude, fix_quality,
            num_satellites, hdop, altitude, geoid_height)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
        [
            (
                r["utc_time"],
                r["latitude"],
                r["longitude"],
                r["fix_quality"],
                r["num_satellites"],
                r["hdop"],
                r["altitude"],
                r["geoid_height"],
            )
            for r in records
        ],
    )
    conn.commit()
    count = len(records)
    conn.close()
    return count


def get_all_points():
    """Return all GPS points as a list of dicts."""
    conn = get_connection()
    rows = conn.execute(
        """SELECT id, utc_time, latitude, longitude, fix_quality,
                  num_satellites, hdop, altitude, geoid_height
           FROM gps_points ORDER BY id"""
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def get_point_count():
    conn = get_connection()
    count = conn.execute("SELECT COUNT(*) FROM gps_points").fetchone()[0]
    conn.close()
    return count


def get_latest_points(n=50):
    """Return the N most recent points (newest first)."""
    conn = get_connection()
    rows = conn.execute(
        """SELECT id, utc_time, latitude, longitude, fix_quality,
                  num_satellites, hdop, altitude, geoid_height
           FROM gps_points ORDER BY id DESC LIMIT ?""",
        (n,),
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def clear_all_points():
    """Delete all records. For testing/reset."""
    conn = get_connection()
    conn.execute("DELETE FROM gps_points")
    conn.commit()
    conn.close()
    log.info("All GPS points cleared from database")