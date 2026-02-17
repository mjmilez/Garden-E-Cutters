/*
 * uart_bridge.h
 *
 * UART bridge interface for communication with the Raspberry Pi.
 * 
 * Sends framed messages over UART so the Pi can reliably parse
 * incoming cut data and status updates. Uses UART1 (not UART0,
 * which is reserved for debug console).
 * 
 * Frame format:
 *   [0]       0xAA           start-of-frame marker
 *   [1]       msg_type       message type (see uart_msg_type_t)
 *   [2..3]    payload_len    uint16_t payload length
 *   [4..N]    payload        raw bytes
 *   [N+1]     checksum       XOR of bytes [1] through [N]
 *
 * The Pi side mirrors this format in its receiver.
 */

 #pragma once

 #include <stdint.h>
 #include <stddef.h>
 #include "esp_err.h"

/* ── Hardware configuration ─────────────────────────────────────────────── */

/* Using UART1. UART0 is the USB debug/monitor console */
#define UART_BRIDGE_PORT_NUM	1
#define UART_BRIDGE_BAUD     	115200
#define UART_BRIDGE_TX_PIN      17		/* ESP32 GPIO17 → Pi RX (GPIO15) */
#define UART_BRIDGE_RX_PIN     	16		/* ESP32 GPIO16 → Pi TX (GPIO14) */

/* ── Frame constants ────────────────────────────────────────────────────── */

#define UART_FRAME_START 		0xAA
#define UART_FRAME_MAX_PAYLOAD	200

/* ── Message types ──────────────────────────────────────────────────────── */

typedef enum {
	/* 
	* Cut event record.
	* Payload: uart_cut_record_t (21 bytes)
	*/
	UART_MSG_CUT_RECORD = 0x01,

	/*
	* Status update. (connnection state, transfer progress, etc.)
	* Payload: 1 byte status code (see uart_status_code_t)
	*/
	UART_MSG_STATUS = 0x02,

	/*
	* Raw log line forwarded from the shears CSV.
	* Payload: variable-length ASCII string (no null terminator needed)
	*/
	UART_MSG_LOG_LINE = 0x03,
} uart_msg_type_t;

/* ── Status codes for UART_MSG_STATUS ───────────────────────────────────── */

typedef enum {
	UART_STATUS_SHEAR_CONNECTED = 0x01,
	UART_STATUS_SHEAR_DISCONNECTED = 0x02,
	UART_STATUS_TRANSFER_START = 0x03,
	UART_STATUS_TRANSFER_DONE = 0x04,
	UART_STATUS_TRANSFER_ERROR = 0x05,
} uart_status_code_t;

/* ── Cut record structure (packed, sent as UART_MSG_CUT_RECORD payload) ── */

typedef struct __attribute__((packed)) {
	uint32_t sequence_id;
	uint32_t timestamp;		 /* Unix epoch seconds */
	float    latitude;
	float    longitude;
	float    force;          /* in kg, Pi converts to Newtons */
	uint8_t  fix_type;       /* GPS fix quality */
} uart_cut_record_t;         /* 21 bytes total */

/* ── Public API ─────────────────────────────────────────────────────────── */

/*
 * Initializes UART1 with the configured pins and baud rate.
 * Call once from app_main()
 */
void uart_bridge_init(void);

/*
 * Sends a framed message over UART
 * Builds the frame (start byte, type, length, payload, checksum)
 * and writes it atomically to the UART TX FIFO.
 */
esp_err_t uart_bridge_send(uart_msg_type_t msg_type,
						   const uint8_t *payload,
					  	   uint16_t len);

/*
 * Convenience: sends a single cut record to the Pi.
 */
esp_err_t uart_bridge_send_cut(const uart_cut_record_t *record);

/*
 * Convenience: sends a status update to the Pi.
 */
esp_err_t uart_bridge_send_status(uart_status_code_t status);

/*
 * Convenience: sends a raw log line (CSV row from shears) to the Pi.
 */
esp_err_t uart_bridge_send_log_line(const char *line, uint16_t len);
