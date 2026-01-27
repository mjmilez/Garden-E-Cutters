idf_component_register(
    SRCS 
        "main.cpp" 
        "hub_controller.cpp"
        "base_ble.c"
        "base_led.c"
        "log_transfer_client.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_http_server bt fatfs spiffs esp_timer esp_event
    PRIV_REQUIRES ble_handler web_server data_manager log_transfer
)


