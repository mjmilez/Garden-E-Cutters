/*
 * main.c
 *
 * Base station entry point.
 *
 * Startup sequence:
 *   - mount SPIFFS for log storage
 *   - start the status LED
 *   - initialize BLE central and scan for WM-SHEARS
 *   - on connect, request the GPS log
 */

#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "base_led.h"
#include "base_ble.h"
#include "csv_debug_button.h"
#include "base_uartFileTransfer.h"
#include "log_paths.h"   /* GPS_LOG_FILE_BASENAME, GPS_LOG_FILE_PATH */

static const char *TAG = "app_main";

/* --- SPIFFS --------------------------------------------------------------- */

/* Mounts the SPIFFS partition so /spiffs/... paths are available. */
static void init_spiffs(void)
{
	esp_vfs_spiffs_conf_t conf = {
		.base_path              = "/spiffs",
		.partition_label        = "storage",
		.max_files              = 5,
		.format_if_mount_failed = true,
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "SPIFFS partition not found");
		} else {
			ESP_LOGE(TAG, "SPIFFS init error (%s)", esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0;
	size_t used  = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "SPIFFS mounted: total=%u, used=%u",
		         (unsigned)total, (unsigned)used);
	} else {
		ESP_LOGW(TAG, "SPIFFS info failed (%s)", esp_err_to_name(ret));
	}
}

/* --- BLE connection state ------------------------------------------------- */

/* Connection state callback passed into base_ble. */
static void bleConnChanged(bool connected)
{
	if (connected) {
		/* Link up: solid LED and request the GPS log. */
		baseLedSetSolidOn();

		/* Shears side resolves basename to its filesystem path. */
		esp_err_t err = bleBaseRequestLog(GPS_LOG_FILE_BASENAME);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to request log (%s)", esp_err_to_name(err));
		}
	} else {
		/* Link down: blink while scanning / reconnecting. */
		baseLedSetBlinking(true);
	}
}

/* --- Entry point ---------------------------------------------------------- */

void app_main(void)
{
	init_spiffs();
	csvDebugButtonInit();
	baseLedInit();
	baseLedSetBlinking(true);
	bleBaseInit(bleConnChanged);
	uartFileTransferInit();

	ESP_LOGI("main", "Ready. Press button to transfer CSV.");
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	/* BLE and LED behavior run from their own tasks / callbacks. */
}
