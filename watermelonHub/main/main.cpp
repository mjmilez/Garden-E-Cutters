// main/main.cpp
#include "hub_controller.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include <string.h>

// BLE and LED headers (C code)
extern "C" {
    #include "base_ble.h"
    #include "base_led.h"
    #include "log_paths.h"
}

static const char* TAG = "MAIN";

esp_err_t init_wifi_ap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    strcpy((char*)wifi_config.ap.ssid, "WatermelonHub");
    strcpy((char*)wifi_config.ap.password, "harvest123");
    wifi_config.ap.ssid_len = strlen("WatermelonHub");
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. Connect to 'WatermelonHub'");
    ESP_LOGI(TAG, "Browse to: http://192.168.4.1");

    return ESP_OK;
}

// BLE connection callback - called when shears connect/disconnect
void bleConnectionCallback(bool connected) {
    if (connected) {
        ESP_LOGI(TAG, "=== SHEARS CONNECTED ===");
        baseLedSetSolidOn();

        // Request GPS log file from shears
        esp_err_t err = bleBaseRequestLog(GPS_LOG_FILE_BASENAME);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to request log: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Requested log file: %s", GPS_LOG_FILE_BASENAME);
        }
    } else {
        ESP_LOGI(TAG, "=== SHEARS DISCONNECTED ===");
        baseLedSetBlinking(true);
    }
}

// Component factory functions (defined in each component)
extern IBLEHandler* createBLEHandler();
extern IWebServer* createWebServer();
extern IDataManager* createDataManager();

HubController* hub = nullptr;

// Task that runs the main hub logic
void hubTask(void* pvParameters) {
    while(1) {
        hub->processIncomingData();
        hub->updateWebInterface();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "  Watermelon Hub - Starting Up");
    ESP_LOGI(TAG, "=========================================");

    // Initialize NVS (required for WiFi and BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    ESP_LOGI(TAG, "SPIFFS initialized");

    // Initialize WiFi AP
    ret = init_wifi_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi AP initialized successfully");
    }

    // Initialize status LED
    ESP_LOGI(TAG, "Initializing status LED...");
    baseLedInit();
    baseLedSetBlinking(true);  // Blink while scanning

    // Initialize BLE central (scans for WM-SHEARS)
    ESP_LOGI(TAG, "Initializing BLE central...");
    bleBaseInit(bleConnectionCallback);

    // Create hub controller
    hub = new HubController();

    // Set components
    hub->setBLEHandler(createBLEHandler());
    hub->setWebServer(createWebServer());
    hub->setDataManager(createDataManager());

    // Initialize and start
    if (hub->initialize()) {
        hub->start();

        // Create main task
        xTaskCreate(hubTask, "hub_task", 8192, NULL, 5, NULL);

        ESP_LOGI(TAG, "=========================================");
        ESP_LOGI(TAG, "  Hub running!");
        ESP_LOGI(TAG, "  WiFi: WatermelonHub @ 192.168.4.1");
        ESP_LOGI(TAG, "  BLE: Scanning for WM-SHEARS...");
        ESP_LOGI(TAG, "  LED: Blinking=scanning, Solid=connected");
        ESP_LOGI(TAG, "=========================================");
    } else {
        ESP_LOGE(TAG, "Failed to initialize hub");
    }
}
