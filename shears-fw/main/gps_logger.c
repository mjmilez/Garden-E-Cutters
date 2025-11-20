/**
 * @file gps_logger.c
 * @brief Implementation of GPS NMEA logging for the shears firmware.
 *
 * This module:
 *  - Mounts SPIFFS and ensures a GPS CSV file exists with a header
 *  - Configures UART2 for 9600 baud NMEA input
 *  - Reads characters into a line buffer until '\n'
 *  - Maintains the most recent full NMEA sentence in latestNmea[]
 *  - Provides a "save request" flag, set by:
 *      - a physical button ISR on GPIO 23, OR
 *      - the gpsLoggerRequestSave() API
 *  - A background task consumes that flag, parses $GPGGA, and appends
 *    a CSV row into /spiffs/gps_points.csv
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

/* UART configuration used for GPS NMEA input */
#define GPS_UART_NUM   UART_NUM_2
#define GPS_UART_RX    GPIO_NUM_16
#define GPS_UART_TX    GPIO_NUM_17
#define GPS_BUF_SIZE   512

/* GPIO for the "save GPS point" button */
#define GPS_BUTTON_PIN GPIO_NUM_23

/* Module log tag */
static const char *TAG = "gps_logger";

/* Global-ish buffer storing the most recent full NMEA sentence */
static char latestNmea[GPS_BUF_SIZE];

/* Flag indicating that a save was requested (from ISR or API) */
static volatile bool saveRequestedFlag = false;

/* Forward declarations */
static void buttonIsrHandler(void *arg);
static void uartReadTask(void *arg);
static void saveTask(void *arg);
static void initSpiffs(void);
static double nmeaToDecimal(const char *nmea_val, char hemisphere);
static void storeGgaCsv(const char *nmea);
static void printCsvFile(void);

/**
 * @brief GPIO ISR for the physical "save" button.
 *
 * This ISR is intentionally minimal: it only sets a flag that a
 * background FreeRTOS task will consume.
 */
static void IRAM_ATTR buttonIsrHandler(void *arg) {
    (void)arg;
    saveRequestedFlag = true;
}

/**
 * @brief FreeRTOS task that reads raw bytes from UART2 and builds NMEA lines.
 *
 * Behavior:
 *  - Reads up to GPS_BUF_SIZE-1 bytes at a time with a timeout
 *  - Accumulates bytes into nmea_buf until '\n' is seen
 *  - On newline: terminates the string and copies it into latestNmea[]
 */
static void uartReadTask(void *arg) {
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

                /* End of line found → finalize this NMEA sentence */
                if (c == '\n') {
                    nmea_buf[nmea_len] = '\0';
                    strncpy(latestNmea, nmea_buf, GPS_BUF_SIZE);
                    nmea_len = 0;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Convert NMEA coordinate format to decimal degrees.
 *
 * NMEA format: ddmm.mmmm (lat) or dddmm.mmmm (lon)
 * Example: "2934.5678", 'N' → 29 + 34.5678/60
 */
static double nmeaToDecimal(const char *nmea_val, char hemisphere) {
    double val = atof(nmea_val);
    int degrees = (int)(val / 100);
    double minutes = val - (degrees * 100);
    double decimal = degrees + minutes / 60.0;

    if (hemisphere == 'S' || hemisphere == 'W') {
        decimal *= -1;
    }

    return decimal;
}

/**
 * @brief Parse a $GPGGA NMEA sentence and append to gps_points.csv in SPIFFS.
 *
 * If the sentence is not a $GPGGA, this function returns immediately.
 * The CSV fields are:
 *   utc_time, latitude, longitude, fix_quality, num_satellites,
 *   hdop, altitude, geoid_height
 */
static void storeGgaCsv(const char *nmea) {
    /* Only handle GPGGA sentences */
    if (strncmp(nmea, "$GPGGA,", 7) != 0) {
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

    /* Need at least enough tokens to access fields used below */
    if (i < 12) {
        ESP_LOGW(TAG, "GPGGA sentence too short, i=%d", i);
        return;
    }

    const char *utc_time = tokens[1];
    double lat          = nmeaToDecimal(tokens[2], tokens[3][0]);
    double lon          = nmeaToDecimal(tokens[4], tokens[5][0]);
    int    fix          = atoi(tokens[6]);
    int    num_sats     = atoi(tokens[7]);
    double hdop         = atof(tokens[8]);
    double altitude     = atof(tokens[9]);
    double geoid_height = atof(tokens[11]);

    FILE *f = fopen("/spiffs/gps_points.csv", "a");
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

/**
 * @brief Print contents of gps_points.csv to stdout.
 */
static void printCsvFile(void) {
    FILE *f = fopen("/spiffs/gps_points.csv", "r");
    if (!f) {
        ESP_LOGE(TAG, "Could not open CSV file for read");
        return;
    }

    ESP_LOGI(TAG, "---- GPS Data Points ----");
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }
    fclose(f);
}

/**
 * @brief Mount SPIFFS and ensure gps_points.csv exists with a header row.
 */
static void initSpiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not mount SPIFFS (err=0x%x)", ret);
        return;
    }

    ESP_LOGI(TAG, "SPIFFS mounted");

    /* Ensure the CSV file exists, creating and writing a header if needed. */
    FILE *f = fopen("/spiffs/gps_points.csv", "r");
    if (!f) {
        f = fopen("/spiffs/gps_points.csv", "w");
        if (f) {
            fprintf(f,
                    "utc_time,latitude,longitude,fix_quality,"
                    "num_satellites,hdop,altitude,geoid_height\n");
            fclose(f);
            ESP_LOGI(TAG, "Created gps_points.csv with header");
        } else {
            ESP_LOGE(TAG, "Failed to create gps_points.csv");
        }
    } else {
        fclose(f);
    }
}

/**
 * @brief Task that handles save requests (from button or API).
 *
 * Whenever saveRequestedFlag is set, this task:
 *   - Clears the flag
 *   - Calls storeGgaCsv() with the current latestNmea
 *   - Optionally prints the CSV contents (for debugging)
 */
static void saveTask(void *arg) {
    (void)arg;

    while (1) {
        if (saveRequestedFlag) {
            saveRequestedFlag = false;

            ESP_LOGI(TAG, "Save requested; latest NMEA: %s", latestNmea);
            storeGgaCsv(latestNmea);

            /* Optional: dump CSV for debugging */
            printCsvFile();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* -------- Public API -------- */

void gpsLoggerInit(void) {
    /* Mount SPIFFS and ensure CSV exists */
    initSpiffs();

    /* Configure UART2 for NMEA at 9600 baud, 8N1, no flow control */
    uart_config_t uart_config = {
        .baud_rate = 9600,
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

    ESP_LOGI(TAG, "UART2 configured for GPS at 9600 baud");

    /* Configure the button GPIO as input with internal pull-up
     * and falling-edge interrupt (button press).
     */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPS_BUTTON_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPS_BUTTON_PIN, buttonIsrHandler, NULL);

    ESP_LOGI(TAG, "Button interrupt configured on GPIO %d", GPS_BUTTON_PIN);

    /* Start UART reader task */
    xTaskCreate(uartReadTask, "gps_uart_read", 4096, NULL, 5, NULL);

    /* Start save handler task */
    xTaskCreate(saveTask, "gps_save_task", 4096, NULL, 5, NULL);
}

void gpsLoggerRequestSave(void) {
    saveRequestedFlag = true;
}

void gpsLoggerPrintCsv(void) {
    printCsvFile();
}
