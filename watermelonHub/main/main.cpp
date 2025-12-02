// main/main.cpp
#include "hub_controller.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include <string.h>

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

    // Zero initialize the entire struct
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    
    // Now set only the fields we need
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
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    
    baseLedInit();
    baseLedSetBlinking(true);

    ret = init_wifi_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
        // You could still continue without WiFi, or restart
        // esp_restart(); // Optional: restart if WiFi is critical
    } else {
        ESP_LOGI(TAG, "WiFi AP initialized successfully");
    }

    bleBaseInit([](bool connected) {
        if (connected) {
            baseLedSetSolidOn();
            ESP_LOGI(TAG, "Connected to shears!");
            bleBaseRequestLog("gps_points.csv");
        } else {
            baseLedSetBlinking(true);
            ESP_LOGI(TAG, "Disconnected from shears");
        }
    });

    // Create hub controller
    hub = new HubController();
    
    // Set components (your friends will replace these with real implementations)
    hub->setBLEHandler(createBLEHandler());
    hub->setWebServer(createWebServer());  // Comment out if not implemented yet
    hub->setDataManager(createDataManager());
    
    // Initialize and start
    if (hub->initialize()) {
        hub->start();
        
        // Create main task
        xTaskCreate(hubTask, "hub_task", 8192, NULL, 5, NULL);
        
        ESP_LOGI(TAG, "System running");
    } else {
        ESP_LOGE(TAG, "Failed to initialize hub");
    }
}
