#pragma once

/*
 * File path conventions shared between the base firmware and the shears.
 *
 * The shears stores its CSV logs under /spiffs/, and the base expects to
 * create its own copy using the same naming so the log transfer client can
 * write incoming data directly to this path.
 *
 * The basename is what actually gets sent over BLE during a START_TRANSFER
 * request. It must remain short enough to fit inside the MTU-limited write
 * to the control characteristic.
 */

/* Full path used by fopen() on both devices */
#define GPS_LOG_FILE_PATH      "/spiffs/gps_points.csv"

/* Short filename sent over BLE (small enough for a single control write) */
#define GPS_LOG_FILE_BASENAME  "gps_points.csv"
