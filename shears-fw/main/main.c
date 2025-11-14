#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "shears_ble";

static uint8_t ownAddrType;
static const char *deviceName = "WM-SHEARS";

/* GAP event handler */
static int gapEventHandler(struct ble_gap_event *event, void *arg) {
	switch (event->type) {

	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "Connected");
		} else {
			ESP_LOGW(TAG, "Connect failed, restarting advertising");
			ble_gap_adv_start(ownAddrType,
			                  NULL,
			                  BLE_HS_FOREVER,
			                  &(struct ble_gap_adv_params){0},
			                  gapEventHandler,
			                  NULL);
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Disconnected, restarting advertising");
		ble_gap_adv_start(ownAddrType,
		                  NULL,
		                  BLE_HS_FOREVER,
		                  &(struct ble_gap_adv_params){0},
		                  gapEventHandler,
		                  NULL);
		break;

	default:
		break;
	}

	return 0;
}

static void startAdvertising(void) {
	struct ble_hs_adv_fields fields;
	memset(&fields, 0, sizeof(fields));

	/* General discoverable, BLE only */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/* Device name in advertisement */
	fields.name = (uint8_t *)deviceName;
	fields.name_len = strlen(deviceName);
	fields.name_is_complete = 1;

	/* Optional: one custom 16 bit service UUID, 0xFFF0 */
	uint16_t serviceUuid = 0xFFF0;
	ble_uuid16_t bleUuid16 = {
	    .u = { .type = BLE_UUID_TYPE_16 },
	    .value = serviceUuid
	};
	fields.uuids16 = &bleUuid16;
	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 1;

	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error setting advertisement data, rc=%d", rc);
		return;
	}

	struct ble_gap_adv_params advParams;
	memset(&advParams, 0, sizeof(advParams));
	advParams.conn_mode = BLE_GAP_CONN_MODE_UND;  // connectable undirected
	advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;  // general discoverable

	rc = ble_gap_adv_start(ownAddrType,
	                       NULL,
	                       BLE_HS_FOREVER,
	                       &advParams,
	                       gapEventHandler,
	                       NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error starting advertising, rc=%d", rc);
	} else {
		ESP_LOGI(TAG, "Advertising as \"%s\"", deviceName);
	}
}

/* Called when the NimBLE host and controller are synced and ready */
static void onSync(void) {
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error determining address type, rc=%d", rc);
		return;
	}

	/* Set GAP device name (also used by some helper functions) */
	ble_svc_gap_device_name_set(deviceName);

	startAdvertising();
}

/* NimBLE host task */
static void hostTask(void *param) {
	nimble_port_run();              // This function blocks
	nimble_port_freertos_deinit();  // Clean up on exit (never normally reached)
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Controller + HCI init is now done inside nimble_port_init() in IDF 5.x
    nimble_port_init();

    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.sync_cb = onSync;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    nimble_port_freertos_init(hostTask);
}
