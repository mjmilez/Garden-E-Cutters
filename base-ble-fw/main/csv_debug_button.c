#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"

#include <errno.h>
#include <stdio.h>

#ifndef GPS_LOG_FILE_PATH
#define GPS_LOG_FILE_PATH	"/spiffs/gps_points.csv"
#endif

#define csvDebugButtonGpio		GPIO_NUM_27
#define debounceMs				200

static void formatUtcTime(const char *nmeaUtc, char *out, size_t outLen);
static const char* TAG = "csvDbgBtn";
static TaskHandle_t buttonTaskHandle = NULL;

static void printCsvFile(void){
  FILE *f = fopen(GPS_LOG_FILE_PATH, "r");
  if (!f) {
    ESP_LOGE(TAG, "Could not open CSV file for read");
    return;
  }

#define MAX_LINES 5
#define LINE_BUF  256

  char header[LINE_BUF] = {0};

  char lines[MAX_LINES][LINE_BUF];
  int lineNums[MAX_LINES];

  int dataLinesSeen = 0;
  char buffer[LINE_BUF];

  //read header
  if (!fgets(header, sizeof(header), f)) {
    fclose(f);
    ESP_LOGW(TAG, "CSV file is empty");
    return;
  }

  //read data rows
  while (fgets(buffer, sizeof(buffer), f)) {
    int idx = dataLinesSeen % MAX_LINES;

    strncpy(lines[idx], buffer, sizeof(lines[idx]) - 1);
    lines[idx][sizeof(lines[idx]) - 1] = '\0';

    //header line is line 1 
    //data line is after
    lineNums[idx] = dataLinesSeen + 1;

    dataLinesSeen++;
  }

  fclose(f);

  ESP_LOGI(TAG, "---- Newest GPS Data Points ----");

  if (dataLinesSeen == 0) {
    ESP_LOGI(TAG, "(no data rows yet)");
    return;
  }

  //table
  printf("\n");
  printf("line | %-11s | %-11s | %-12s | %-3s | %-4s | %-4s | %-8s | %-11s\n",
         "utc_time", "latitude", "longitude", "fix", "sats", "hdop", "alt(m)", "geoid(m)");
  printf("-----+-------------+-------------+--------------+-----+------+------+-"
         "----------+------------\n");

  int linesToPrint = (dataLinesSeen < MAX_LINES) ? dataLinesSeen : MAX_LINES;
  int start = (dataLinesSeen >= MAX_LINES) ? (dataLinesSeen % MAX_LINES) : 0;

  for (int i = 0; i < linesToPrint; i++) {
    int idx = (start + i) % MAX_LINES;

    char row[LINE_BUF];
    strncpy(row, lines[idx], sizeof(row) - 1);
    row[sizeof(row) - 1] = '\0';

    // stripping
    size_t len = strlen(row);
    if (len > 0 && row[len - 1] == '\n') {
      row[len - 1] = '\0';
    }

    char *tokens[8] = {0};
    int t = 0;

    char *tok = strtok(row, ",");
    while (tok != NULL && t < 8) {
      tokens[t++] = tok;
      tok = strtok(NULL, ",");
    }

    if (t < 8) {
      printf("%4d | (malformed) %s\n", lineNums[idx], lines[idx]);
      continue;
    }

    //Utc time
    char timeFmt[16];
    formatUtcTime(tokens[0], timeFmt, sizeof(timeFmt));

    printf("%4d | %-10s | %11s | %12s | %3s | %4s | %4s | %8s | %11s\n",
           lineNums[idx],
           timeFmt,
           tokens[1], /* latitude */
           tokens[2], /* longitude */
           tokens[3], /* fix_quality */
           tokens[4], /* num_satellites */
           tokens[5], /* hdop */
           tokens[6], /* altitude */
           tokens[7]  /* geoid_height */
           );
  }

  printf("\n");
}

static void IRAM_ATTR buttonIsrHandler(void* arg)
{
	BaseType_t higherPriorityWoken = pdFALSE;
	vTaskNotifyGiveFromISR(buttonTaskHandle, &higherPriorityWoken);
	if (higherPriorityWoken) {
		portYIELD_FROM_ISR();
	}
}

static void buttonTask(void* arg)
{
	uint32_t lastPressTick = 0;

	while (1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		uint32_t now = xTaskGetTickCount();
		uint32_t msSince = (now - lastPressTick) * portTICK_PERIOD_MS;

		if (msSince < debounceMs) {
			continue;
		}
		lastPressTick = now;

		if (gpio_get_level(csvDebugButtonGpio) == 0) {
			ESP_LOGI(TAG, "Button -> print CSV");
			printCsvFile();
		}
	}
}

void csvDebugButtonInit(void)
{
	ESP_LOGI(TAG, "Init CSV debug button on GPIO %d", (int)csvDebugButtonGpio);

	xTaskCreate(buttonTask, "csvBtnTask", 4096, NULL, 10, &buttonTaskHandle);

	gpio_config_t ioConf = {0};
	ioConf.intr_type = GPIO_INTR_NEGEDGE;
	ioConf.mode = GPIO_MODE_INPUT;
	ioConf.pin_bit_mask = (1ULL << csvDebugButtonGpio);
	ioConf.pull_up_en = GPIO_PULLUP_ENABLE;
	ioConf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&ioConf));

	esp_err_t err = gpio_install_isr_service(0);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
		return;
	}

	ESP_ERROR_CHECK(gpio_isr_handler_add(csvDebugButtonGpio, buttonIsrHandler, NULL));
}

static void formatUtcTime(const char *nmeaUtc, char *out, size_t outLen){
  if (!nmeaUtc || strlen(nmeaUtc) < 6) {
    snprintf(out, outLen, "--:--:--");
    return;
  }

  char hh[3] = { nmeaUtc[0], nmeaUtc[1], '\0' };
  char mm[3] = { nmeaUtc[2], nmeaUtc[3], '\0' };
  char ss[16];

  //copy time units
  snprintf(ss, sizeof(ss), "%s", nmeaUtc + 4);

  snprintf(out, outLen, "%s:%s:%s", hh, mm, ss);
}