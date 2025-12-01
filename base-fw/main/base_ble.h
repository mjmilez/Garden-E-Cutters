#pragma once

#include <stdbool.h>
#include "esp_err.h"

/*
 * App-level notification for base BLE link status.
 *
 * This fires whenever our connection to WM-SHEARS comes up or goes down:
 *   connected = true  → we have an active link to the shears
 *   connected = false → no link; we are (or will be) scanning
 *
 * Typical use: update a status LED or let the UI know that log requests
 * can be made.
 */
typedef void (*bleBaseConnCallback_t)(bool connected);

/*
 * Bring up NimBLE on the base and start scanning for "WM-SHEARS".
 *
 * This should be called once from app_main(). All GAP/GATT events are
 * handled inside base_ble.c; the app just gets high-level connect/disconnect
 * via the callback.
 */
void bleBaseInit(bleBaseConnCallback_t cb);

/*
 * Ask the shears to send a log file over the custom log-transfer service.
 *
 * The filename should be whatever the shears side passes into fopen(),
 * e.g. "/spiffs/gps_log.csv" or "session_0001.csv".
 *
 * If the log service has not finished GATT discovery yet, the request is
 * queued and sent once discovery completes.
 */
esp_err_t bleBaseRequestLog(const char *filename);
