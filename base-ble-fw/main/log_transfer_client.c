/*
 * log_transfer_client.c
 *
 * Base-side client for the log transfer protocol.
 *
 * High-level behavior:
 *   - START_TRANSFER is written to the control characteristic with a filename
 *   - status updates arrive on the control characteristic
 *   - file chunks arrive on the data characteristic
 *   - payload is written to SPIFFS when available, otherwise stored in RAM
 *   - on completion, the first few lines are printed for a quick sanity check
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_err.h"

#include "host/ble_hs.h"

#include "log_transfer_client.h"
#include "log_transfer_protocol.h"
#include "log_paths.h"   /* GPS_LOG_FILE_PATH, GPS_LOG_FILE_BASENAME */

static const char *TAG = "log_xfer_cli";

/* --- Internal state ------------------------------------------------------- */

typedef struct {
	bool     active;
	char     requestedName[64];   /* name requested from the shears */
	FILE    *fp;                  /* local output file (SPIFFS) */

	/* RAM fallback when filesystem output is unavailable. */
	uint8_t *buf;
	uint32_t buf_size;

	uint32_t expectedSize;
	uint32_t bytesReceived;
	uint16_t nextChunkIndex;
} base_log_transfer_state_t;

static log_transfer_client_cfg_t g_cfg;
static base_log_transfer_state_t g_state;

/* Debug helper used after transfer completion. */
static void dump_downloaded_file(void);

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

		/* Tear down any previous transfer state. */
		if (g_state.active) {
			if (g_state.fp) {
				fclose(g_state.fp);
				g_state.fp = NULL;
			}
			if (g_state.buf) {
				free(g_state.buf);
				g_state.buf = NULL;
				g_state.buf_size = 0;
			}
		}

		/* Primary destination is SPIFFS; fall back to RAM on failure. */
		g_state.fp = fopen(GPS_LOG_FILE_PATH, "wb");
		if (!g_state.fp) {
			ESP_LOGE(TAG,
			         "Failed to open local file '%s', using RAM buffer only",
			         GPS_LOG_FILE_PATH);

			g_state.buf = (uint8_t *)malloc(fileSize);
			if (!g_state.buf) {
				ESP_LOGE(TAG, "RAM allocation failed for %u bytes", fileSize);
				g_state.active = false;
				return;
			}
			g_state.buf_size = fileSize;
		} else {
			ESP_LOGI(TAG, "Opened local file '%s' for writing", GPS_LOG_FILE_PATH);
			g_state.buf = NULL;
			g_state.buf_size = 0;
		}

		g_state.active         = true;
		g_state.expectedSize   = fileSize;
		g_state.bytesReceived  = 0;
		g_state.nextChunkIndex = 0;

		ESP_LOGI(TAG, "Transfer accepted; size=%u bytes (dest='%s', RAM=%s)",
		         fileSize,
		         GPS_LOG_FILE_PATH,
		         g_state.buf ? "yes" : "no");
		break;
	}

	case STATUS_TRANSFER_DONE:
		/* Transfer complete; close outputs and print a short preview. */
		if (g_state.active) {
			ESP_LOGI(TAG,
			         "Transfer finished from shears: received=%u bytes, expected=%u",
			         g_state.bytesReceived, g_state.expectedSize);

			if (g_state.fp) {
				fclose(g_state.fp);
				g_state.fp = NULL;
			}
		} else {
			ESP_LOGW(TAG, "Transfer done but no active state");
		}

		g_state.active = false;

		dump_downloaded_file();
		break;

	case STATUS_ERR_NO_FILE:
		ESP_LOGW(TAG, "Shears: file not found");
		break;

	case STATUS_ERR_BUSY:
		ESP_LOGW(TAG, "Shears: busy");
		break;

	case STATUS_ERR_FS:
		ESP_LOGW(TAG, "Shears: filesystem error");
		break;

	case STATUS_TRANSFER_ABORTED:
		ESP_LOGW(TAG, "Shears: transfer aborted");
		if (g_state.active && g_state.fp) {
			fclose(g_state.fp);
			g_state.fp = NULL;
		}
		if (g_state.active && g_state.buf) {
			free(g_state.buf);
			g_state.buf = NULL;
			g_state.buf_size = 0;
		}
		g_state.active = false;
		break;

	default:
		ESP_LOGW(TAG, "Unhandled STATUS 0x%02X", status);
		break;
	}
}

void log_transfer_client_on_data_notify(const uint8_t *data, uint16_t len)
{
	ESP_LOGI(TAG, "DATA notify: len=%u", len);

	ESP_LOGI(TAG, "chunk bytes: %02X %02X", data[0], data[1]);

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
		ESP_LOGW(TAG, "Chunk mismatch: got %u expected %u (resync for debug)",
				chunkIndex, g_state.nextChunkIndex);
		g_state.nextChunkIndex = chunkIndex;
	}

	size_t payloadLen = len - 2;
	const uint8_t *payload = &data[2];

	printf("---- CHUNK %u (%u bytes) ----\n", chunkIndex, (unsigned)payloadLen);

	for (size_t i = 0; i < payloadLen; i++) {
		putchar(payload[i]);
	}

	printf("\n---- END CHUNK ----\n");

	/* Stream into SPIFFS if an output file is active. */
	if (g_state.fp) {
		size_t written = fwrite(payload, 1, payloadLen, g_state.fp);
		(void)written;
	}

	/* Optional RAM copy when filesystem output is unavailable. */
	if (g_state.buf && g_state.buf_size > 0) {
		if (g_state.bytesReceived + payloadLen <= g_state.buf_size) {
			memcpy(&g_state.buf[g_state.bytesReceived], payload, payloadLen);
		} else {
			ESP_LOGW(TAG, "RAM buffer overflow; dropping extra data");
		}
	}

	g_state.bytesReceived += payloadLen;
	g_state.nextChunkIndex++;
}

/* --- Debug helpers -------------------------------------------------------- */

static void dump_downloaded_file(void)
{
	/* Prefer RAM buffer output when available. */
	if (g_state.buf && g_state.expectedSize > 0) {
		ESP_LOGI(TAG, "Dumping first lines from RAM buffer (%u bytes):",
		         g_state.expectedSize);

		const char *ptr = (const char *)g_state.buf;
		const char *end = ptr + g_state.expectedSize;
		char line[128];
		int lineCount = 0;

		while (ptr < end && lineCount < 5) {
			size_t i = 0;

			while (ptr < end && *ptr != '\n' && i < sizeof(line) - 1) {
				line[i++] = *ptr++;
			}
			line[i] = '\0';

			if (i > 0) {
				ESP_LOGI(TAG, "%s", line);
				lineCount++;
			}

			if (ptr < end && *ptr == '\n') {
				ptr++;
			}
		}

		/* RAM buffer is only retained for the duration of the transfer. */
		free(g_state.buf);
		g_state.buf = NULL;
		g_state.buf_size = 0;

		return;
	}

	/* Fall back to reading from SPIFFS if no RAM buffer exists. */
	FILE *fp = fopen(GPS_LOG_FILE_PATH, "rb");
	if (!fp) {
		ESP_LOGE(TAG, "Could not open downloaded file '%s' for dump", GPS_LOG_FILE_PATH);
		return;
	}

	ESP_LOGI(TAG, "Dumping first lines of '%s':", GPS_LOG_FILE_PATH);

	char line[128];
	int lineCount = 0;
	while (fgets(line, sizeof(line), fp) && lineCount < 5) {
		ESP_LOGI(TAG, "%s", line);
		lineCount++;
	}

	fclose(fp);
}