#!/usr/bin/env python3
"""
download_tiles.py — Download OSM tiles for offline use with Garden-E-Cutters.

Downloads map tiles for a given bounding box and zoom range, then stores them
in a single .mbtiles (SQLite) file that Flask can serve locally.

Usage (on the Pi, connected to a hotspot with internet):

    python3 download_tiles.py

The output file (uf_campus.mbtiles) will be created in the same directory.
Point your Flask app at this file to serve tiles offline.

Tile source: OpenStreetMap (please respect their tile usage policy).
This script throttles requests to ~1 per 100ms to be a good citizen.
"""

import math
import os
import sqlite3
import sys
import time
import urllib.request

# ── Configuration ──────────────────────────────────────────────────

# UF campus bounding box (with a small buffer around the main campus)
# Covers roughly from SW 34th St to E University Ave, and from Archer Rd to NW 8th Ave
BBOX = {
    "min_lat": 29.6350,
    "max_lat": 29.6550,
    "min_lon": -82.3700,
    "max_lon": -82.3350,
}

# Zoom levels to download (OSM max native zoom is 19)
MIN_ZOOM = 15
MAX_ZOOM = 19

# Output file
OUTPUT_FILE = "uf_campus.mbtiles"

# Tile server URL template
TILE_URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"

# Be a good citizen: identify ourselves and throttle requests
USER_AGENT = "GardenECutters-TileDownloader/1.0 (UF Senior Design Project)"
REQUEST_DELAY = 0.1  # seconds between requests

# ── Tile math ──────────────────────────────────────────────────────

def lat_lon_to_tile(lat, lon, zoom):
    """Convert lat/lon to tile x/y at a given zoom level."""
    n = 2 ** zoom
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    # Clamp to valid range
    x = max(0, min(n - 1, x))
    y = max(0, min(n - 1, y))
    return x, y


def count_tiles(bbox, min_zoom, max_zoom):
    """Count total tiles in the bounding box across all zoom levels."""
    total = 0
    for z in range(min_zoom, max_zoom + 1):
        x_min, y_min = lat_lon_to_tile(bbox["max_lat"], bbox["min_lon"], z)
        x_max, y_max = lat_lon_to_tile(bbox["min_lat"], bbox["max_lon"], z)
        total += (x_max - x_min + 1) * (y_max - y_min + 1)
    return total


def flip_y(y, zoom):
    """Convert OSM y (top-left origin) to TMS y (bottom-left origin) for MBTiles spec."""
    return (2 ** zoom) - 1 - y

# ── MBTiles setup ─────────────────────────────────────────────────

def create_mbtiles(filepath):
    """Create a new MBTiles database with the required schema."""
    if os.path.exists(filepath):
        os.remove(filepath)

    conn = sqlite3.connect(filepath)
    c = conn.cursor()

    c.execute("""
        CREATE TABLE metadata (
            name  TEXT,
            value TEXT
        )
    """)

    c.execute("""
        CREATE TABLE tiles (
            zoom_level  INTEGER,
            tile_column INTEGER,
            tile_row    INTEGER,
            tile_data   BLOB
        )
    """)

    c.execute("""
        CREATE UNIQUE INDEX idx_tiles
        ON tiles (zoom_level, tile_column, tile_row)
    """)

    # MBTiles metadata
    metadata = [
        ("name",        "UF Campus - Garden-E-Cutters"),
        ("type",        "baselayer"),
        ("version",     "1"),
        ("description", "OSM tiles for UF campus area, zoom 15-19"),
        ("format",      "png"),
        ("bounds",      f"{BBOX['min_lon']},{BBOX['min_lat']},{BBOX['max_lon']},{BBOX['max_lat']}"),
        ("minzoom",     str(MIN_ZOOM)),
        ("maxzoom",     str(MAX_ZOOM)),
    ]
    c.executemany("INSERT INTO metadata (name, value) VALUES (?, ?)", metadata)

    conn.commit()
    return conn

# ── Download ──────────────────────────────────────────────────────

def download_tile(z, x, y):
    """Download a single tile from OSM. Returns PNG bytes or None on failure."""
    url = TILE_URL.format(z=z, x=x, y=y)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})

    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return resp.read()
    except Exception as e:
        print(f"  WARN: Failed to download z={z} x={x} y={y}: {e}")
        return None


def main():
    total = count_tiles(BBOX, MIN_ZOOM, MAX_ZOOM)
    print(f"Garden-E-Cutters Tile Downloader")
    print(f"================================")
    print(f"Area:   UF Campus ({BBOX['min_lat']:.4f},{BBOX['min_lon']:.4f}) to ({BBOX['max_lat']:.4f},{BBOX['max_lon']:.4f})")
    print(f"Zooms:  {MIN_ZOOM} – {MAX_ZOOM}")
    print(f"Tiles:  {total}")
    print(f"Output: {OUTPUT_FILE}")
    print(f"Est. time: ~{int(total * REQUEST_DELAY / 60) + 1} minutes")
    print()

    conn = create_mbtiles(OUTPUT_FILE)
    cursor = conn.cursor()

    downloaded = 0
    failed = 0
    start_time = time.time()

    for z in range(MIN_ZOOM, MAX_ZOOM + 1):
        x_min, y_min = lat_lon_to_tile(BBOX["max_lat"], BBOX["min_lon"], z)
        x_max, y_max = lat_lon_to_tile(BBOX["min_lat"], BBOX["max_lon"], z)

        zoom_tiles = (x_max - x_min + 1) * (y_max - y_min + 1)
        print(f"Zoom {z}: {zoom_tiles} tiles (x: {x_min}-{x_max}, y: {y_min}-{y_max})")

        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                tile_data = download_tile(z, x, y)

                if tile_data:
                    # MBTiles uses TMS y-axis (flipped from OSM)
                    tms_y = flip_y(y, z)
                    cursor.execute(
                        "INSERT OR REPLACE INTO tiles (zoom_level, tile_column, tile_row, tile_data) VALUES (?, ?, ?, ?)",
                        (z, x, tms_y, tile_data)
                    )
                    downloaded += 1
                else:
                    failed += 1

                # Progress
                done = downloaded + failed
                pct = (done / total) * 100 if total else 0
                sys.stdout.write(f"\r  Progress: {done}/{total} ({pct:.1f}%) — {downloaded} ok, {failed} failed")
                sys.stdout.flush()

                time.sleep(REQUEST_DELAY)

        # Commit after each zoom level
        conn.commit()
        print()  # newline after progress bar

    conn.close()

    elapsed = time.time() - start_time
    file_size_mb = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)

    print()
    print(f"Done!")
    print(f"  Downloaded: {downloaded} tiles")
    print(f"  Failed:     {failed} tiles")
    print(f"  File size:  {file_size_mb:.1f} MB")
    print(f"  Time:       {elapsed:.0f} seconds")
    print()
    print(f"Next steps:")
    print(f"  1. Move {OUTPUT_FILE} to your project directory (next to app.py)")
    print(f"  2. Add the /tiles/ route to app.py")
    print(f"  3. Set USE_ONLINE_TILES = false in app.js")


if __name__ == "__main__":
    main()
