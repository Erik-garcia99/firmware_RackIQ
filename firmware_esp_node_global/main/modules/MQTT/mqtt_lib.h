#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Broker default (HiveMQ online requires TLS/SSL)
#define DEFAULT_BROKER "mqtts://ceb7d747a570414d9232ac9615bb1011.s1.eu.hivemq.cloud:8883"

// Suscripciones y Publicaciones
#define TOPIC_LED "device/led"
#define TOPIC_ADC "device/adc"
#define TOPIC_HUM "device/humi"
#define TOPIC_TEMP "device/temp"
#define TOPIC_ACT "device/action"

// UART configuration for requesting new broker
#define UART_PORT_NUM UART_NUM_1
#define UART_TX_PIN 18 // Ajusta según los pines que uses en tu ESP32 de 30 pines
#define UART_RX_PIN 19  // Ajusta según los pines que uses en tu ESP32 de 30 pines

extern esp_mqtt_client_handle_t client;
extern QueueHandle_t flow_data;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void mqtt_start(void);
void request_new_broker_uart(void);

#endif
