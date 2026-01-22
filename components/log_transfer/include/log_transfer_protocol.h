/*
 * log_transfer_protocol.h
 *
 * Shared protocol definitions for BLE log transfer.
 *
 * Included by both the base and shears firmware so control opcodes, status
 * codes, and packet layouts stay aligned. This header defines only the
 * protocol surface (enums / structs); implementation lives in each side’s
 * client/server modules.
 */

#pragma once

#include <stdint.h>

/* --- Control opcodes (base → shears) ------------------------------------- */

typedef enum {
	CTRL_CMD_NOP            = 0x00,

	/*
	 * Requests a file transfer from the shears.
	 *
	 * Control write payload:
	 *   [0]     CTRL_CMD_START_TRANSFER
	 *   [1..N]  Null-terminated ASCII filename (usually a basename)
	 *
	 * The shears side resolves the provided name into a filesystem path
	 * before opening the file.
	 */
	CTRL_CMD_START_TRANSFER = 0x01,

	/*
	 * Aborts an active transfer.
	 *
	 * The shears side stops sending chunks and reports STATUS_TRANSFER_ABORTED.
	 */
	CTRL_CMD_ABORT          = 0x02,

	/* Control events sent back from the shears. */
	CTRL_EVT_STATUS         = 0x80
} ctrl_opcode_t;

/* --- Status / event codes (shears → base) -------------------------------- */

typedef enum {
	STATUS_OK               = 0x00,   /* Request accepted; size follows */
	STATUS_ERR_NO_FILE      = 0x01,   /* Requested filename not found */
	STATUS_ERR_FS           = 0x02,   /* Filesystem error */
	STATUS_ERR_BUSY         = 0x03,   /* Transfer already in progress */

	STATUS_TRANSFER_DONE    = 0x04,   /* All chunks sent */
	STATUS_TRANSFER_ABORTED = 0x05    /* Aborted due to command or error */
} ctrl_status_code_t;

/* --- Data packet layout (shears → base) ---------------------------------- */

/*
 * Each notification on the data characteristic carries one file chunk.
 *
 * Layout:
 *   [0..1]  uint16_t chunkIndex (little-endian)
 *   [2.. ]  raw file bytes
 *
 * chunkIndex starts at 0 and increments by 1 per notification.
 * This is used to detect missing or out-of-order packets.
 */
typedef struct __attribute__((packed)) {
	uint16_t chunkIndex;
	uint8_t  data[];
} log_xfer_chunk_t;
