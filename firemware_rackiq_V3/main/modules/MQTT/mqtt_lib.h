#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "modules/HX711/hx711_lib.h"

/**
 * @brief Inicializa el cliente MQTT y arranca la conexión al broker local.
 *
 * @param event_group  Grupo de eventos usado para señalizar WiFi (se usa internamente para esperar la IP).
 *
 * @note  Debe llamarse después de que el WiFi esté conectado (WIFI_CONNECTED_BIT).
 */
esp_err_t mqtt_init(EventGroupHandle_t event_group);

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