/*
 * Base station entry point.
 *
 * Rough flow:
 *   - mount SPIFFS so we have somewhere to put CSV logs
 *   - bring up the status LED
 *   - bring up BLE (as a central) and start scanning for WM-SHEARS
 *   - when the link comes up, switch the LED and kick off a log request
 */

#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"

#include "base_led.h"
#include "base_ble.h"
#include "log_paths.h"   /* GPS_LOG_FILE_BASENAME, GPS_LOG_FILE_PATH */

static const char *TAG = "app_main";

/*
 * Mount SPIFFS on the "storage" partition so /spiffs/... is usable.
 * Partition label must match the partition table entry.
 */
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

/*
 * BLE connection state callback used by base_ble.
 *
 * This is the only place app_main really "reacts" to BLE:
 *   - connected: solid LED, immediately request the GPS log
 *   - not connected: blink LED while we are scanning / reconnecting
 */
static void bleConnChanged(bool connected)
{
	if (connected) {
		/* Solid LED → link is up. */
		baseLedSetSolidOn();

		/* Ask the shears for the GPS log by basename.
		 * Shears side is responsible for adding "/spiffs/".
		 */
		esp_err_t err = bleBaseRequestLog(GPS_LOG_FILE_BASENAME);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to request log (%s)", esp_err_to_name(err));
		}
	} else {
		/* Blink while we hunt for the shears. */
		baseLedSetBlinking(true);
	}
}

void app_main(void)
{
	/* Make sure /spiffs/gps_points.csv is a valid path on the base. */
	init_spiffs();

	/* LED starts blinking right away so we have a heartbeat while scanning. */
	baseLedInit();
	baseLedSetBlinking(true);

	/* Bring up BLE; scanning begins inside base_ble once the stack syncs. */
	bleBaseInit(bleConnChanged);

	/* No foreground loop here; BLE and LED work happen in their own tasks. */
}
