idf_component_register(
    SRCS 
        "main.c" 
        "modules/WIFI/wifi_lib.c" 
        "modules/UART/uart_lib.c" 
        "modules/HX711/hx711_lib.c" 
        "modules/MQTT/mqtt_lib.c"
        "modules/HTTP/http_setup.c"
    INCLUDE_DIRS 
        "." 
        "modules/WIFI" 
        "modules/UART" 
        "modules/HX711" 
        "modules/MQTT"
        "modules/HTTP"
    REQUIRES 
        mqtt               # Gestionado por el Component Manager
        esp_wifi           # Fundamental para WiFi
        esp_event          # Manejo de eventos
        nvs_flash          # Almacenamiento de credenciales
        wpa_supplicant     # SOLUCIÓN al error de esp_eap_client.h
        lwip               # Stack TCP/IP
        esp_http_server
    PRIV_REQUIRES 
        esp_driver_uart    # Para driver/uart.h
        esp_driver_gpio    # Para driver/gpio.h
        esp_netif          # Interfaz de red
)