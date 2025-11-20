#pragma once

#include <stdint.h>

/**
 * @file log_transfer_protocol.h
 * @brief Shared BLE file-transfer protocol between shears and base station.
 *
 * This header defines the application-level protocol used by both sides
 * for CSV log file transfer over BLE. It contains only enums, constants,
 * and data definitions — no implementation — so both projects stay in sync.
 */

/* -------------------------------------------------------------------------- */
/*                          Control Command Opcodes                           */
/* -------------------------------------------------------------------------- */
/**
 * Commands sent *from base → to shears* via the control characteristic.
 */
typedef enum {
    CTRL_CMD_NOP            = 0x00,

    /**
     * Base requests a file transfer from the shears.
     * Payload:
     *   [0]   = CTRL_CMD_START_TRANSFER
     *   [1..] = null-terminated ASCII filename (e.g. "session_001.csv")
     */
    CTRL_CMD_START_TRANSFER = 0x01,

    /**
     * Abort any in-progress transfer.
     * Payload:
     *   [0] = CTRL_CMD_ABORT
     */
    CTRL_CMD_ABORT          = 0x02,

    /* Events from shears → base start at 0x80 */
    CTRL_EVT_STATUS         = 0x80
} ctrl_opcode_t;


/* -------------------------------------------------------------------------- */
/*                              Status / Events                               */
/* -------------------------------------------------------------------------- */
/**
 * Status codes sent *from shears → base* via the control characteristic.
 */
typedef enum {
    STATUS_OK               = 0x00,   /**< Transfer accepted; file exists */
    STATUS_ERR_NO_FILE      = 0x01,   /**< Filename not found on shears FS */
    STATUS_ERR_FS           = 0x02,   /**< Filesystem error on shears side */
    STATUS_ERR_BUSY         = 0x03,   /**< Transfer already in progress */

    STATUS_TRANSFER_DONE    = 0x04,   /**< Shears finished sending file */
    STATUS_TRANSFER_ABORTED = 0x05    /**< Transfer aborted */
} ctrl_status_code_t;


/* -------------------------------------------------------------------------- */
/*                         Data Characteristic Format                         */
/* -------------------------------------------------------------------------- */
/**
 * Packets sent *from shears → base* over the data characteristic.
 *
 * Format:
 *   [0..1] = uint16_t chunkIndex (LE). First chunk = 0, increments by 1.
 *   [2.. ] = raw file bytes (CSV file data)
 *
 * Notes:
 *   - Max payload = (MTU - 3 - 2)
 *   - No CRC here — can be added later if desired
 */
typedef struct __attribute__((packed)) {
    uint16_t chunkIndex;
    uint8_t  data[];   /* Flexible array (actual size derived from MTU) */
} log_xfer_chunk_t;

