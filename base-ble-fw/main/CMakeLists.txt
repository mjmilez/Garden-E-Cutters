# CMakeLists.txt for the base station firmware
# This defines the "main" component for ESP-IDF and tells it which
# source files to compile and which include paths to use.

idf_component_register(
    SRCS
        "main.c"       # high-level wiring: LED + BLE init
        "base_led.c"   # status LED task and control API
        "base_ble.c"   # BLE central logic: scan + connect to WM-SHEARS
        "csv_debug_button.c"
        "base_uartFileTransfer.c"
        "log_transfer_client.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        bt
        esp_timer
        esp_event
        esp_driver_gpio
        esp_driver_uart
        log_transfer
    PRIV_REQUIRES
        esp_wifi
        spiffs
)
