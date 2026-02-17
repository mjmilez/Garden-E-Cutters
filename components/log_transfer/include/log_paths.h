/*
 * log_paths.h
 *
 * Shared log file naming conventions used by both the base and the shears.
 *
 * The shears writes CSV logs under /spiffs/, and the base mirrors that layout
 * so incoming data can be written directly to the same path.
 *
 * Only the basename is sent over BLE during a START_TRANSFER request.
 * It must remain short enough to fit within a single control write.
 */

#pragma once

/* Full filesystem path used when opening the log file locally. */
#define GPS_LOG_FILE_PATH      "/spiffs/gps_points.csv"

/* Basename sent over BLE during log transfer requests. */
#define GPS_LOG_FILE_BASENAME  "gps_points.csv"