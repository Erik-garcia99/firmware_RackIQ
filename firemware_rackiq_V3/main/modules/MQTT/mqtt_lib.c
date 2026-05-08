#include "mqtt_lib.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "mqtt_client.h"
#include "global.h"        
#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT";

// #define MQTT_BROKER_URI  "mqtt://192.168.1.100:1883"   
#define MQTT_SHELF_ID    "shelf_01"                     
static char broker_uri[64] = {0};
static esp_mqtt_client_handle_t mqtt_client = NULL;
static EventGroupHandle_t wifi_event_group = NULL;
static char shelf_id[32] = MQTT_SHELF_ID;

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data);

esp_err_t mqtt_init(EventGroupHandle_t event_group, const char *broker_ip)
{
    wifi_event_group = event_group;
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:1883", broker_ip);

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .session.keepalive = 60,
        .session.disable_clean_session = false,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Fallo al inicializar MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    return ESP_OK;
}



static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Conectado al broker MQTT");
            // Publicar MAC:ACK
            uint8_t mac[6];
            esp_efuse_mac_get_default(mac);
            char payload[32];
            snprintf(payload, sizeof(payload),
                     "%02X:%02X:%02X:%02X:%02X:%02X:ACK",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            esp_mqtt_client_publish(mqtt_client, "Rackiq/broker", payload, 0, 1, 0);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado del broker");
            break;
        default:
            break;
    }
}

static void set_shelf_id_from_mac(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(shelf_id, sizeof(shelf_id), "%02x%02x%02x",
             mac[3], mac[4], mac[5]);
}



void task_mqtt_weight_publisher(void *arg)
{
    hx711_t *hx = (hx711_t *)arg;
    // float last_published_weight = -999.0f;
    char topic[64];
    char json_payload[64];
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(60000);   // 1 minuto

    // Asegurar shelf_id
    if (shelf_id[0] == '\0') {
        set_shelf_id_from_mac();
    }

    while (1) {
        if (mqtt_client == NULL) {
            vTaskDelay(period);
            continue;
        }

        float weight;
        if (hx711_is_stable(hx)) {
            weight = hx711_get_stable_weight(hx);
        } else {
            weight = hx711_get_weight(hx);
        }

        //construri el JSON 

        // Construir JSON manualmente
        int len = snprintf(json_payload, sizeof(json_payload),"{\"weight\":%.2f}", weight);
        if (len < 0 || len >= sizeof(json_payload)) {
            ESP_LOGE(TAG, "Error al crear payload JSON");
            continue;
        }

        // Publicar
        snprintf(topic, sizeof(topic), "rackiq/shelf/%s/weight", shelf_id);
        esp_mqtt_client_publish(mqtt_client, topic, json_payload, 0, 1, 0);
        vTaskDelayUntil(&last_wake_time, period);
    }
}

void task_mqtt_heartbeat(void *arg)
{
    char topic[64];
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(30000);

    if (shelf_id[0] == '\0') {
        set_shelf_id_from_mac();
    }

    while (1) {
        if (mqtt_client != NULL) {
            snprintf(topic, sizeof(topic), "rackiq/shelf/%s/heartbeat", shelf_id);
            esp_mqtt_client_publish(mqtt_client, topic, "alive", 0, 1, 0);
        }
        vTaskDelayUntil(&last_wake_time, period);
    }
}