// main/main.cpp
#include "hub_controller.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

static const char* TAG = "MAIN";

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
