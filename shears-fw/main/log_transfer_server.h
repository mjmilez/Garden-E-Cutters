/*
 * log_transfer_server.h
 *
 * GATT server interface for BLE log transfer on the shears.
 *
 * Registers the custom service and exposes helpers for connection state
 * and transfer control.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Registers the log transfer service and starts the background task. */
void log_transfer_server_init(void);

/* Updates the active BLE connection used for transfer notifications. */
void log_transfer_server_setConnection(uint16_t conn_handle);

/* Clears the active BLE connection. */
void log_transfer_server_clearConnection(void);

/* Returns true when a BLE connection is available for transfer. */
bool log_transfer_server_isConnected(void);

/* Returns true while a file transfer is in progress. */
bool log_transfer_server_isTransferActive(void);

/* Starts a transfer for the given filename basename. */
bool log_transfer_server_startTransfer(const char *filename);

/* Aborts the active transfer, if one is running. */
void log_transfer_server_abortTransfer(void);

#ifdef __cplusplus
}
#endif