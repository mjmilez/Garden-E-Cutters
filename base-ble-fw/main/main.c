/*
 * main.c
 *
 * Base station entry point.
 *
 * Stripped-down base station firmware 
 * Focused on BLE connectivity and log transfer.
 * 
 * Removed features: (Now handled by Pi)
 *  - SPIFFS initialization (no local file storage)
 * 	- Web server (no user interface)
 * 	- Wi-Fi AP connectivity (no remote access)
 * 
 * Startup sequence:
 *  - Initialize NVS (required for BLE)
 *  - Initialize UART bridge to Pi
 * 	- Initialize status LED (start blinking)
 *  - Initialize BLE central with connection callback
 * 
 */

#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "base_led.h"
#include "base_ble.h"
#include "log_paths.h"   /* GPS_LOG_FILE_BASENAME, GPS_LOG_FILE_PATH */
#include "uart_bridge.h" /* UART_STATUS_SHEAR_CONNECTED, UART_STATUS_SHEAR_DISCONNECTED */

static const char *TAG = "app_main";

/* --- BLE connection state ------------------------------------------------- */

/* Connection state callback passed into base_ble. */
static void bleConnChanged(bool connected)
{
	if (connected) {
		/* Link up: solid LED and request the GPS log. */
		ESP_LOGI(TAG, "=== SHEARS CONNECTED ===");
		baseLedSetSolidOn();

		/* Notify the Pi that a shear connected */
		uart_bridge_send_status(UART_STATUS_SHEAR_CONNECTED);

		/* Shears side resolves basename to its filesystem path. */
		esp_err_t err = bleBaseRequestLog(GPS_LOG_FILE_BASENAME);
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "Failed to request log (%s)", esp_err_to_name(err));
		}
	} else {
		/* Link down: blink while scanning / reconnecting. */
		ESP_LOGI(TAG, "=== SHEARS DISCONNECTED ===");
		baseLedSetBlinking(true);

		/* Notify the Pi that the shear disconnected */
		uart_bridge_send_status(UART_STATUS_SHEAR_DISCONNECTED);
	}
}

/* --- Entry point ---------------------------------------------------------- */

void app_main(void)
{
	ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  Watermelon Hub ESP32 – Beta");
    ESP_LOGI(TAG, "  BLE Gateway + UART Bridge to Pi");
    ESP_LOGI(TAG, "=========================================");

	/* NVS initialization required for BLE controller */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Initialize UART bridge to the Pi */
	uart_bridge_init();
	ESP_LOGI(TAG, "UART bridge ready (TX=GPIO%d, RX=GPIO%d, %d baud)",
			 UART_BRIDGE_TX_PIN, UART_BRIDGE_RX_PIN, UART_BRIDGE_BAUD);

	/* Status LED: blink while scanning */
	baseLedInit();
	baseLedSetBlinking(true);

	/* BLE Central: scan for WM-SHEARS */
	bleBaseInit(bleConnChanged);

	/* BLE, UART and LED behavior run from their own tasks / callbacks. */
}