/*
 * gps_logger.h
 *
 * GPS logging interface for the shears firmware.
 *
 * This module reads NMEA sentences from UART2 and appends $GPGGA fixes to
 * /spiffs/gps_points.csv. Save requests can come from a GPIO button interrupt
 * or from gpsLoggerRequestSave().
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* --- Public API ----------------------------------------------------------- */

/* Initializes SPIFFS, UART2, button interrupt, and background tasks. */
void gpsLoggerInit(void);

/* Triggers a save using the same internal path as the button press. */
void gpsLoggerRequestSave(void);

/* Prints /spiffs/gps_points.csv to the log (debug helper). */
void gpsLoggerPrintCsv(void);

#ifdef __cplusplus
}
#endif
