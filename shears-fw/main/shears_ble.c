/*
 * shears_ble.c
 *
 * BLE peripheral implementation for the shears node.
 *
 * Responsibilities:
 *   - initialize NimBLE host/controller
 *   - advertise as "WM-SHEARS" and include the custom 0xFFF0 service UUID
 *   - accept connections from the base (central)
 *   - restart advertising on disconnect
 *   - forward connection state to the application callback
 */

#include "shears_ble.h"

#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "log_transfer_server.h"   /* Log transfer GATT service */

static const char *TAG = "shears_ble";

/* BLE GAP name used for advertising and discovery. */
static const char *deviceName = "WM-SHEARS";

/* Address type selected by the NimBLE host after sync. */
static uint8_t ownAddrType;

/* Optional application callback for connect/disconnect state. */
static shearsBleConnCallback_t connCallback = NULL;

/* Forward declarations. */
static void startAdvertising(void);
static int  gapEventHandler(struct ble_gap_event *event, void *arg);
static void onSync(void);
static void hostTask(void *param);

/* --- GAP event handler ---------------------------------------------------- */

static int gapEventHandler(struct ble_gap_event *event, void *arg)
{
	(void)arg;

	switch (event->type) {

	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "Connected to central");
			if (connCallback) {
				connCallback(true);
			}
		} else {
			ESP_LOGW(TAG,
			         "Connect failed (status=%d), restarting advertising",
			         event->connect.status);

			if (connCallback) {
				connCallback(false);
			}
			startAdvertising();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Disconnected, restarting advertising");
		if (connCallback) {
			connCallback(false);
		}
		startAdvertising();
		break;

	default:
		/* Unused GAP events (MTU updates, PHY changes, etc.). */
		break;
	}

	return 0;
}

/* --- Advertising ---------------------------------------------------------- */

static void startAdvertising(void)
{
	struct ble_hs_adv_fields fields;
	memset(&fields, 0, sizeof(fields));

	/* General discoverable; BR/EDR (classic) unsupported. */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/* Complete device name. */
	fields.name = (uint8_t *)deviceName;
	fields.name_len = strlen(deviceName);
	fields.name_is_complete = 1;

	/* Include the custom 16-bit service UUID (0xFFF0). */
	uint16_t serviceUuid = 0xFFF0;
	ble_uuid16_t uuid16 = {
		.u = { .type = BLE_UUID_TYPE_16 },
		.value = serviceUuid
	};
	fields.uuids16 = &uuid16;
	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 1;

	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error setting advertisement data, rc=%d", rc);
		return;
	}

	struct ble_gap_adv_params advParams;
	memset(&advParams, 0, sizeof(advParams));

	advParams.conn_mode = BLE_GAP_CONN_MODE_UND;  /* undirected connectable */
	advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;  /* general discovery mode */

	rc = ble_gap_adv_start(ownAddrType,
	                       NULL,
	                       BLE_HS_FOREVER,
	                       &advParams,
	                       gapEventHandler,
	                       NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error starting advertising, rc=%d", rc);
	} else {
		ESP_LOGI(TAG, "Advertising as \"%s\" (connectable)", deviceName);
	}
}

/* --- NimBLE lifecycle ----------------------------------------------------- */

static void onSync(void)
{
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error determining address type, rc=%d", rc);
		return;
	}

	ble_svc_gap_device_name_set(deviceName);

	startAdvertising();
}

static void hostTask(void *param)
{
	(void)param;
	nimble_port_run();
	nimble_port_freertos_deinit();
}

/* --- Public API ----------------------------------------------------------- */

void shearsBleInit(shearsBleConnCallback_t cb)
{
	connCallback = cb;

	/* NVS is required by the controller; handle the "no free pages" case. */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	nimble_port_init();

	ble_hs_cfg.sync_cb = onSync;
	ble_hs_cfg.reset_cb = NULL;

	ble_svc_gap_init();
	ble_svc_gatt_init();

	log_transfer_server_init();

	nimble_port_freertos_init(hostTask);

	ESP_LOGI(TAG, "Shears BLE initialized");
}
