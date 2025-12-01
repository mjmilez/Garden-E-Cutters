#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Log transfer GATT server for the shears.
 *
 * This module:
 *   - registers the custom log service (0xFFF0) with control and data chars
 *   - handles START_TRANSFER and ABORT commands from the base
 *   - streams a file from /spiffs over notifications on the data characteristic
 *   - sends STATUS_* events back on the control characteristic
 *
 * It does not expose any configuration knobs to the rest of the shears
 * firmware; the only public entry point is the init function below.
 */

/*
 * Register the log transfer GATT service and start the background transfer task.
 *
 * Call once during BLE bring-up on the shears side, after NimBLE is initialized
 * and before advertising starts.
 */
void log_transfer_server_init(void);

#ifdef __cplusplus
}
#endif
