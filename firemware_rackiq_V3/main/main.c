
/**
 * @author: erik garcia chavez - ingenieria con computacion
 * @date: 23 de abril del 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// FreeRTOS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

// drivers
#include <driver/uart.h>
#include<driver/gpio.h>

// logs
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <nvs.h>

// wifi
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <esp_http_server.h>

//dns
#include "mdns.h"
#include "esp_mac.h"
#include "esp_log.h"


// librerías propias
#include "global.h"
#include "modules/WIFI/wifi_lib.h"
#include "modules/UART/uart_lib.h"
#include "modules/HX711/hx711_lib.h"
#include "modules/MQTT/mqtt_lib.h"
#include "modules/HTTP/http_setup.h"








//+++++++++++++++++++++++++++++++colas 
QueueHandle_t flow_data_queue;
QueueHandle_t uart_event;
QueueHandle_t uart1_tx_queue;


//+++++++++++++++++++++++++++++++grupos de deventos 
EventGroupHandle_t s_wifi_event_group;
esp_wifi_t         esp_wifi;
hx711_t            hx;

//+++++++++++++++++++++++++++++++varibales globales 

// static task_uart_port_t user_uart;
float g_cal_weight_kg = 0.0f;
uint8_t g_device_role = 0;  




//+++++++++++++++++++++++++++++++estructuras 

static const wifi_default_profile_t default_profiles[] = {
    { "DFT_1", "INFINITUMF4AF", "nFukH34MPW" },
};
#define DEFAULT_PROFILES_COUNT (sizeof(default_profiles) / sizeof(default_profiles[0]))

static const wifi_default_ent_profile_t default_ent_profiles[] = {
    { "DFT_ENT_1", "UABC-5G", "erik.garcia.chavez", "MarKI?1T04A" },
};
#define DEFAULT_ENT_PROFILES_COUNT (sizeof(default_ent_profiles) / sizeof(default_ent_profiles[0]))



//+++++++++++++++++++++++++++++++Credenciales WiFi / Broker preparadas
char g_wifi_ssid[33] = {0};
char g_wifi_pass[65] = {0};
char g_broker_ip[16]  = {0};   // solo la IP, luego se arma la URI



//+++++++++++++++++++++++++++++++funciones 

char    **parse_input(char *line);
esp_err_t update_setup_cred(char *key, char *anchor, char *pswd_ent, char *identificator);
// static esp_err_t resolve_mdns_host(const char *hostname, esp_ip4_addr_t *out_addr);
static esp_err_t find_rackiq_broker_via_mdns(char *broker_ip, size_t ip_len);

//+++++++++++++++++++++++++++++++ NVS helpers
esp_err_t nvs_save_str(const char *key, const char *value);
esp_err_t nvs_load_str(const char *key, char *buf, size_t len);
// static void      nvs_erase_key(const char *key);



//+++++++++++++++++++++++++++++++tareas
void      task_cmd_uart(void *params);
void      task_hx711_uart(void *params);
void      task_hx711_calibrate(void *params);



void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Detectar rol por GPIO
    gpio_set_direction(ROLE_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(ROLE_PIN);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (gpio_get_level(ROLE_PIN) == 0) {
        g_device_role = ROLE_SLAVE;
        printf("\r\n[ROLE] SLAVE\r\n");
    } else {
        g_device_role = ROLE_MASTER;
        printf("\r\n[ROLE] MASTER\r\n");
    }

    // Colas
    flow_data_queue    = xQueueCreate(10, sizeof(char *));
    uart1_tx_queue     = xQueueCreate(5,  sizeof(char *));

    // UART0 (debug)
    QueueHandle_t uart0_evt;
    uart_init(UART_NUM_0, 115200, UART_DATA_8_BITS, UART_PARITY_DISABLE,
              UART_STOP_BITS_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, &uart0_evt);
    uart_task_config_t uart0_cfg = { .port = UART_NUM_0, .event_queue = uart0_evt };
    xTaskCreate(task_uart, "task_uart", 4096, &uart0_cfg, 9, NULL);

    // UART1 (solo usado por master para SET_WIFI)
    QueueHandle_t uart1_evt;
    uart_init(UART_NUM_1, 115200, UART_DATA_8_BITS, UART_PARITY_DISABLE,
              UART_STOP_BITS_1, 18, 19, &uart1_evt);
    uart_task_config_t uart1_cfg = { .port = UART_NUM_1, .event_queue = uart1_evt };
    xTaskCreate(task_uart, "task_uart1", 4096, &uart1_cfg, 8, NULL);

    xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL, 8, NULL);

    // Grupo de eventos WiFi
    s_wifi_event_group = xEventGroupCreate();

    // HX711
    hx711_init(&hx, DOUT_PIN, PD_SCK_PIN);
    if (hx711_load_calibration(&hx)) {
        char msg[90];
        int  len = snprintf(msg, sizeof(msg),
                            "\r\n[HX711] Calibracion cargada: offset=%ld  scale=%.4f\r\n",
                            (long)hx.offset, hx.scale);
        uart_write_bytes(UART_NUM_0, msg, len);
        hx711_start_task(&hx);
    } else {
        xEventGroupSetBits(s_wifi_event_group, HX711_PAUSE_PRINT);
        char msg[] = "\r\n[HX711] Sin calibracion guardada.\r\n"
                     "Coloque la estructura/soporte vacio sobre la celda\r\n"
                     "y envie: TARE\r\n";
        uart_write_bytes(UART_NUM_0, msg, sizeof(msg) - 1);
        xTaskCreate(task_hx711_calibrate, "hx711_cal", 4096, &hx, 6, NULL);
    }

    // ─── Carga/obtención de credenciales WiFi ────────────────
    bool creds_in_nvs = false;
    if (nvs_load_str("wifi_ssid", g_wifi_ssid, sizeof(g_wifi_ssid)) == ESP_OK &&
        nvs_load_str("wifi_pass", g_wifi_pass, sizeof(g_wifi_pass)) == ESP_OK) {
        creds_in_nvs = true;
    }

    if (!creds_in_nvs) {
        // AP + página web
        char msg[] = "\r\n[WIFI] Sin credenciales guardadas. Iniciando punto de acceso...\r\n";
        uart_write_bytes(UART_NUM_0, msg, sizeof(msg)-1);
        wifi_init_ap();
        start_http_setup_server(s_wifi_event_group);
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CREDS_RECEIVED,
                            pdTRUE, pdFALSE, portMAX_DELAY);
        stop_http_setup_server();
        wifi_stop_ap();

        // Si es master, envía SET_WIFI a la Raspberry Pi por UART1
        if (g_device_role == ROLE_MASTER) {
            char set_wifi_cmd[128];
            snprintf(set_wifi_cmd, sizeof(set_wifi_cmd),
                     "SET_WIFI:%s:%s", g_wifi_ssid, g_wifi_pass);
            char *cmd_to_send = strdup(set_wifi_cmd);
            xQueueSend(uart1_tx_queue, &cmd_to_send, portMAX_DELAY);
            xEventGroupWaitBits(s_wifi_event_group, BROKER_RECEIVED,
                                pdTRUE, pdFALSE, portMAX_DELAY);
            xEventGroupClearBits(s_wifi_event_group, BROKER_RECEIVED);
            uart_write_bytes(UART_NUM_0, "[UART1] ACK recibido\r\n", 26);
        }
    }

    // Conectar a WiFi en modo estación
    update_setup_cred(g_wifi_ssid, g_wifi_pass, NULL, "SSID");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
    wifi_init_sta();
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    // ─── Descubrimiento del broker MQTT (mDNS para ambos roles) ──
    if (nvs_load_str("mqtt_broker", g_broker_ip, sizeof(g_broker_ip)) != ESP_OK) {
        esp_err_t mdns_err = mdns_init();
        if (mdns_err == ESP_OK) {
            uart_write_bytes(UART_NUM_0, "[MDNS] Buscando servicio Rackiq-broker vía mDNS...\r\n", 56);
            char ip_str[16] = {0};
            mdns_err = find_rackiq_broker_via_mdns(ip_str, sizeof(ip_str));
            if (mdns_err == ESP_OK && ip_str[0] != '\0') {
                strncpy(g_broker_ip, ip_str, sizeof(g_broker_ip));
                nvs_save_str("mqtt_broker", g_broker_ip);
                char msg[60];
                int len = snprintf(msg, sizeof(msg),
                                   "\r\n[MDNS] Broker encontrado: %s\r\n", g_broker_ip);
                uart_write_bytes(UART_NUM_0, msg, len);
            } else {
                uart_write_bytes(UART_NUM_0, "[MDNS] No se encontró Rackiq-broker\r\n", 38);
            }
            mdns_free();
        } else {
            uart_write_bytes(UART_NUM_0, "[MDNS] Error al inicializar mDNS\r\n", 36);
        }
    }

    // Inicializar MQTT
    mqtt_init(s_wifi_event_group, g_broker_ip);

    // Tareas de publicación
    xTaskCreate(task_hx711_uart, "task_hx711_uart", 4096, NULL, 5, NULL);
    xTaskCreate(task_mqtt_weight_publisher, "mqtt_weight", 4096, &hx, 5, NULL);
    xTaskCreate(task_mqtt_heartbeat,        "mqtt_heartbeat", 2048, NULL, 4, NULL);
}


static esp_err_t find_rackiq_broker_via_mdns(char *broker_ip, size_t ip_len)
{
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_srv("Rackiq-broker", "_http", "_tcp", 3000, &results);
    if (err != ESP_OK || results == NULL) {
        return ESP_FAIL;
    }

    bool found = false;
    for (mdns_result_t *r = results; r != NULL; r = r->next) {
        if (r->instance_name && strcasecmp(r->instance_name, "Rackiq-broker") == 0) {
            if (r->hostname) {
                esp_ip4_addr_t addr;
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

// NVS helpers 


esp_err_t nvs_save_str(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_load_str(const char *key, char *buf, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    size_t required = len;
    err = nvs_get_str(handle, key, buf, &required);
    nvs_close(handle);
    return err;
}



void task_hx711_calibrate(void *params)
{
    hx711_t *hx_p = (hx711_t *)params;
    char msg[160];
    int  len;
    xEventGroupWaitBits(s_wifi_event_group,
                        HX711_CAL_TARE_READY,
                        pdTRUE, pdFALSE, portMAX_DELAY);

    hx711_suspend_task(hx_p);
    hx711_tare(hx_p, 20);

    len = snprintf(msg, sizeof(msg),
                   "\r\n[HX711] Tara ok (offset=%ld).\r\n"
                   "Coloque el peso de referencia y envie:\r\n"
                   "CAL:<peso_kg>   ej: CAL:1.000\r\n",
                   (long)hx_p->offset);
    uart_write_bytes(UART_NUM_0, msg, len);
    xEventGroupWaitBits(s_wifi_event_group,
                        HX711_CAL_WEIGHT_OK,
                        pdTRUE, pdFALSE, portMAX_DELAY);

    if (g_cal_weight_kg <= 0.0f) {
        len = snprintf(msg, sizeof(msg),
                       "\r\n[HX711] ERROR: peso invalido. Reinicia y vuelve a calibrar.\r\n");
        uart_write_bytes(UART_NUM_0, msg, len);
        if (hx_p->task_handle == NULL) hx711_start_task(hx_p);
        else                           hx711_resume_task(hx_p);
        xEventGroupClearBits(s_wifi_event_group, HX711_PAUSE_PRINT);
        vTaskDelete(NULL);
        return;
    }
    int32_t raw_ref = hx711_read_average(hx_p, 20);
    float   scale   = (float)(raw_ref - hx_p->offset) / g_cal_weight_kg;

    hx711_set_scale(hx_p, scale);
    hx711_save_calibration(hx_p);

    xSemaphoreTake(hx_p->mutex, portMAX_DELAY);
    hx_p->weight = 0.0f;
    hx_p->last_weight = 0.0f;
    hx_p->stable_weight = 0.0f;
    xSemaphoreGive(hx_p->mutex);

    len = snprintf(msg, sizeof(msg),
                   "\r\n[HX711] Calibracion lista!\r\n"
                   "  Referencia : %.3f kg\r\n"
                   "  raw_ref    : %ld\r\n"
                   "  offset     : %ld\r\n"
                   "  Escala     : %.2f cnt/kg\r\n"
                   "  Guardado en NVS.\r\n\r\n",
                   g_cal_weight_kg, (long)raw_ref, (long)hx_p->offset, scale);
    uart_write_bytes(UART_NUM_0, msg, len);

    if (hx_p->task_handle == NULL) hx711_start_task(hx_p);
    else                           hx711_resume_task(hx_p);

    xEventGroupClearBits(s_wifi_event_group, HX711_PAUSE_PRINT);
    vTaskDelete(NULL);
}
void task_hx711_uart(void *params)
{
    char msg[100];
    int  len;

    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        
        if (!(bits & HX711_PAUSE_PRINT)) {
            if (hx711_is_stable(&hx)) {
                len = snprintf(msg, sizeof(msg), "STABLE: %.3f kg\r\n",
                               hx711_get_stable_weight(&hx));
            } else {
                len = snprintf(msg, sizeof(msg), "RAW:    %.3f kg\r\n",
                               hx711_get_weight(&hx));
            }
            uart_write_bytes(UART_NUM_0, msg, len);
        }
        
        
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}


void task_cmd_uart(void *params)
{
    char *cmd_receive;
    char  error_msg[120];
    int   len;

    while (1) {
        if (xQueueReceive(flow_data_queue, &cmd_receive, portMAX_DELAY)) {

            char **tokens = parse_input(cmd_receive);

            if (tokens == NULL || tokens[0] == NULL) {
                len = snprintf(error_msg, sizeof(error_msg),
                               "\r\nMAIN -> ERR: No se recibieron comandos\r\n");
                uart_write_bytes(UART_NUM_0, error_msg, len);
                free(cmd_receive);
                continue;
            }

            if (strcmp(tokens[0], "SET_WIFI") == 0) {
                if (tokens[1] == NULL) {
                    len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN: error al parsear\r\n");
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
                else if (strncmp(tokens[1], "DFT_", 4) == 0) {
                    int found = 0;
                    for (int i = 0; i < (int)DEFAULT_PROFILES_COUNT; i++) {
                        if (strcmp(tokens[1], default_profiles[i].alias) == 0) {
                            found = 1;
                            if (update_setup_cred((char *)default_profiles[i].ssid,
                                                  (char *)default_profiles[i].pswd,
                                                  NULL, "SSID") == ESP_OK)
                                xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                            else {
                                len = snprintf(error_msg, sizeof(error_msg),
                                               "\r\nMAIN: error al cargar perfil\r\n");
                                uart_write_bytes(UART_NUM_0, error_msg, len);
                            }
                            break;
                        }
                    }
                    if (!found) {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN: perfil no encontrado\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    }
                }
                else {
                    if (tokens[2] == NULL) {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN -> ERR: formato incorrecto\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    } else if (update_setup_cred(tokens[1], tokens[2], NULL, "SSID") == ESP_OK) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                    } else {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN -> ERR: no memoria\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    }
                }
            }

            // ── ENT_WIFI ──────────────────────────────────────────────────────
            else if (strcmp(tokens[0], "ENT_WIFI") == 0) {
                if (tokens[1] == NULL) {
                    len = snprintf(error_msg, sizeof(error_msg),
                                   "\r\nMAIN -> ERR: formato incorrecto\r\n");
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
                else if (strncmp(tokens[1], "DFT_ENT_", 8) == 0) {
                    int found = 0;
                    for (int i = 0; i < (int)DEFAULT_ENT_PROFILES_COUNT; i++) {
                        if (strcmp(tokens[1], default_ent_profiles[i].alias) == 0) {
                            found = 1;
                            if (update_setup_cred((char *)default_ent_profiles[i].ssid,
                                                  (char *)default_ent_profiles[i].pswd,
                                                  (char *)default_ent_profiles[i].user,
                                                  "ENT") == ESP_OK)
                                xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                            else {
                                len = snprintf(error_msg, sizeof(error_msg),
                                               "\r\nMAIN: error credenciales\r\n");
                                uart_write_bytes(UART_NUM_0, error_msg, len);
                            }
                            break;
                        }
                    }
                    if (!found) {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN: perfil ENT no encontrado\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    }
                }
                else {
                    if (tokens[2] == NULL || tokens[3] == NULL) {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN -> ERR: formato incorrecto\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    } else if (update_setup_cred(tokens[1], tokens[3], tokens[2], "ENT") == ESP_OK) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                    } else {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\nMAIN -> ERR: no memoria\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    }
                }
            }

            // ── TARE ──────────────────────────────────────────────────────────
            else if (strcmp(tokens[0], "TARE") == 0) {
                EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
                if (bits & HX711_PAUSE_PRINT) {
                    // Durante calibración: solo señalar, la tarea calibra llama hx711_tare
                    xEventGroupSetBits(s_wifi_event_group, HX711_CAL_TARE_READY);
                } else {
                    // Uso normal: suspender → tarar → reanudar
                    hx711_suspend_task(&hx);
                    hx711_tare(&hx, 20);
                    hx711_resume_task(&hx);
                    len = snprintf(error_msg, sizeof(error_msg),
                                   "\r\n[HX711] Tara aplicada (offset=%ld).\r\n",
                                   (long)hx.offset);
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
            }

            // ── CAL:<peso_kg> ─────────────────────────────────────────────────
            else if (strcmp(tokens[0], "CAL") == 0) {
                EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
                if (!(bits & HX711_PAUSE_PRINT)) {
                    len = snprintf(error_msg, sizeof(error_msg),
                                   "\r\n[HX711] CAL solo disponible durante calibracion.\r\n"
                                   "Manda CAL_RESET para recalibrar.\r\n");
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
                else if (tokens[1] == NULL) {
                    len = snprintf(error_msg, sizeof(error_msg),
                                   "\r\n[HX711] ERR: CAL:<peso_kg>  ej: CAL:1.000\r\n");
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
                else {
                    char *endptr = NULL;
                    float  w     = strtof(tokens[1], &endptr);
                    if (w <= 0.0f || endptr == tokens[1]) {
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\n[HX711] ERR: peso invalido '%s'."
                                       " Usa punto decimal: CAL:1.000\r\n", tokens[1]);
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    } else {
                        g_cal_weight_kg = w;
                        len = snprintf(error_msg, sizeof(error_msg),
                                       "\r\n[HX711] Referencia: %.3f kg. Calculando...\r\n", w);
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                        xEventGroupSetBits(s_wifi_event_group, HX711_CAL_WEIGHT_OK);
                    }
                }
            }

            // ── CAL_RESET ─────────────────────────────────────────────────────
            else if (strcmp(tokens[0], "CAL_RESET") == 0) {
                xEventGroupSetBits(s_wifi_event_group, HX711_PAUSE_PRINT);
                g_cal_weight_kg = 0.0f;
                // Suspender ANTES de tocar offset/scale para evitar race condition
                hx711_suspend_task(&hx);
                hx.offset = 0;
                hx.scale  = 1.0f;
                hx.weight = 0.0f;
                hx.last_weight = 0.0f;
                hx.stable_weight = 0.0f;
                char msg[] = "\r\n[HX711] Calibracion borrada.\r\n"
                             "Coloque la estructura/soporte vacio y envie: TARE\r\n";
                uart_write_bytes(UART_NUM_0, msg, sizeof(msg) - 1);
                xTaskCreate(task_hx711_calibrate, "hx711_cal", 4096, &hx, 6, NULL);
            }

            else if (g_device_role == ROLE_MASTER && strcmp(tokens[0], "ACK") == 0) {
                // ACK recibido desde UART1
                xEventGroupSetBits(s_wifi_event_group, BROKER_RECEIVED);
            }
            free(cmd_receive);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_input
// ─────────────────────────────────────────────────────────────────────────────
char **parse_input(char *line)
{
    char **tokens  = malloc(6 * sizeof(char *));
    char  *token;
    int    position = 0;

    token = strtok(line, ":");
    while (token != NULL) {
        if (position >= 5) break;
        tokens[position++] = strdup(token);
        token = strtok(NULL, ":");
    }
    tokens[position] = NULL;
    return tokens;
}

// ─────────────────────────────────────────────────────────────────────────────
// update_setup_cred
// ─────────────────────────────────────────────────────────────────────────────
esp_err_t update_setup_cred(char *ssid, char *pswd, char *user, char *type)
{
    if (strcmp(type, "SSID") == 0) {
        esp_wifi.ssid = realloc(esp_wifi.ssid, strlen(ssid) + 1);
        esp_wifi.pswd = realloc(esp_wifi.pswd, strlen(pswd) + 1);
        if (esp_wifi.ssid != NULL && esp_wifi.pswd != NULL) {
            strcpy(esp_wifi.ssid, ssid);
            strcpy(esp_wifi.pswd, pswd);
            free(esp_wifi.user_name);
            esp_wifi.user_name      = NULL;
            esp_wifi.type_connected = 0;
            return ESP_OK;
        }
        uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
        return ESP_FAIL;
    }

    if (strcmp(type, "ENT") == 0) {
        esp_wifi.ssid      = realloc(esp_wifi.ssid,      strlen(ssid) + 1);
        esp_wifi.pswd      = realloc(esp_wifi.pswd,      strlen(pswd) + 1);
        esp_wifi.user_name = realloc(esp_wifi.user_name, strlen(user) + 1);
        if (esp_wifi.ssid != NULL && esp_wifi.pswd != NULL && esp_wifi.user_name != NULL) {
            strcpy(esp_wifi.ssid,      ssid);
            strcpy(esp_wifi.pswd,      pswd);
            strcpy(esp_wifi.user_name, user);
            esp_wifi.type_connected = 1;
            return ESP_OK;
        }
        uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
        return ESP_FAIL;
    }

    return ESP_FAIL;
}