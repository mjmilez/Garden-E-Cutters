/*
 * gps_logger.c
 *
 * GPS NMEA logger for the shears firmware.
 *
 * Responsibilities:
 *   - configure UART2 for 115200 baud NMEA input
 *   - keep the most recent full NMEA sentence in latestNmea[]
 *   - accept save requests from a GPIO button or gpsLoggerRequestSave()
 *   - on save, append one $GNGGA row to gps_points.csv
 *
 * Note:
 *   - SPIFFS is mounted elsewhere (app_main). This module only uses the filesystem.
 */

#include "gps_logger.h"
#include "shears_piezo.h"
#include "shears_primeSwitch.h"
#include "shears_gpsButtons.h"
#include "shears_gpsStorage.h"

#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "log_paths.h"

#define GPS_UART_NUM   UART_NUM_2
#define GPS_UART_RX    GPIO_NUM_16
#define GPS_UART_TX    GPIO_NUM_17
#define GPS_BUF_SIZE   512

#define LED_STATUS_PIN      GPIO_NUM_25

#define CLEAR_HOLD_US         5000000
#define LED_BLINK_ON_MS       200
#define LED_BLINK_OFF_MS      200

static const char *TAG = "gps_logger";

static char latestNmea[GPS_BUF_SIZE];

static volatile bool nmeaValid = false;
static volatile bool saveRequestedFlag = false;

static volatile bool clearRequestedFlag = false;
static int64_t buttonPressTimeUs = 0;

static volatile bool clearBeepRequested = false;
static volatile int cutBeepCount = 0;

static volatile bool gpsButtonHeld = false;
static volatile bool clearTriggered = false;

static volatile bool captureNextGGA = false;

static void uartReadTask(void *arg);
static void saveTask(void *arg);
static void requestCutFeedback(void);

/* ISR callbacks (wired up by shears_gpsButtons) */
static void onPrimeLevel(int level);
static void onGpsButtonLevel(int level);
static void onCutPress(gpio_num_t pin);

static void IRAM_ATTR onPrimeLevel(int level)
{
	shearsPrimeSwitchUpdateFromLevel(level);

	if (shearsPrimeSwitchConsumeUnprimedEdge()) {
		captureNextGGA = false;
	}
}

static void IRAM_ATTR onGpsButtonLevel(int level)
{
	if (level == 0) {
		buttonPressTimeUs = esp_timer_get_time();
		gpsButtonHeld = true;
		clearTriggered = false;
		return;
	}

	int64_t nowTime = esp_timer_get_time();
	int64_t timeDiff = nowTime - buttonPressTimeUs;

	gpsButtonHeld = false;
	clearTriggered = false;

	if (timeDiff < CLEAR_HOLD_US) {
		if (shearsPrimeSwitchIsPrimed() && !captureNextGGA) {
			captureNextGGA = true;
			requestCutFeedback();
		}
	}
}

static void IRAM_ATTR onCutPress(gpio_num_t pin)
{
	(void)pin;

	if (shearsPrimeSwitchIsPrimed() && !captureNextGGA) {
		captureNextGGA = true;
		requestCutFeedback();
	}
}

static void uartReadTask(void *arg)
{
	(void)arg;

	uint8_t data[GPS_BUF_SIZE];
	static char nmea_buf[GPS_BUF_SIZE];
	size_t nmea_len = 0;

	while (1) {
		int len = uart_read_bytes(GPS_UART_NUM,
		                          data,
		                          GPS_BUF_SIZE - 1,
		                          pdMS_TO_TICKS(100));

		if (len > 0) {
			for (int i = 0; i < len; i++) {
				char c = (char)data[i];

				if (nmea_len < GPS_BUF_SIZE - 1) {
					nmea_buf[nmea_len++] = c;
				}

				if (c == '\n') {
					nmea_buf[nmea_len] = '\0';

					if (captureNextGGA && strncmp(nmea_buf, "$GNGGA,", 7) == 0) {
						strncpy(latestNmea, nmea_buf, GPS_BUF_SIZE);
						nmeaValid = true;

						captureNextGGA = false;
						saveRequestedFlag = true;
					}

					nmea_len = 0;
				}
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

static void requestCutFeedback(void)
{
	cutBeepCount++;
}

static void saveTask(void *arg)
{
	(void)arg;

	while (1) {
		static int64_t lastBlinkUs = 0;
		static bool ledOn = true;

		bool primed = shearsPrimeSwitchIsPrimed();

		if (gpsButtonHeld && !primed && !clearTriggered) {
			int64_t nowUs = esp_timer_get_time();
			if ((nowUs - buttonPressTimeUs) >= CLEAR_HOLD_US) {
				clearTriggered = true;
				clearRequestedFlag = true;
				clearBeepRequested = true;
			}
		}

		if (clearRequestedFlag) {
			clearRequestedFlag = false;

			shearsGpsStorageClearCsv(GPS_LOG_FILE_PATH);

			memset(latestNmea, 0, sizeof(latestNmea));
			nmeaValid = false;

			if (clearBeepRequested) {
				clearBeepRequested = false;
				shearsPiezoToneMs(2000);
			}
		} else if (saveRequestedFlag) {
			saveRequestedFlag = false;

			if (nmeaValid) {
				ESP_LOGI(TAG, "Save requested; latest NMEA: %s", latestNmea);

				shearsGpsStorageAppendGngga(GPS_LOG_FILE_PATH, latestNmea);

				nmeaValid = false;
				memset(latestNmea, 0, sizeof(latestNmea));

				shearsGpsStoragePrintNewest(GPS_LOG_FILE_PATH, 5);
			} else {
				ESP_LOGW(TAG, "Save requested but no valid NMEA data available");
			}
		}

		if (shearsPrimeSwitchConsumePrimedEdge()) {
			shearsPiezoBeepPattern(3);
		}

		if (cutBeepCount > 0) {
			int count = cutBeepCount;
			cutBeepCount = 0;
			shearsPiezoBeepPattern(count);
		}

		if (!primed) {
			if (!ledOn) {
				ledOn = true;
				gpio_set_level(LED_STATUS_PIN, 1);
			}
		} else {
			int64_t nowUs = esp_timer_get_time();
			int64_t intervalUs = (ledOn ? LED_BLINK_ON_MS : LED_BLINK_OFF_MS) * 1000;
			if ((nowUs - lastBlinkUs) >= intervalUs) {
				ledOn = !ledOn;
				gpio_set_level(LED_STATUS_PIN, ledOn ? 1 : 0);
				lastBlinkUs = nowUs;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void gpsLoggerInit(void)
{
	/* SPIFFS is mounted outside this module. Just ensure our file exists. */
	shearsGpsStorageEnsureCsvExists(GPS_LOG_FILE_PATH);

	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};

	uart_param_config(GPS_UART_NUM, &uart_config);
	uart_set_pin(GPS_UART_NUM,
	             GPS_UART_TX,
	             GPS_UART_RX,
	             UART_PIN_NO_CHANGE,
	             UART_PIN_NO_CHANGE);
	uart_driver_install(GPS_UART_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0);

	ESP_LOGI(TAG, "UART2 configured for GPS at 115200 baud");

	shearsPrimeSwitchInit(gpio_get_level(SHEARS_PRIME_BUTTON_PIN));

	shearsGpsButtonsCallbacks_t callbacks = {
		.onPrimeLevel = onPrimeLevel,
		.onGpsButtonLevel = onGpsButtonLevel,
		.onCutPress = onCutPress
	};
	shearsGpsButtonsInit(&callbacks);

	gpio_config_t out_conf = {
		.pin_bit_mask = (1ULL << LED_STATUS_PIN),
		.mode         = GPIO_MODE_OUTPUT,
		.pull_up_en   = 0,
		.pull_down_en = 0,
		.intr_type    = GPIO_INTR_DISABLE
	};
	gpio_config(&out_conf);
	gpio_set_level(LED_STATUS_PIN, 1);

	shearsPiezoInit();

	bool primed = shearsPrimeSwitchIsPrimed();

	ESP_LOGI(TAG, "Input interrupts configured on GPS=%d PRIME=%d CUT2=%d CUT3=%d",
	         SHEARS_GPS_BUTTON_PIN, SHEARS_PRIME_BUTTON_PIN, SHEARS_CUT2_BUTTON_PIN, SHEARS_CUT3_BUTTON_PIN);
	ESP_LOGI(TAG, "Prime switch startup state: %s", primed ? "PRIMED" : "SAFE");

	xTaskCreate(uartReadTask, "gps_uart_read", 4096, NULL, 5, NULL);
	xTaskCreate(saveTask, "gps_save_task", 4096, NULL, 5, NULL);
}

void gpsLoggerRequestSave(void)
{
	if (nmeaValid) {
		saveRequestedFlag = true;
	} else {
		ESP_LOGW(TAG, "Save requested but no valid NMEA data available");
	}
}

void gpsLoggerPrintCsv(void)
{
	shearsGpsStoragePrintNewest(GPS_LOG_FILE_PATH, 5);
}