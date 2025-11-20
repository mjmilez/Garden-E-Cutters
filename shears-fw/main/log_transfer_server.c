#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "host/ble_hs.h"

#include "log_transfer_protocol.h"

static const char *TAG = "log_xfer_srv";

#define LOG_SVC_UUID       0xFFF0
#define LOG_CTRL_CHR_UUID  0xFFF1
#define LOG_DATA_CHR_UUID  0xFFF2

typedef struct {
	bool        active;
	char        filename[64];
	FILE       *fp;
	uint32_t    file_size;
	uint32_t    bytes_sent;
	uint16_t    chunk_index;
	uint16_t    chunk_size;
	uint16_t    conn_handle;
	uint16_t    ctrl_val_handle;
	uint16_t    data_val_handle;
} log_transfer_t;

static log_transfer_t g_log_xfer;

static uint16_t g_ctrl_char_handle = 0;
static uint16_t g_data_char_handle = 0;

static int log_ctrl_access_cb(uint16_t conn_handle,
			      uint16_t attr_handle,
			      struct ble_gatt_access_ctxt *ctxt,
			      void *arg);
static int log_data_access_cb(uint16_t conn_handle,
			      uint16_t attr_handle,
			      struct ble_gatt_access_ctxt *ctxt,
			      void *arg);

static void log_transfer_task(void *arg);
static void handle_start_transfer(uint16_t conn_handle,
				  const uint8_t *filename_buf,
				  uint16_t filename_len);
static void handle_abort_transfer(void);
static void send_status(ctrl_status_code_t status, uint32_t file_size);

static struct ble_gatt_svc_def log_svc_def[] = {
	{
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(LOG_SVC_UUID),
		.characteristics = (struct ble_gatt_chr_def[]) {
			{
				.uuid       = BLE_UUID16_DECLARE(LOG_CTRL_CHR_UUID),
				.access_cb  = log_ctrl_access_cb,
				.flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
				.val_handle = &g_ctrl_char_handle,
			},
			{
				.uuid       = BLE_UUID16_DECLARE(LOG_DATA_CHR_UUID),
				.access_cb  = log_data_access_cb,
				.flags      = BLE_GATT_CHR_F_NOTIFY,
				.val_handle = &g_data_char_handle,
			},
			{ 0 }
		},
	},
	{ 0 }
};

static void send_status(ctrl_status_code_t status, uint32_t file_size)
{
	uint8_t payload[1 + 1 + 4];
	uint16_t len = 0;

	payload[len++] = CTRL_EVT_STATUS;
	payload[len++] = (uint8_t)status;

	if (status == STATUS_OK) {
		memcpy(&payload[len], &file_size, sizeof(file_size));
		len += sizeof(file_size);
	}

	if (g_log_xfer.ctrl_val_handle == 0) {
		g_log_xfer.ctrl_val_handle = g_ctrl_char_handle;
	}

	if (g_log_xfer.ctrl_val_handle == 0 || g_log_xfer.conn_handle == 0) {
		return;
	}

	struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, len);
	if (!om) {
		return;
	}

	int rc = ble_gatts_notify_custom(g_log_xfer.conn_handle,
					 g_log_xfer.ctrl_val_handle,
					 om);
	if (rc != 0) {
		ESP_LOGW(TAG, "STATUS notify failed rc=%d", rc);
	}
}

static void handle_start_transfer(uint16_t conn_handle,
				  const uint8_t *filename_buf,
				  uint16_t filename_len)
{
	if (g_log_xfer.ctrl_val_handle == 0) {
		g_log_xfer.ctrl_val_handle = g_ctrl_char_handle;
	}
	if (g_log_xfer.data_val_handle == 0) {
		g_log_xfer.data_val_handle = g_data_char_handle;
	}

	if (g_log_xfer.active) {
		send_status(STATUS_ERR_BUSY, 0);
		return;
	}

	if (filename_len == 0 || filename_len >= sizeof(g_log_xfer.filename)) {
		send_status(STATUS_ERR_FS, 0);
		return;
	}

	memcpy(g_log_xfer.filename, filename_buf, filename_len);
	g_log_xfer.filename[filename_len] = '\0';

	ESP_LOGI(TAG, "Start transfer for file '%s'", g_log_xfer.filename);

	FILE *fp = fopen(g_log_xfer.filename, "rb");
	if (!fp) {
		ESP_LOGW(TAG, "File not found");
		send_status(STATUS_ERR_NO_FILE, 0);
		return;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		send_status(STATUS_ERR_FS, 0);
		return;
	}

	long size = ftell(fp);
	if (size < 0) {
		fclose(fp);
		send_status(STATUS_ERR_FS, 0);
		return;
	}

	fseek(fp, 0, SEEK_SET);

	g_log_xfer.active      = true;
	g_log_xfer.fp          = fp;
	g_log_xfer.file_size   = (uint32_t)size;
	g_log_xfer.bytes_sent  = 0;
	g_log_xfer.chunk_index = 0;
	g_log_xfer.conn_handle = conn_handle;
	g_log_xfer.chunk_size  = 160;

	send_status(STATUS_OK, g_log_xfer.file_size);
}

static void handle_abort_transfer(void)
{
	if (!g_log_xfer.active) {
		return;
	}

	if (g_log_xfer.fp) {
		fclose(g_log_xfer.fp);
		g_log_xfer.fp = NULL;
	}

	g_log_xfer.active = false;
	send_status(STATUS_TRANSFER_ABORTED, g_log_xfer.file_size);
}

static int log_ctrl_access_cb(uint16_t conn_handle,
			      uint16_t attr_handle,
			      struct ble_gatt_access_ctxt *ctxt,
			      void *arg)
{
	(void)attr_handle;
	(void)arg;

	if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
		return 0;
	}

	uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
	if (len == 0) {
		return 0;
	}

	uint8_t buf[80];
	if (len > sizeof(buf)) {
		len = sizeof(buf);
	}

	os_mbuf_copydata(ctxt->om, 0, len, buf);
	uint8_t opcode = buf[0];

	switch ((ctrl_opcode_t)opcode) {
	case CTRL_CMD_START_TRANSFER:
		handle_start_transfer(conn_handle, &buf[1], len - 1);
		break;

	case CTRL_CMD_ABORT:
		handle_abort_transfer();
		break;

	default:
		ESP_LOGW(TAG, "Unknown CTRL opcode 0x%02X", opcode);
		break;
	}

	return 0;
}

static int log_data_access_cb(uint16_t conn_handle,
			      uint16_t attr_handle,
			      struct ble_gatt_access_ctxt *ctxt,
			      void *arg)
{
	(void)conn_handle;
	(void)attr_handle;
	(void)ctxt;
	(void)arg;

	return 0;
}

static void log_transfer_task(void *arg)
{
	(void)arg;

	uint8_t buf[2 + 160];

	while (1) {
		if (g_log_xfer.active && g_log_xfer.fp) {
			if (g_log_xfer.data_val_handle == 0) {
				g_log_xfer.data_val_handle = g_data_char_handle;
			}

			size_t to_read = g_log_xfer.chunk_size;
			size_t n = fread(&buf[2], 1, to_read, g_log_xfer.fp);

			if (n > 0 && g_log_xfer.data_val_handle != 0 &&
			    g_log_xfer.conn_handle != 0) {
				uint16_t idx = g_log_xfer.chunk_index;
				memcpy(&buf[0], &idx, sizeof(idx));

				struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, 2 + n);
				if (om) {
					int rc = ble_gatts_notify_custom(g_log_xfer.conn_handle,
									 g_log_xfer.data_val_handle,
									 om);
					if (rc != 0) {
						ESP_LOGW(TAG, "DATA notify failed rc=%d", rc);
					}
				}

				g_log_xfer.bytes_sent += n;
				g_log_xfer.chunk_index++;
			}

			if (n < to_read) {
				if (g_log_xfer.fp) {
					fclose(g_log_xfer.fp);
					g_log_xfer.fp = NULL;
				}
				g_log_xfer.active = false;
				send_status(STATUS_TRANSFER_DONE, g_log_xfer.file_size);
			}

			vTaskDelay(pdMS_TO_TICKS(10));
		} else {
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

void log_transfer_server_init(void)
{
	memset(&g_log_xfer, 0, sizeof(g_log_xfer));

	int rc = ble_gatts_count_cfg(log_svc_def);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_count_cfg failed; rc=%d", rc);
		return;
	}

	rc = ble_gatts_add_svcs(log_svc_def);
	if (rc != 0) {
		ESP_LOGE(TAG, "ble_gatts_add_svcs failed; rc=%d", rc);
		return;
	}

	ESP_LOGI(TAG, "Log transfer service registered");

	xTaskCreate(log_transfer_task, "log_xfer_task", 4096, NULL, 5, NULL);
}
