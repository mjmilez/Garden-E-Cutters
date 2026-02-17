/*
 * log_transfer_client.h
 *
 * Base-side interface for the log transfer protocol.
 *
 * This module handles requesting log files from the shears and
 * receiving control/data notifications over BLE. It is wired
 * up by the BLE layer once the log service and characteristics
 * have been discovered.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "log_transfer_protocol.h"

/*
 * Configuration used to bind the client to a specific BLE connection
 * and set of log-transfer characteristics.
 */
typedef struct {
	uint16_t connHandle;
	uint16_t ctrlChrHandle;
	uint16_t dataChrHandle;
} log_transfer_client_cfg_t;

/*
 * Initializes the log transfer client with the active connection
 * and characteristic handles.
 *
 * Typically called after GATT discovery completes.
 */
void log_transfer_client_init(const log_transfer_client_cfg_t *cfg);

/*
 * Updates the connection handle after a reconnect when the
 * characteristic handles remain valid.
 */
void log_transfer_client_set_conn_handle(uint16_t connHandle);

/*
 * Requests a log file from the shears.
 *
 * The filename must match the path expected by the shears-side
 * filesystem (e.g. "gps_log.csv", "/spiffs/gps_log.csv").
 */
esp_err_t log_transfer_client_request_file(const char *filename);

/*
 * Notification handlers used by the base BLE layer.
 *
 * CTRL notifications carry protocol status updates.
 * DATA notifications carry file payload chunks.
 */
void log_transfer_client_on_ctrl_notify(const uint8_t *data, uint16_t len);
void log_transfer_client_on_data_notify(const uint8_t *data, uint16_t len);