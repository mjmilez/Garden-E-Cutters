#pragma once

#include <stdint.h>

/*
 * Shared protocol definitions for the BLE log transfer path.
 *
 * Both the shears firmware and the base firmware include this header so the
 * control opcodes, status codes, and packet layout stay aligned. This header
 * contains only constants and enums; all implementation lives in each side's
 * client/server modules.
 */

/* -------------------------------------------------------------------------- */
/* Control opcodes (base → shears)                                            */
/* -------------------------------------------------------------------------- */
typedef enum {
    CTRL_CMD_NOP            = 0x00,

    /*
     * Request a file transfer from the shears.
     *
     * Payload layout:
     *   [0]     CTRL_CMD_START_TRANSFER
     *   [1..N]  Null-terminated ASCII filename ("gps_points.csv", etc.)
     *
     * The shears is responsible for converting this basename into a full
     * filesystem path before opening the file.
     */
    CTRL_CMD_START_TRANSFER = 0x01,

    /*
     * Abort any active transfer. The shears should stop sending chunks
     * and reply with STATUS_TRANSFER_ABORTED.
     */
    CTRL_CMD_ABORT          = 0x02,

    /*
     * Control events coming back from the shears use the 0x80 namespace.
     */
    CTRL_EVT_STATUS         = 0x80
} ctrl_opcode_t;

/* -------------------------------------------------------------------------- */
/* Status / event codes (shears → base)                                       */
/* -------------------------------------------------------------------------- */
typedef enum {
    STATUS_OK               = 0x00,   /* File exists; transfer accepted */
    STATUS_ERR_NO_FILE      = 0x01,   /* Requested filename not found */
    STATUS_ERR_FS           = 0x02,   /* Filesystem error opening or reading */
    STATUS_ERR_BUSY         = 0x03,   /* A transfer is already in progress */

    STATUS_TRANSFER_DONE    = 0x04,   /* All chunks sent; transfer complete */
    STATUS_TRANSFER_ABORTED = 0x05    /* Aborted due to command or error */
} ctrl_status_code_t;

/* -------------------------------------------------------------------------- */
/* Data packet layout (shears → base)                                         */
/* -------------------------------------------------------------------------- */
/*
 * Each notification on the data characteristic carries one chunk of the file.
 *
 * Layout:
 *   [0..1]  uint16_t chunkIndex (little-endian)
 *           - First chunk = 0
 *           - Base uses this to detect dropped or out-of-order packets
 *
 *   [2.. ]  Raw file bytes
 *
 * The amount of data per packet depends on the negotiated MTU. CRC is not
 * included at this layer; future versions may add per-chunk or per-entry CRC.
 */
typedef struct __attribute__((packed)) {
    uint16_t chunkIndex;
    uint8_t  data[];
} log_xfer_chunk_t;
