#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "modules/HX711/hx711_lib.h"
/**
 * @brief Initializes MQTT client and connects to broker.
 *
 * @param event_group  Event group used to wait for WiFi connection.
 * @param broker_ip    IP address of the MQTT broker (e.g., "192.168.1.100").
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t mqtt_init(EventGroupHandle_t event_group, const char *broker_ip);

/**
 * @brief Tarea que publica periódicamente el peso estable cuando cambia.
 *
 * @param arg  Puntero a la estructura hx711_t (se castea dentro).
 *
 * Lee el peso filtrado y estable. Si hay un cambio respecto al último publicado,
 * envía un mensaje JSON al tópico `rackiq/shelf/<id>/weight`.
 * También envía un heartbeat en cada ciclo.
 */
void task_mqtt_weight_publisher(void *arg);

/**
 * @brief Tarea que envía un heartbeat simple cada 30 segundos.
 *
 * @param arg  No usado (NULL).
 *
 * Publica un mensaje en `rackiq/shelf/<id>/heartbeat` con contenido "alive".
 */
void task_mqtt_heartbeat(void *arg);

#endif