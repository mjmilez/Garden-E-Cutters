#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "shears_ble";

#define SHEARS_STATUS_LED_GPIO GPIO_NUM_33

static uint8_t ownAddrType;
static const char *deviceName = "WM-SHEARS";

static TaskHandle_t ledTaskHandle = NULL;
static volatile bool ledBlinking = false;

static void startAdvertising(void);

/* Simple LED status task:
 * - fast blink while advertising / trying to connect
 * - solid on while connected (set directly in GAP handler)
 *
 * Make sure we don't turn the LED off after ledBlinking is cleared.
 */
static void ledTask(void *arg) {
	while (1) {
		if (ledBlinking) {
			gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
			vTaskDelay(pdMS_TO_TICKS(100));

			/* Check again before turning it off so we don't
			 * override the solid-on state after a connect.
			 */
			if (!ledBlinking) {
				continue;
			}

			gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
			vTaskDelay(pdMS_TO_TICKS(100));
		} else {
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/* GAP event handler */
static int gapEventHandler(struct ble_gap_event *event, void *arg) {
	switch (event->type) {

	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "Connected to central");
			/* Connected: stop blinking and drive LED high */
			ledBlinking = false;
			gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
		} else {
			ESP_LOGW(TAG, "Connect failed (status=%d), restarting adv",
			         event->connect.status);
			/* Back to advertising: blink again */
			ledBlinking = true;
			startAdvertising();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Disconnected, restarting adv");
		/* Back to advertising: blink */
		ledBlinking = true;
		gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
		startAdvertising();
		break;

	default:
		break;
	}

	return 0;
}

static void startAdvertising(void) {
	struct ble_hs_adv_fields fields;
	memset(&fields, 0, sizeof(fields));

	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	fields.name = (uint8_t *)deviceName;
	fields.name_len = strlen(deviceName);
	fields.name_is_complete = 1;

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

	advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
	advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;

	rc = ble_gap_adv_start(ownAddrType, NULL, BLE_HS_FOREVER,
	                       &advParams, gapEventHandler, NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error starting advertising, rc=%d", rc);
	} else {
		ESP_LOGI(TAG, "Advertising as \"%s\" (connectable)", deviceName);
	}
}

/* Called when NimBLE host is synced */
static void onSync(void) {
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error determining address type, rc=%d", rc);
		return;
	}

	ble_svc_gap_device_name_set(deviceName);

	/* Start in advertising state: blink LED */
	ledBlinking = true;
	startAdvertising();
}

/* Host task */
static void hostTask(void *param) {
	nimble_port_run();
	nimble_port_freertos_deinit();
}

void app_main(void) {
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* GPIO for status LED */
	gpio_config_t io_conf = {
		.pin_bit_mask = 1ULL << SHEARS_STATUS_LED_GPIO,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&io_conf);
	gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);

	/* LED task */
	xTaskCreate(ledTask, "shears_led", 2048, NULL, 5, &ledTaskHandle);

	nimble_port_init();

	ble_hs_cfg.sync_cb = onSync;
	ble_hs_cfg.reset_cb = NULL;

	ble_svc_gap_init();
	ble_svc_gatt_init();

	nimble_port_freertos_init(hostTask);
}
