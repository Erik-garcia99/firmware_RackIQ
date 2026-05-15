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
#include "mdns.h"

static const char *TAG = "MQTT";

// #define MQTT_BROKER_URI  "mqtt://192.168.1.100:1883"   
#define MQTT_SHELF_ID    "shelf_01"                     
static char broker_uri[64] = {0};
static esp_mqtt_client_handle_t mqtt_client = NULL;
static EventGroupHandle_t wifi_event_group = NULL;
static char shelf_id[32] = MQTT_SHELF_ID;

static bool mqtt_started = false;

static void mqtt_event_handler(void *arg, esp_event_base_t base,int32_t event_id, void *event_data);

void task_broker_finder(void *arg);


esp_err_t find_rackiq_broker_via_mdns(char *broker_ip, size_t ip_len);



esp_err_t mqtt_init(EventGroupHandle_t event_group, const char *broker_ip)
{
    if (mqtt_started) return ESP_OK;  

    wifi_event_group = event_group;
    snprintf(broker_uri, sizeof(broker_uri), "mqtt://%s:1883", broker_ip);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .session.keepalive  = 60,
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
    mqtt_started = true;
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

            esp_mqtt_client_subscribe(mqtt_client, "rackiq/shelf/+/calibrate/tare", 1);
            esp_mqtt_client_subscribe(mqtt_client, "rackiq/shelf/+/calibrate/scale", 1);

            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado del broker");
            break;

        case MQTT_EVENT_DATA: {
            esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
            char topic[128];
            char data[64];
            strncpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';
            strncpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';

            // Extraer mqtt_id del topic (rackiq/shelf/[mqtt_id]/calibrate/...)
            char mqtt_id[32] = {0};
            if (sscanf(topic, "rackiq/shelf/%[^/]/calibrate/tare", mqtt_id) == 1) {
                // Comando TARE
                extern hx711_t hx;
                hx711_suspend_task(&hx);
                hx711_tare(&hx, 20);
                hx711_resume_task(&hx);
                // Publicar resultado
                char resp_topic[64] = "rackiq/calibration/result";
                char resp_payload[128];
                snprintf(resp_payload, sizeof(resp_payload), 
                        "{\"mqtt_id\":\"%s\",\"status\":\"tare_done\",\"offset\":%ld}", 
                        mqtt_id, hx.offset);
                esp_mqtt_client_publish(mqtt_client, resp_topic, resp_payload, 0, 1, 0);
                ESP_LOGI(TAG, "Tare completada para %s", mqtt_id);
            }
            else if (sscanf(topic, "rackiq/shelf/%[^/]/calibrate/scale", mqtt_id) == 1) {
                // Comando SCALE
                float ref_weight = 0;
                if (sscanf(data, "{\"weight\":%f}", &ref_weight) == 1 && ref_weight > 0) {
                    extern hx711_t hx;
                    hx711_suspend_task(&hx);
                    int32_t raw = hx711_read_average(&hx, 20);
                    float new_scale = (raw - hx.offset) / ref_weight;
                    hx711_set_scale(&hx, new_scale);
                    hx711_save_calibration(&hx);
                    hx711_resume_task(&hx);
                    char resp_payload[256];
                    snprintf(resp_payload, sizeof(resp_payload),
                            "{\"mqtt_id\":\"%s\",\"status\":\"scale_done\",\"new_scale\":%.4f,\"offset\":%ld}",
                            mqtt_id, new_scale, hx.offset);
                    esp_mqtt_client_publish(mqtt_client, "rackiq/calibration/result", resp_payload, 0, 1, 0);
                    ESP_LOGI(TAG, "Calibración completada para %s, nueva escala: %.4f", mqtt_id, new_scale);
                } else {
                    ESP_LOGW(TAG, "Peso de referencia inválido para %s: %s", mqtt_id, data);
                }
            }
            break;
        }
        
        default:
            break;
    }
}

static void set_shelf_id_from_mac(void)
{
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    // Usar últimos 3 bytes de la MAC (6 caracteres hex)
    snprintf(shelf_id, sizeof(shelf_id), "%02x%02x%02x", mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "mqtt_id generado: %s", shelf_id);
}

// En mqtt_lib.c — agregar después de set_shelf_id_from_mac()

static bool hx711_pin_has_sensor(gpio_num_t dout_pin) {
    // Configurar pin como input y ver si el HX711 baja la línea
    gpio_set_direction(dout_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(dout_pin, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50));
    // Si el pin está en LOW el HX711 está listo (no flotando)
    return gpio_get_level(dout_pin) == 0;
}

void task_mqtt_pin_status_publisher(void *arg) {
    char topic[64];
    char payload[128];
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(60000); // cada 60s

    if (shelf_id[0] == '\0') set_shelf_id_from_mac();

    // Los 5 pines DOUT definidos en global.h
    const gpio_num_t dout_pins[] = {
        DOUT_PIN,      // GPIO 4
        DOUT_HX711_2,  // GPIO 22
        DOUT_HX711_3,  // GPIO 23
        DOUT_HX711_4,  // GPIO 12
        DOUT_HX711_5   // GPIO 14
    };
    const int pin_nums[] = {4, 22, 23, 12, 14};
    const int n_pins = 5;

    while (1) {
        if (mqtt_client == NULL) {
            vTaskDelay(period);
            continue;
        }

        // Construir JSON con estado real de cada pin
        int offset = 0;
        offset += snprintf(payload + offset, sizeof(payload) - offset, "{");
        for (int i = 0; i < n_pins; i++) {
            bool active = hx711_pin_has_sensor(dout_pins[i]);
            offset += snprintf(payload + offset, sizeof(payload) - offset,
                               "\"pin_%d\":%s%s",
                               pin_nums[i],
                               active ? "true" : "false",
                               i < n_pins - 1 ? "," : "");
        }
        snprintf(payload + offset, sizeof(payload) - offset, "}");

        snprintf(topic, sizeof(topic), "rackiq/shelf/%s/pin_status", shelf_id);
        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "pin_status publicado: %s", payload);

        vTaskDelayUntil(&last_wake, period);
    }
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


// En mqtt_lib.h también declara: void task_broker_finder(void *arg);

void task_broker_finder(void *arg)
{
    extern char g_broker_ip[16];
    extern EventGroupHandle_t s_wifi_event_group;

    // Esperar WiFi antes de hacer nada
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        // Si ya tenemos broker corriendo, esta tarea ya no hace nada
        if (mqtt_started) {
            vTaskDelete(NULL);
            return;
        }

        // 1. Intentar cargar IP guardada en NVS
        bool found = false;
        if (g_broker_ip[0] != '\0') {
            found = true;
            uart_write_bytes(UART_NUM_0,
                "[BROKER] Usando IP guardada en NVS\r\n", 38);
            ESP_LOGI(TAG, "Usando broker IP de NVS/memoria: %s", g_broker_ip);
        }

        // 2. Si no hay IP, buscar por mDNS
        if (!found) {
            uart_write_bytes(UART_NUM_0,
                "[BROKER] Buscando Rackiq-broker via mDNS...\r\n", 46);

            if (mdns_init() == ESP_OK) {
                char ip_str[16] = {0};
                esp_err_t err = find_rackiq_broker_via_mdns(ip_str, sizeof(ip_str));
                mdns_free();

                if (err == ESP_OK && ip_str[0] != '\0') {
                    strncpy(g_broker_ip, ip_str, sizeof(g_broker_ip));
                    nvs_save_str("mqtt_broker", g_broker_ip);
                    char msg[64];
                    snprintf(msg, sizeof(msg),
                             "[BROKER] Encontrado via mDNS: %s\r\n", g_broker_ip);
                    uart_write_bytes(UART_NUM_0, msg, strlen(msg));
                    found = true;
                } else {
                    uart_write_bytes(UART_NUM_0,
                        "[BROKER] No encontrado via mDNS. Reintentando en 30s...\r\n", 60);
                }
            } else {
                uart_write_bytes(UART_NUM_0,
                    "[BROKER] mDNS init falló. Reintentando en 30s...\r\n", 52);
            }
        }

        // 3. Si tenemos IP, iniciar MQTT
        if (found && g_broker_ip[0] != '\0') {
            char msg[80];
            snprintf(msg, sizeof(msg),
                     "[BROKER] Conectando a MQTT en %s:1883...\r\n", g_broker_ip);
            uart_write_bytes(UART_NUM_0, msg, strlen(msg));
            
            if (mqtt_init(s_wifi_event_group, g_broker_ip) == ESP_OK) {
                uart_write_bytes(UART_NUM_0,
                    "[BROKER] ✓ MQTT iniciado\r\n", 27);
                vTaskDelete(NULL);
                return;
            } else {
                uart_write_bytes(UART_NUM_0,
                    "[BROKER] Error al iniciar MQTT. Reintentando en 30s...\r\n", 58);
            }
        }

        // Esperar 30 segundos antes del siguiente intento
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}




esp_err_t find_rackiq_broker_via_mdns(char *broker_ip, size_t ip_len)
{
    // Primero, intentar resolver el hostname de la RPI por DNS (raspberrypi.local o rasberry-dmrx.local)
    const char *rpi_hostnames[] = {
        "rasberry-dmrx.local",
        "raspberrypi.local",
        "rackiq-gateway.local"
    };
    
    esp_ip4_addr_t addr;
    
    for (int i = 0; i < sizeof(rpi_hostnames) / sizeof(rpi_hostnames[0]); i++) {
        uart_write_bytes(UART_NUM_0, 
            "[BROKER] Resolviendo hostname ", 30);
        uart_write_bytes(UART_NUM_0, (uint8_t*)rpi_hostnames[i], strlen(rpi_hostnames[i]));
        uart_write_bytes(UART_NUM_0, "...\r\n", 5);
        
        esp_err_t err = mdns_query_a(rpi_hostnames[i], 3000, &addr);
        if (err == ESP_OK && addr.addr != 0) {
            snprintf(broker_ip, ip_len, IPSTR, IP2STR(&addr));
            char msg[64];
            snprintf(msg, sizeof(msg), "[BROKER] ✓ Resolvió a: %s\r\n", broker_ip);
            uart_write_bytes(UART_NUM_0, (uint8_t*)msg, strlen(msg));
            return ESP_OK;
        }
    }
    
    // Si no se resuelven hostnames, intentar buscar por servicio mDNS _http._tcp
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_srv("Rackiq-broker", "_http", "_tcp", 3000, &results);
    if (err != ESP_OK || results == NULL) {
        uart_write_bytes(UART_NUM_0,
            "[BROKER] No encontrado por hostname ni por servicio\r\n", 55);
        return ESP_FAIL;
    }

    bool found = false;
    for (mdns_result_t *r = results; r != NULL; r = r->next) {
        if (r->instance_name && strcasecmp(r->instance_name, "Rackiq-broker") == 0) {
            if (r->hostname) {
                err = mdns_query_a(r->hostname, 3000, &addr);
                if (err == ESP_OK && addr.addr != 0) {
                    snprintf(broker_ip, ip_len, IPSTR, IP2STR(&addr));
                    found = true;
                    break;
                }
            }
        }
    }
    mdns_query_results_free(results);
    return found ? ESP_OK : ESP_FAIL;
}
