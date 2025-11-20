/**
 * @file gps_logger.h
 * @brief GPS NMEA logging module for the shears firmware.
 *
 * This module:
 *  - Mounts SPIFFS and ensures gps_points.csv exists with a header
 *  - Configures UART2 to read NMEA sentences from a GPS module
 *  - Spawns a task to assemble full NMEA lines and track the latest one
 *  - Installs a button ISR on GPIO 23 to request a "save point"
 *  - Spawns a logger task that, when asked, parses the latest $GPGGA
 *    and appends it as a CSV row to /spiffs/gps_points.csv
 *
 * The app can also trigger a save in software (e.g. via BLE) with
 * gpsLoggerRequestSave(), not just the physical button.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPIFFS, UART2, tasks, and the save button ISR.
 *
 * Call this once from app_main() after FreeRTOS is running.
 */
void gpsLoggerInit(void);

/**
 * @brief Request that the current GPS fix be saved to CSV.
 *
 * This sets the same internal flag that the physical button ISR sets.
 * A background task will handle parsing and writing to the file.
 */
void gpsLoggerRequestSave(void);

/**
 * @brief Print the contents of /spiffs/gps_points.csv to the log.
 *
 * Useful for debugging or serial inspection.
 */
void gpsLoggerPrintCsv(void);

#ifdef __cplusplus
}
#endif
