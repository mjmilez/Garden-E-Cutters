"""
database.py

SQLite wrapper for storing GPS cut/point records.

Schema matches the actual CSV output from shears firmware:
    utc_time, latitude, longitude, fix_quality, num_satellites, hdop, altitude, geoid_height

Soft-delete support:
    - deleted_at column: NULL = active, timestamp = soft-deleted
    - Points with deleted_at set are hidden from normal queries
    - purge_expired_deleted() removes soft-deleted points older than 48 hours
    
The Flask API reads from this database to serve /api/cuts for frontend.
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
            utc_date        TEXT    NOT NULL DEFAULT '0000-00-00',
            utc_time        TEXT    NOT NULL,
            latitude        REAL    NOT NULL,
            longitude       REAL    NOT NULL,
            fix_quality     INTEGER NOT NULL DEFAULT 0,
            num_satellites  INTEGER NOT NULL DEFAULT 0,
            hdop            REAL    NOT NULL DEFAULT 0.0,
            altitude        REAL    NOT NULL DEFAULT 0.0,
            geoid_height    REAL    NOT NULL DEFAULT 0.0,
            received_at     DATETIME DEFAULT CURRENT_TIMESTAMP,
            deleted_at      TEXT    DEFAULT NULL
        )
    """)

    # ── Migration: add utc_date if upgrading from older schema ───
    cursor = conn.execute("PRAGMA table_info(gps_points)")
    columns = [row["name"] for row in cursor.fetchall()]
    if "utc_date" not in columns:
        conn.execute("ALTER TABLE gps_points ADD COLUMN utc_date TEXT NOT NULL DEFAULT '0000-00-00'")
        log.info("Migrated gps_points table: added utc_date column")

    # ── Migration: add deleted_at if upgrading from older schema ───
    cursor = conn.execute("PRAGMA table_info(gps_points)")
    columns = [row["name"] for row in cursor.fetchall()]
    if "deleted_at" not in columns:
        conn.execute("ALTER TABLE gps_points ADD COLUMN deleted_at TEXT DEFAULT NULL")
        log.info("Migrated gps_points table: added deleted_at column")

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
           (utc_date, utc_time, latitude, longitude, fix_quality,
            num_satellites, hdop, altitude, geoid_height)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        [
            (
                r["utc_date"],
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
    """Return all active GPS points as a list of dicts."""
    conn = get_connection()
    rows = conn.execute(
        """SELECT id, utc_date, utc_time, latitude, longitude, fix_quality,
                  num_satellites, hdop, altitude, geoid_height
           FROM gps_points
           WHERE deleted_at IS NULL
           ORDER BY id"""
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def get_point_count():
    """Return the total count of active GPS points."""
    conn = get_connection()
    count = conn.execute(
        "SELECT COUNT(*) FROM gps_points WHERE deleted_at IS NULL"
    ).fetchone()[0]
    conn.close()
    return count


def get_latest_points(n=50):
    """Return the N most recent points (newest first)."""
    conn = get_connection()
    rows = conn.execute(
        """SELECT id, utc_date, utc_time, latitude, longitude, fix_quality,
                  num_satellites, hdop, altitude, geoid_height
           FROM gps_points
           WHERE deleted_at IS NULL
           ORDER BY id DESC LIMIT ?""",
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
    
def soft_delete_points(ids):
    """_summary_

    Args:
        ids (_type_): _description_
    """
    if not ids:
        return 0

    conn = get_connection()
    placeholders = ",".join("?" for _ in ids)
    cursor = conn.execute(
        f"UPDATE gps_points SET deleted_at = CURRENT_TIMESTAMP "
        f"WHERE id IN ({placeholders}) AND deleted_at IS NULL",
        ids,
    )
    conn.commit()
    affected = cursor.rowcount
    conn.close()
    log.info("Soft-deleted %d points with IDs: %s", affected, ids)
    return affected

def restore_points(ids):
    """
    Restore soft-deleted points by clearing deleted_at.
    ids: list of point IDs to restore.
    Returns the number of rows affected.
    """
    if not ids:
        return 0

    conn = get_connection()
    placeholders = ",".join("?" for _ in ids)
    cursor = conn.execute(
        f"UPDATE gps_points SET deleted_at = NULL "
        f"WHERE id IN ({placeholders}) AND deleted_at IS NOT NULL",
        ids,
    )
    conn.commit()
    affected = cursor.rowcount
    conn.close()
    log.info("Restored %d points with IDs: %s", affected, ids)
    return affected

def get_deleted_points():
    """Return all soft-deleted points (the 'trash' list)."""
    conn = get_connection()
    rows = conn.execute(
        """SELECT id, utc_date, utc_time, latitude, longitude, fix_quality,
                  num_satellites, hdop, altitude, geoid_height, deleted_at
           FROM gps_points
           WHERE deleted_at IS NOT NULL
           ORDER BY deleted_at DESC"""
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]

def purge_expired_deleted(hours=48):
    """
    Permanently delete soft-deleted points older than expire_hours.
    Called periodically to clean up the database in app.py.
    Returns the number of rows permanently deleted."""
    conn = get_connection()
    cursor = conn.execute(
        """DELETE FROM gps_points
           WHERE deleted_at IS NOT NULL
             AND deleted_at < datetime('now', ? || ' hours')""",
        (f"{hours} hours"),
    )
    conn.commit()
    purged = cursor.rowcount
    conn.close()
    if purged > 0:
        log.info("Purged %d expired soft-deleted point(s) (older than %d hours)", purged, hours)
    return purged

def insert_cut(lat, lng, timestamp=None, hdop=None, utc_date=None):
    """
    Insert a single cut record for dev/testing.
    - utc_time defaults to "0" if not provided
    - fix_quality, num_satellites, geoid_height default to 0
    - altitude defaults to 73.0
    - hdop can be provided, otherwise defaults to 0.0
    Returns the new row id.
    """
    if timestamp is None:
        timestamp = "0"
    if hdop is None:
        hdop = 0.0
    if utc_date is None:
        utc_date = "0000-00-00"
        
    altitude = 73.0

    conn = get_connection()
    cursor = conn.execute(
        """INSERT INTO gps_points
           (utc_date, utc_time, latitude, longitude, fix_quality,
            num_satellites, hdop, altitude, geoid_height)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        (utc_date, timestamp, lat, lng, 0, 0, hdop, altitude, 0.0)
    )
    conn.commit()
    new_id = cursor.lastrowid
    conn.close()
    return new_id