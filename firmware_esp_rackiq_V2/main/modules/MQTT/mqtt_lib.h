#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include<mqtt_client.h>
#include "esp_err.h"

// Exponemos el cliente para poder publicar desde otras tareas
extern esp_mqtt_client_handle_t mqtt_client;

void mqtt_app_start(void);
void mqtt_publish_weight(int shelf_id, float weight_kg);
void mqtt_publish_heartbeat(void);

#endif