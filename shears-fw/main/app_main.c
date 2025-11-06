#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void app_main(void) {
    ESP_LOGI("MAIN", "Shears firmware initialized (v1 skeleton)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
