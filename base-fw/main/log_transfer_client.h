#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "log_transfer_protocol.h"

/*
 * Configuration for the base-side log transfer client.
 *
 * Contains BLE wiring information:
 *   - connHandle    : active connection to WM-SHEARS
 *   - ctrlChrHandle : control characteristic (commands / status)
 *   - dataChrHandle : data characteristic (file chunks)
 */
typedef struct {
	uint16_t connHandle;
	uint16_t ctrlChrHandle;
	uint16_t dataChrHandle;
} log_transfer_client_cfg_t;

/*
 * Initializes the log transfer client with the current connection
 * and characteristic handles.
 *
 * Intended to be called after the log service has been discovered.
 */
void log_transfer_client_init(const log_transfer_client_cfg_t *cfg);

/*
 * Updates the connection handle after a reconnect when
 * characteristic handles remain unchanged.
 */
void log_transfer_client_set_conn_handle(uint16_t connHandle);

/*
 * Requests a file transfer from the shears.
 *
 * 'filename' must match the path expected by the shears-side filesystem
 * (e.g. "gps_log.csv", "/spiffs/gps_log.csv").
 */
esp_err_t log_transfer_client_request_file(const char *filename);

/*
 * Notification handlers invoked from the base BLE layer.
 *
 * In base_ble.c:
 *   - CTRL characteristic notifications are routed here
 *   - DATA characteristic notifications are routed here
 */
void log_transfer_client_on_ctrl_notify(const uint8_t *data, uint16_t len);
void log_transfer_client_on_data_notify(const uint8_t *data, uint16_t len);