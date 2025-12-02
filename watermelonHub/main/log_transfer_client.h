#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "log_transfer_protocol.h"

/*
 * Configuration for the base-side log transfer client.
 *
 * This is basically the wiring info from the BLE layer:
 *   - connHandle    : active connection to WM-SHEARS
 *   - ctrlChrHandle : control characteristic for commands / status
 *   - dataChrHandle : data characteristic for file chunks
 */
typedef struct {
	uint16_t connHandle;
	uint16_t ctrlChrHandle;
	uint16_t dataChrHandle;
} log_transfer_client_cfg_t;

/*
 * Set up the client with the current connection + characteristic handles.
 * Call this once after you've discovered the log service on the base.
 */
void log_transfer_client_init(const log_transfer_client_cfg_t *cfg);

/*
 * If the link drops and reconnects but the characteristics stay the same,
 * you can just patch in the new connHandle here.
 */
void log_transfer_client_set_conn_handle(uint16_t connHandle);

/*
 * Kick off a file transfer from the shears.
 *
 * 'filename' is whatever the shears side uses with fopen()
 * (e.g. "gps_log.csv", "/spiffs/gps_log.csv", etc.).
 */
esp_err_t log_transfer_client_request_file(const char *filename);

/*
 * These are called from the base's BLE notification path.
 *
 * In base_ble.c:
 *   - route notifications from the log CTRL characteristic here
 *   - route notifications from the log DATA characteristic here
 */
void log_transfer_client_on_ctrl_notify(const uint8_t *data, uint16_t len);
void log_transfer_client_on_data_notify(const uint8_t *data, uint16_t len);
