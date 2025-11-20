/**
 * @file shears_ble.c
 * @brief Implements BLE peripheral behavior for the shears node.
 *
 * Responsibilities:
 *  - Initialize NimBLE host/controller
 *  - Advertise as "WM-SHEARS" with a custom 0xFFF0 service UUID
 *  - Accept connections from a central (the base station)
 *  - Restart advertising on disconnect
 *  - Notify application layer when the connection is established or lost
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

#include "log_transfer_server.h"   /* Log CSV transfer GATT service */

/* Log tag for this module */
static const char *TAG = "shears_ble";

/* Device BLE name */
static const char *deviceName = "WM-SHEARS";

/* Our own address type (public/random), determined at runtime */
static uint8_t ownAddrType;

/* App-supplied callback to report connect/disconnect events */
static shearsBleConnCallback_t connCallback = NULL;

/* Forward declarations */
static void startAdvertising(void);
static int gapEventHandler(struct ble_gap_event *event, void *arg);
static void onSync(void);
static void hostTask(void *param);

/**
 * @brief GAP event handler for the shears peripheral.
 *
 * At this point we only care about:
 *  - BLE_GAP_EVENT_CONNECT     : central connected or connect failed
 *  - BLE_GAP_EVENT_DISCONNECT  : link dropped / central disconnected
 */
static int gapEventHandler(struct ble_gap_event *event, void *arg) {
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
		/* Other GAP events (MTU updates, PHY changes, etc.) not handled here. */
		break;
	}

	return 0;
}

/**
 * @brief Configure advertising payload and start advertising indefinitely.
 */
static void startAdvertising(void) {
	struct ble_hs_adv_fields fields;
	memset(&fields, 0, sizeof(fields));

	/* General discoverable; BR/EDR (classic) unsupported */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/* Advertise the complete device name "WM-SHEARS" */
	fields.name = (uint8_t *)deviceName;
	fields.name_len = strlen(deviceName);
	fields.name_is_complete = 1;

	/* Example 16-bit service UUID (0xFFF0) to indicate a custom service */
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

/**
 * @brief NimBLE sync callback.
 *
 * Called by the BLE host when it is fully initialized and ready. At this point
 * the device address is known and we can safely start advertising.
 */
static void onSync(void) {
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error determining address type, rc=%d", rc);
		return;
	}

	/* Publish our local name as WM-SHEARS */
	ble_svc_gap_device_name_set(deviceName);

	/* Start advertising immediately; app can set LED to "blinking" on init */
	startAdvertising();
}

/**
 * @brief NimBLE host FreeRTOS task entry point.
 */
static void hostTask(void *param) {
	(void)param;
	nimble_port_run();
	nimble_port_freertos_deinit();
}

/**
 * @brief Public entry point: initialize BLE stack and begin advertising.
 *
 * This wraps:
 *  - NVS init (required by BLE controller)
 *  - NimBLE init
 *  - GAP/GATT service init
 *  - Launches NimBLE host task
 */
void shearsBleInit(shearsBleConnCallback_t cb) {
	connCallback = cb;

	/* Initialize NVS storage used by the BLE controller */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Initialize NimBLE stack */
	nimble_port_init();

	/* Register host callbacks */
	ble_hs_cfg.sync_cb = onSync;
	ble_hs_cfg.reset_cb = NULL;

	/* Initialize standard GAP/GATT services */
	ble_svc_gap_init();
	ble_svc_gatt_init();

	/* Register custom log transfer service */
	log_transfer_server_init();

	/* Start the NimBLE host task */
	nimble_port_freertos_init(hostTask);

	ESP_LOGI(TAG, "Shears BLE initialized");
}
