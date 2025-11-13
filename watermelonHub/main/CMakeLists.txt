idf_component_register(
	SRCS "main.cpp" "hub_controller.cpp"
	INCLUDE_DIRS "."
	REQUIRES nvs_flash esp_wifi esp_http_server bt fatfs spiffs esp_timer
	PRIV_REQUIRES ble_handler web_server data_manager
)

