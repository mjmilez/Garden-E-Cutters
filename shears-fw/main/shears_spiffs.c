#include "shears_spiffs.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_err.h"

static const char* TAG = "shears_spiffs";

bool shearsSpiffsInit(void)
{
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = "storage",
		.max_files = 5,
		.format_if_mount_failed = true
	};

	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Mount/format failed");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Partition not found");
		} else {
			ESP_LOGE(TAG, "SPIFFS init error (%s)", esp_err_to_name(ret));
		}
		return false;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "Mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
	} else {
		ESP_LOGW(TAG, "Info failed (%s)", esp_err_to_name(ret));
	}

	return true;
}