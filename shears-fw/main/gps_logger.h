#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GPS logging entry point for the shears.
 *
 * Once initialized, this module:
 *   - mounts SPIFFS and creates /spiffs/gps_points.csv with a header
 *   - configures UART2 to read NMEA sentences from the GPS
 *   - keeps track of the latest full NMEA line
 *   - exposes a "save" path driven by a button interrupt or API call
 *   - on save, parses $GPGGA and appends one CSV row
 */

/*
 * Bring up SPIFFS, UART2, the button ISR, and background tasks.
 *
 * Call once from app_main() during startup.
 */
void gpsLoggerInit(void);

/*
 * Request that the current GPS fix be written as a CSV row.
 *
 * This sets the same internal flag that the physical button interrupt uses.
 * The logger task will consume the flag and handle parsing and file I/O.
 */
void gpsLoggerRequestSave(void);

/*
 * Print the contents of /spiffs/gps_points.csv to the log.
 * Mainly useful for debugging over the serial console.
 */
void gpsLoggerPrintCsv(void);

#ifdef __cplusplus
}
#endif
