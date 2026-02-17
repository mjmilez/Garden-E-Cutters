# CMakeLists.txt for the base station firmware
# This defines the "main" component for ESP-IDF and tells it which
# source files to compile and which include paths to use.

idf_component_register(
    SRCS
        "main.c"
        "base_ble.c"
        "base_led.c"
        "log_transfer_client.c"
        "uart_bridge.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        bt
        esp_timer
        esp_event
        driver
        log_transfer
    PRIV_REQUIRES
        esp_wifi
)