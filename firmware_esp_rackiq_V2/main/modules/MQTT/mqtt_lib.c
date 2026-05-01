#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "global.h"
#include "mqtt_lib.h"

static const char *TAG = "MQTT_LIB";

esp_mqtt_client_handle_t mqtt_client = NULL;

// Manejador de eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Conectado a HiveMQ");
            // Aquí puedes suscribirte a topics si la Raspberry necesita mandar comandos al ESP32
            // esp_mqtt_client_subscribe(mqtt_client, "rackiq/comandos/#", 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Desconectado");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Mensaje publicado con éxito, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error MQTT");
            break;
        default:
            ESP_LOGI(TAG, "Otro evento MQTT: %d", event->event_id);
            break;
    }
}

void mqtt_app_start(void) {
    // Configuración para HiveMQ Cloud
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "",
        .credentials.username = "", 
        .credentials.authentication.password = "", 

    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    // Registramos el handler de eventos
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void mqtt_publish_weight(int shelf_id, float weight_kg) {
    if (mqtt_client == NULL) return;

    char topic[64];
    char payload[64];

    // Formateamos el topic y el payload (simulando una MAC/ID harcodeada por ahora)
    snprintf(topic, sizeof(topic), "rackiq/branch/NORTE/node/ESP32_01/shelf/%d/weight", shelf_id);
    snprintf(payload, sizeof(payload), "{\"weight_kg\": %.3f}", weight_kg);

    // Publicamos con QoS 1 para asegurar entrega
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Publicando peso: %s -> %s (msg_id: %d)", topic, payload, msg_id);
}
