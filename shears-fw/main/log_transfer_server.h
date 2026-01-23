/*
 * log_transfer_server.h
 *
 * GATT server interface for BLE log transfer on the shears.
 *
 * Registers the custom log transfer service and exposes a single
 * initialization entry point. All protocol handling, file I/O,
 * and data streaming are internal to the server implementation.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* --- Public API ----------------------------------------------------------- */

/* Registers the log transfer service and starts the background transfer task. */
void log_transfer_server_init(void);

#ifdef __cplusplus
}
#endif
