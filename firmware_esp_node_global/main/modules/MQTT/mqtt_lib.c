#include <stdio.h>
#include <string.h>

// Librerías de FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include "esp_event.h"
#include "driver/uart.h"

// MQTT
#include <mqtt_client.h>

// Librerías propias
#include "mqtt_lib.h"
#include "global.h"
#include "uart_lib.h"

static const char *TAG = "MQTT_LIB";

esp_mqtt_client_handle_t client = NULL;
QueueHandle_t flow_data = NULL; // Asegúrate de inicializarla en main.c

static int retry_count = 0;
static const int MAX_RETRIES = 5;

// Manejador de eventos para MQTT
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            retry_count = 0; // Resetear contador al conectar exitosamente
            esp_mqtt_client_subscribe(client, TOPIC_ACT, 0);
            ESP_LOGI(TAG, "Subscribed to topic %s", TOPIC_ACT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            retry_count++;
            if (retry_count >= MAX_RETRIES) {
                ESP_LOGE(TAG, "Max MQTT retries reached. Requesting new broker via UART...");
                esp_mqtt_client_stop(client);
                request_new_broker_uart();
            }
            break;
            
        case MQTT_EVENT_DATA: {
            char topic[128] = {0};
            char data[256]  = {0};

            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            snprintf(data,  sizeof(data),  "%.*s", event->data_len,  event->data);

            ESP_LOGI(TAG, "Topic : %s", topic);
            ESP_LOGI(TAG, "Data  : %s", data);

            if (flow_data != NULL) {
                uint16_t length = event->topic_len + event->data_len;
                char *msg = malloc(length + 2);
                if (msg) {
                    snprintf(msg, length + 2, "%s:%s", topic, data);
                    xQueueSend(flow_data, &msg, portMAX_DELAY);
                }
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

void request_new_broker_uart(void) {
    
    // Si el UART no está inicializado, usamos tu función uart_init()
    if (!uart_is_driver_installed(UART_PORT_NUM)) {
        uart_init(UART_PORT_NUM, 115200, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1, UART_TX_PIN, UART_RX_PIN);
    }

    const char* req_msg = "REQ_BRK\n";
    uint8_t data[256];
    bool broker_received = false;

    while (!broker_received) {
        // Enviar petición
        uart_write_bytes(UART_PORT_NUM, req_msg, strlen(req_msg));
        ESP_LOGI(TAG, "Sent UART request: REQ_BRK");

        // Esperar respuesta: <ACK:SET_BRK:<direccion_o_ip>>
        int len = uart_read_bytes(UART_PORT_NUM, data, sizeof(data) - 1, pdMS_TO_TICKS(3000));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGI(TAG, "Received UART Data: %s", (char*)data);

            char *start = strstr((char*)data, "<ACK:SET_BRK:");
            if (start != NULL) {
                start += 13; // Avanzar después de <ACK:SET_BRK:
                char *end = strchr(start, '>');
                if (end != NULL) {
                    *end = '\0';
                    char new_broker_uri[128];
                    
                    // Comprobar si tiene el prefijo mqtt://
                    if (strncmp(start, "mqtt://", 7) == 0) {
                        snprintf(new_broker_uri, sizeof(new_broker_uri), "%s", start);
                    } else {
                        snprintf(new_broker_uri, sizeof(new_broker_uri), "mqtt://%s:1883", start);
                    }
                    
                    ESP_LOGI(TAG, "New Broker Setting: %s", new_broker_uri);

                    // Reconfigurar y reiniciar MQTT
                    esp_mqtt_client_set_uri(client, new_broker_uri);
                    retry_count = 0; // Reiniciar cuenta
                    esp_mqtt_client_start(client);
                    
                    broker_received = true;
                }
            }
        }
        
        if (!broker_received) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void mqtt_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = DEFAULT_BROKER,
        .credentials.username = "rackiq",
        .credentials.authentication.password = "8Og7ulMHHBY9O",
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
