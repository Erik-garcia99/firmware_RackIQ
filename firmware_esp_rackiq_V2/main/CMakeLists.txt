idf_component_register(
    SRCS "main.c" "modules/WIFI/wifi_lib.c" "modules/UART/uart_lib.c" "modules/HX711/hx711.c" "modules/MQTT/mqtt_lib.c"
    INCLUDE_DIRS "." "modules/WIFI" "modules/UART" "modules/HX711" "modules/MQTT"
    REQUIRES mqtt esp_wifi nvs_flash esp_driver_uart wpa_supplicant esp_driver_gpio
)