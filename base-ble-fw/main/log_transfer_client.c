/*
 * log_transfer_client.c
 *
 * Base-side client for the log transfer protocol.
 *
 * High-level behavior:
 *   - START_TRANSFER is written to the control characteristic with a filename
 *   - status updates arrive on the control characteristic
 *   - file chunks arrive on the data characteristic
 *   - each chunk's payload is forwarded to the Pi over UART
 *   - transfer completion/error status is also forwarded to the Pi
 * 
 * Beta Changes:
 *  - Removed SPIFFS file output (no local storage)
 *  - Removed RAM buffer fallback (assumes Pi can keep up with UART streaming)
 *  - Removed file dumping helper (no local file to read back from)
 *  - Added uart_bridge calls to forward data and status to the Pi
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_err.h"

#include "host/ble_hs.h"

#include "log_transfer_client.h"
#include "log_transfer_protocol.h"
#include "log_paths.h"   /* GPS_LOG_FILE_PATH, GPS_LOG_FILE_BASENAME */
#include "uart_bridge.h" /* uart_bridge_send_data, uart_bridge_send_status */

static const char *TAG = "log_xfer_cli";

/* --- Internal state ------------------------------------------------------- */

typedef struct {
	bool     active;
	char     requestedName[64];   /* name requested from the shears */

	uint32_t expectedSize;
	uint32_t bytesReceived;
	uint16_t nextChunkIndex;
} base_log_transfer_state_t;

static log_transfer_client_cfg_t g_cfg;
static base_log_transfer_state_t g_state;

/* --- Public API ----------------------------------------------------------- */

void log_transfer_client_init(const log_transfer_client_cfg_t *cfg)
{
	memset(&g_state, 0, sizeof(g_state));

	if (cfg) {
		g_cfg = *cfg;
	}

	ESP_LOGI(TAG, "client_init: conn=%u ctrl=0x%04x data=0x%04x",
	         g_cfg.connHandle, g_cfg.ctrlChrHandle, g_cfg.dataChrHandle);
}

void log_transfer_client_set_conn_handle(uint16_t connHandle)
{
	g_cfg.connHandle = connHandle;
}

/*
 * Sends START_TRANSFER with the requested filename over the control
 * characteristic.
 */
esp_err_t log_transfer_client_request_file(const char *filename)
{
	if (!filename || filename[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	if (g_cfg.ctrlChrHandle == 0) {
		ESP_LOGE(TAG, "Control characteristic handle is 0; client not initialized");
		return ESP_FAIL;
	}

	uint8_t buf[1 + 64];
	uint16_t len = 0;

	buf[len++] = CTRL_CMD_START_TRANSFER;

	size_t nameLen = strnlen(filename, sizeof(buf) - 2);
	memcpy(&buf[len], filename, nameLen);
	len += nameLen;
	buf[len++] = '\0';

	strncpy(g_state.requestedName, filename, sizeof(g_state.requestedName));
	g_state.requestedName[sizeof(g_state.requestedName) - 1] = '\0';

	int rc = ble_gattc_write_flat(g_cfg.connHandle,
	                              g_cfg.ctrlChrHandle,
	                              buf,
	                              len,
	                              NULL,
	                              NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gattc_write_flat failed rc=%d", rc);
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Requested file '%s' from shears (conn=%u, ctrl=0x%04x)",
	         g_state.requestedName, g_cfg.connHandle, g_cfg.ctrlChrHandle);

	return ESP_OK;
}

/* --- Notification handlers ------------------------------------------------ */

void log_transfer_client_on_ctrl_notify(const uint8_t *data, uint16_t len)
{
	ESP_LOGI(TAG, "CTRL notify: len=%u", len);

	if (len < 2) {
		return;
	}

	uint8_t opcode = data[0];
	uint8_t status = data[1];

	ESP_LOGI(TAG, "CTRL notify: opcode=0x%02X status=0x%02X", opcode, status);

	if (opcode != CTRL_EVT_STATUS) {
		ESP_LOGW(TAG, "Unknown CTRL EVT opcode 0x%02X", opcode);
		return;
	}

	ctrl_status_code_t st = (ctrl_status_code_t)status;

	switch (st) {

	case STATUS_OK: {
		/* Request accepted; payload includes the file size. */
		if (len < 6) {
			ESP_LOGW(TAG, "STATUS_OK without size field");
			return;
		}

		uint32_t fileSize = 0;
		memcpy(&fileSize, &data[2], sizeof(fileSize));

		/* Reset transfer state for the new transfer.*/
		g_state.active         = true;
		g_state.expectedSize   = fileSize;
		g_state.bytesReceived  = 0;
		g_state.nextChunkIndex = 0;

		ESP_LOGI(TAG, "Transfer accepted; size=%" PRIu32 " bytes", fileSize);

		/* Notify Pi that a transfer is starting. */
		uart_bridge_send_status(UART_STATUS_TRANSFER_START);
		break;
	}

	case STATUS_TRANSFER_DONE:
		/* Transfer complete; close outputs and print a short preview. */
		if (g_state.active) {
			ESP_LOGI(TAG,
			         "Transfer finished from shears: received=%" PRIu32 " bytes, expected=%" PRIu32,
			         g_state.bytesReceived, g_state.expectedSize);

		} else {
			ESP_LOGW(TAG, "Transfer done but no active state");
		}

		g_state.active = false;

		/* Notify Pi that the transfer is complete. */
		uart_bridge_send_status(UART_STATUS_TRANSFER_DONE);
		break;

	case STATUS_ERR_NO_FILE:
		ESP_LOGW(TAG, "Shears: file not found");
		uart_bridge_send_status(UART_STATUS_TRANSFER_ERROR);
		break;

	case STATUS_ERR_BUSY:
		ESP_LOGW(TAG, "Shears: busy");
		uart_bridge_send_status(UART_STATUS_TRANSFER_ERROR);
		break;

	case STATUS_ERR_FS:
		ESP_LOGW(TAG, "Shears: filesystem error");
		uart_bridge_send_status(UART_STATUS_TRANSFER_ERROR);
		break;

	case STATUS_TRANSFER_ABORTED:
		ESP_LOGW(TAG, "Shears: transfer aborted");
		g_state.active = false;
		uart_bridge_send_status(UART_STATUS_TRANSFER_ERROR);
		break;

	default:
		ESP_LOGW(TAG, "Unhandled STATUS 0x%02X", status);
		break;
	}
}

void log_transfer_client_on_data_notify(const uint8_t *data, uint16_t len)
{
	ESP_LOGI(TAG, "DATA notify: len=%u", len);

	if (!g_state.active) {
		return;
	}

	if (len < 3) {
		return;
	}

	/* First two bytes are the chunk index, remaining bytes are payload. */
	uint16_t chunkIndex = 0;
	memcpy(&chunkIndex, &data[0], sizeof(chunkIndex));
	ESP_LOGI(TAG, "DATA notify: chunk=%u", chunkIndex);

	if (chunkIndex != g_state.nextChunkIndex) {
		ESP_LOGW(TAG, "Chunk out of order: got %u expected %u",
		         chunkIndex, g_state.nextChunkIndex);
		return;
	}

	size_t payloadLen = len - 2;
	const uint8_t *payload = &data[2];

	/* Forward the chunk payload to the Pi over UART. */
	uart_bridge_send_log_line((const char *)payload, (uint16_t)payloadLen);

	g_state.bytesReceived += payloadLen;
	g_state.nextChunkIndex++;

	ESP_LOGD(TAG, "Chunk %u forwarded: %u bytes (total %" PRIu32 "/%" PRIu32 ")",
         chunkIndex, (unsigned)payloadLen,
         g_state.bytesReceived, g_state.expectedSize);
}
