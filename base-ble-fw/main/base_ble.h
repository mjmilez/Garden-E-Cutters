/*
 * base_ble.h
 *
 * Base-side BLE interface.
 *
 * Exposes a small public API for bringing up the BLE central and requesting
 * log files from WM-SHEARS. Connection and discovery details stay internal
 * to base_ble.c.
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

/*
 * Connection state callback used by the application for simple status updates.
 */
typedef void (*bleBaseConnCallback_t)(bool connected);

/*
 * Initializes BLE on the base and starts scanning for WM-SHEARS.
 */
void bleBaseInit(bleBaseConnCallback_t cb);

/*
 * Requests a log file from the shears over the log-transfer service.
 *
 * The filename must match the path expected by the shears-side filesystem
 * (e.g. "/spiffs/gps_log.csv", "session_0001.csv").
 *
 * If GATT discovery has not completed yet, the request is queued and sent
 * once the service and characteristics are ready.
 */
esp_err_t bleBaseRequestLog(const char *filename);