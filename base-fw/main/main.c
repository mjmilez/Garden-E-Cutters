#include <stdio.h>
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

static const char *TAG = "base_ble";

#define BASE_STATUS_LED_GPIO GPIO_NUM_33

static uint8_t ownAddrType;
static const char *targetName = "WM-SHEARS";

static TaskHandle_t ledTaskHandle = NULL;
static volatile bool ledBlinking = false;

static void startScan(void);

/* Simple LED task:
 * - fast blink while scanning / trying to connect
 * - solid on while a link is active
 * Avoid turning the LED off after ledBlinking is cleared.
 */
static void ledTask(void *arg) {
	while (1) {
		if (ledBlinking) {
			gpio_set_level(BASE_STATUS_LED_GPIO, 1);
			vTaskDelay(pdMS_TO_TICKS(100));

			/* Check again before turning it off so we don't
			 * override the solid-on state after a connect.
			 */
			if (!ledBlinking) {
				continue;
			}

			gpio_set_level(BASE_STATUS_LED_GPIO, 0);
			vTaskDelay(pdMS_TO_TICKS(100));
		} else {
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/* Extract device name from advertisement fields */
static void getAdvName(const struct ble_hs_adv_fields *fields, char *out, size_t outLen) {
	if (outLen == 0) {
		return;
	}

	if (fields->name_len > 0 && fields->name != NULL) {
		size_t len = fields->name_len;
		if (len >= outLen) {
			len = outLen - 1;
		}
		memcpy(out, fields->name, len);
		out[len] = '\0';
	} else {
		out[0] = '\0';
	}
}

/* GAP event handler */
static int gapEventHandler(struct ble_gap_event *event, void *arg) {
	switch (event->type) {

	case BLE_GAP_EVENT_DISC: {
		struct ble_hs_adv_fields fields;
		int rc = ble_hs_adv_parse_fields(&fields,
		                                 event->disc.data,
		                                 event->disc.length_data);
		if (rc != 0) {
			return 0;
		}

		char name[32];
		getAdvName(&fields, name, sizeof(name));

		/* Ignore anything that isn't WM-SHEARS */
		if (name[0] == '\0' || strcmp(name, targetName) != 0) {
			return 0;
		}

		ESP_LOGI(TAG,
		         "Saw WM-SHEARS: %02X:%02X:%02X:%02X:%02X:%02X",
		         event->disc.addr.val[5],
		         event->disc.addr.val[4],
		         event->disc.addr.val[3],
		         event->disc.addr.val[2],
		         event->disc.addr.val[1],
		         event->disc.addr.val[0]);

		ESP_LOGI(TAG, "Found target device \"%s\", attempting to connect", targetName);

		ble_gap_disc_cancel();

		struct ble_gap_conn_params connParams;
		memset(&connParams, 0, sizeof(connParams));
		connParams.scan_itvl = 0x0010;
		connParams.scan_window = 0x0010;
		connParams.itvl_min = 0x0010;
		connParams.itvl_max = 0x0020;
		connParams.latency = 0;
		connParams.supervision_timeout = 0x0258;
		connParams.min_ce_len = 0x0010;
		connParams.max_ce_len = 0x0300;

		rc = ble_gap_connect(ownAddrType,
		                     &event->disc.addr,
		                     300,
		                     &connParams,
		                     gapEventHandler,
		                     NULL);
		if (rc != 0) {
			ESP_LOGE(TAG, "Error calling ble_gap_connect, rc=%d", rc);
			startScan();
		}
		break;
	}

	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "Connected to device");
			/* Connected: LED solid on */
			ledBlinking = false;
			gpio_set_level(BASE_STATUS_LED_GPIO, 1);
		} else {
			ESP_LOGW(TAG, "Connection failed, status=%d, restarting scan", event->connect.status);
			/* Back to scanning: blink again */
			ledBlinking = true;
			startScan();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Disconnected, restarting scan");
		/* Lost link: go back to blink + scan */
		ledBlinking = true;
		gpio_set_level(BASE_STATUS_LED_GPIO, 0);
		startScan();
		break;

	case BLE_GAP_EVENT_DISC_COMPLETE:
		ESP_LOGI(TAG, "Scan complete, restarting");
		ledBlinking = true;
		startScan();
		break;

	default:
		break;
	}

	return 0;
}

static void startScan(void) {
	struct ble_gap_disc_params params;
	memset(&params, 0, sizeof(params));

	params.passive = 0;              /* active scan to get scan responses */
	params.itvl = 0x0010;
	params.window = 0x0010;
	params.filter_duplicates = 0;

	int rc = ble_gap_disc(ownAddrType,
	                      BLE_HS_FOREVER,
	                      &params,
	                      gapEventHandler,
	                      NULL);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error starting scan, rc=%d", rc);
	} else {
		ESP_LOGI(TAG, "Scanning for \"%s\"...", targetName);
	}
}

/* Called when NimBLE host is synced and ready */
static void onSync(void) {
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Error determining address type, rc=%d", rc);
		return;
	}

	ble_svc_gap_device_name_set("WM-BASE");

	/* Start scanning with LED in blink state */
	ledBlinking = true;
	startScan();
}

/* NimBLE host task */
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
		.pin_bit_mask = 1ULL << BASE_STATUS_LED_GPIO,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&io_conf);
	gpio_set_level(BASE_STATUS_LED_GPIO, 0);

	/* LED task */
	xTaskCreate(ledTask, "base_led", 2048, NULL, 5, &ledTaskHandle);

	/* In ESP-IDF 5.x, controller and HCI init is handled in nimble_port_init */
	nimble_port_init();

	ble_hs_cfg.sync_cb = onSync;

	ble_svc_gap_init();
	ble_svc_gatt_init();

	nimble_port_freertos_init(hostTask);
}
