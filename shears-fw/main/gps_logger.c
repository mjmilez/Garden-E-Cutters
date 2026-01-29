/*
 * gps_logger.c
 *
 * GPS NMEA logger for the shears firmware.
 *
 * Responsibilities:
 *   - mount SPIFFS and ensure gps_points.csv exists with a header
 *   - configure UART2 for 115200 baud NMEA input
 *   - keep the most recent full NMEA sentence in latestNmea[]
 *   - accept save requests from a GPIO button or gpsLoggerRequestSave()
 *   - on save, parse $GPGGA and append one CSV row
 */

#include "gps_logger.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_spiffs.h"
#include "esp_err.h"

#include "log_paths.h"

#define GPS_UART_NUM   UART_NUM_2
#define GPS_UART_RX    GPIO_NUM_16
#define GPS_UART_TX    GPIO_NUM_17
#define GPS_BUF_SIZE   512

#define GPS_BUTTON_PIN GPIO_NUM_23

static const char *TAG = "gps_logger";

static char latestNmea[GPS_BUF_SIZE];

static volatile bool nmeaValid = false;

static volatile bool saveRequestedFlag = false;
static volatile bool clearRequestedFlag=false;

static void buttonIsrHandler(void *arg);
static void uartReadTask(void *arg);
static void saveTask(void *arg);
static void initSpiffs(void);
static double nmeaToDecimal(const char *nmea_val, char hemisphere);
static void storeGgaCsv(const char *nmea);
static void printCsvFile(void);
static void formatUtcTime(const char *nmeaUtc, char *out, size_t outLen);

static void IRAM_ATTR buttonIsrHandler(void *arg){
  (void)arg;
  saveRequestedFlag = true;
}


static void uartReadTask(void *arg){
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
          strncpy(latestNmea, nmea_buf, GPS_BUF_SIZE);
          nmeaValid = true;
          nmea_len = 0;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static double nmeaToDecimal(const char *nmea_val, char hemisphere){
  double val = atof(nmea_val);
  int degrees = (int)(val / 100);
  double minutes = val - (degrees * 100);
  double decimal = degrees + minutes / 60.0;

  if (hemisphere == 'S' || hemisphere == 'W') {
    decimal *= -1;
  }

  return decimal;
}

static void storeGgaCsv(const char *nmea){
  if (strncmp(nmea, "$GNGGA,", 7) != 0) {
    return;
  }

  char copy[GPS_BUF_SIZE];
  strncpy(copy, nmea, GPS_BUF_SIZE);

  char *tokens[20];
  int i = 0;
  char *tok = strtok(copy, ",");

  while (tok != NULL && i < 20) {
    tokens[i++] = tok;
    tok = strtok(NULL, ",");
  }

  if (i < 12) {
    ESP_LOGW(TAG, "GNGGA sentence too short, i=%d", i);
    return;
  }

  const char *utc_time = tokens[1];
  double lat = nmeaToDecimal(tokens[2], tokens[3][0]);
  double lon = nmeaToDecimal(tokens[4], tokens[5][0]);
  int fix = atoi(tokens[6]);
  int num_sats = atoi(tokens[7]);
  double hdop = atof(tokens[8]);
  double altitude = atof(tokens[9]);
  double geoid_height = atof(tokens[11]);

  FILE *f = fopen(GPS_LOG_FILE_PATH, "a");
  if (!f) {
    ESP_LOGE(TAG, "Error opening CSV for append");
    return;
  }

  fprintf(f, "%s,%.7f,%.7f,%d,%d,%.1f,%.3f,%.3f\n",
          utc_time,
          lat,
          lon,
          fix,
          num_sats,
          hdop,
          altitude,
          geoid_height);

  fclose(f);

  ESP_LOGI(TAG, "GPS point saved: time=%s lat=%.7f lon=%.7f",
           utc_time, lat, lon);
}

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
    lineNums[idx] = dataLinesSeen + 2;

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

static void initSpiffs(void){
  esp_vfs_spiffs_conf_t conf = {
    .base_path              = "/spiffs",
    .partition_label        = "storage",
    .max_files              = 5,
    .format_if_mount_failed = true
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
    } 
    else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "SPIFFS partition not found");
    } 
    else {
      ESP_LOGE(TAG, "SPIFFS init error (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS mounted: total=%u, used=%u",
             (unsigned)total, (unsigned)used);
  } 
  else {
    ESP_LOGW(TAG, "SPIFFS info failed (%s)", esp_err_to_name(ret));
  }

  // make sure csv prints with header
  FILE *f = fopen(GPS_LOG_FILE_PATH, "r");
  if (!f) {
    f = fopen(GPS_LOG_FILE_PATH, "w");
    if (f) {
      fprintf(f,
              "utc_time,latitude,longitude,fix_quality,"
              "num_satellites,hdop,altitude,geoid_height\n");
      fclose(f);
      ESP_LOGI(TAG, "Created gps_points.csv with header");
    } 
    else {
      ESP_LOGE(TAG, "Failed to create gps_points.csv");
    }
  } 
  else {
    fclose(f);
  }
}

static void saveTask(void *arg){
  (void)arg;

  while (1) {
    if (saveRequestedFlag) {
      saveRequestedFlag = false;

      if (nmeaValid) {
        ESP_LOGI(TAG, "Save requested; latest NMEA: %s", latestNmea);
        storeGgaCsv(latestNmea);
        nmeaValid = false;
        memset(latestNmea, 0, sizeof(latestNmea));
        printCsvFile();
      } else {
        ESP_LOGW(TAG, "Save requested but no valid NMEA data available");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


void gpsLoggerInit(void){
  // init the spiffs filesystem
  initSpiffs();

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

  gpio_config_t io_conf = {
    .pin_bit_mask = 1ULL << GPS_BUTTON_PIN,
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = 1,
    .pull_down_en = 0,
    .intr_type    = GPIO_INTR_NEGEDGE
  };

  gpio_config(&io_conf);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(GPS_BUTTON_PIN, buttonIsrHandler, NULL);

  ESP_LOGI(TAG, "Button interrupt configured on GPIO %d", GPS_BUTTON_PIN);

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
  printCsvFile();
}

static void formatUtcTime(const char *nmeaUtc, char *out, size_t outLen)
{
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
