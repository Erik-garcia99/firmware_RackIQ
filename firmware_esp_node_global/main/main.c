/**
 * en este proyecto no habra logs ya que el ESP no tendra coenxion con una pantalla
 * el unico que puede tener conexion a una pantalla seria la rasberry, por medio de uart o de mqtt
 * vamos a tramisitri logs, o aspectos importantes del esp, puede que aspectos de bajo nivel 
 * otro conceptos se deberan de mostrar en la app 
 * */

//libereias estandars
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//freertos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

//drivers
#include <driver/uart.h>

//logs
#include <esp_log.h>
#include <esp_err.h>

//wifi
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>

//libreria interna 
#include "modules/WIFI/wifi_lib.h"
#include "modules/UART/uart_lib.h"
#include "modules/HX711/hx711_lib.h"
#include "modules/MQTT/mqtt_lib.h"
#include "global.h"

// ============================================================================
// perfiles por defecto
// ============================================================================

/**
 * Para agregar un perfil normal nuevo:
 * 1. Agrega una entrada al array default_profiles[]
 * 2. El alias debe seguir el patron "DFT_N" (N = numero entero)
 * 3. Invocalo desde UART con:  SET_WIFI:DFT_N
 */
typedef struct {
    const char *alias;
    const char *ssid;
    const char *pswd;
} wifi_default_profile_t;

static const wifi_default_profile_t default_profiles[] = {
    { "DFT_1", "INFINITUMF4AF", "nFukH34MPW" },
};
#define DEFAULT_PROFILES_COUNT (sizeof(default_profiles) / sizeof(default_profiles[0]))

/**
 * Para agregar un perfil enterprise nuevo:
 * 1. Agrega una entrada al array default_ent_profiles[]
 * 2. El alias debe seguir el patron "DFT_ENT_N"
 * 3. Invocalo desde UART con:  ENT_WIFI:DFT_ENT_N
 */
typedef struct {
    const char *alias;
    const char *ssid;
    const char *user;
    const char *pswd;
} wifi_default_ent_profile_t;

static const wifi_default_ent_profile_t default_ent_profiles[] = {
    { "DFT_ENT_1", "UABC-5G", "erik.garcia.chavez", "MarKI?1T04A" },
    // { "DFT_ENT_2", "UABC-2.4G", "erik.garcia.chavez", "MarKI?1T04A" },
};
#define DEFAULT_ENT_PROFILES_COUNT (sizeof(default_ent_profiles) / sizeof(default_ent_profiles[0]))

//++++++++++++++++ colas 
//cola para los eventos de UART
QueueHandle_t uart_event;
//cola que manerjara el flujo de datos de UART 
QueueHandle_t flow_data_queue;
//cola para el flujo de MQTT
QueueHandle_t flow_data;

//++++++++++++++++ grupos de eventos. 
EventGroupHandle_t s_wifi_event_group;

//++++++++++++++++++  estrucutra
//contiene valores de UART 
task_uart_port_t global_uart;
//estucuturua para red 
esp_wifi_t esp_wifi;

//+++++++++++++++++ vairbales globales

// flag: indica que wifi_manager ya conecto y le pidio la IP del broker a la RPi
// task_cmd_uart lo usa para saber que el proximo mensaje es la IP del broker
static volatile uint8_t waiting_for_broker = 0;

hx711_t shelf_1 = {
    .dout_pin = GPIO_NUM_4, // Cambia estos pines por los que uses
    .pd_sck_pin = GPIO_NUM_5,
    .gain = 128,
    .offset = 8450000, // Estos los sacarás calibrando
    .scale = 420.5
};

//++++++++++++++++++++funciones

/**
 * @brief funcion que se encargara de serparar los tokens ingresador por le usuario
 * este solo es serparar entre los comandos, mas no extrae los token necesarios 
 * * @param line recibe un apuntador a los datos que ingresaron por UART 
 *
 * @return regresa la lista de tokens  
 * */ 
char **parse_input(char *line); // ¡Corregido el nombre aquí!

/**
 * @brief funcion encargada de actualizar las distittas credencuales mediante comandos 
 * * @param key donde va el nombre del nuevo SSID o la nueva IP.
 * @param anchor deberia de ir la contrasenia de la nueva red o el nombre del usuario en caso de ser una red de empresa
 * @param pswd_ent si este tiene datos es que se actualizara una red de empresa o unversidad, va la contrasenia del usuario
 * @param identificator le dira a la funcion que tiepo de actualizacion sera, puede ser <SSID, HOST_IP, SSID_ENT, MAT>
 * * @return ESP_OK si se pudo actualziar 
 * @return ESP_FAIL si no fue posible 
 * */
esp_err_t update_setup_cred(char *key, char *anchor ,char *pswd_ent,  char *identificator);

// (Se eliminaron las declaraciones estáticas de handle_set_wifi y handle_ent_wifi que no usabas)

//+++++++++++++++++++++++++++tareas

//tarea encargada de recibir el comando por UART 
void task_cmd_uart(void *params);
void task_wifi_manager(void *params);
void task_sensors(void *params);


void app_main(void)
{
    //creamos el grupo de eventos para WIFI 
    s_wifi_event_group = xEventGroupCreate();

    //esta es la cola que funcoina como orquestador entre lo que entra en el UART y en la tarea que necsita esa informacion. 
    flow_data_queue = xQueueCreate(10, sizeof(char*));
    
    // Cola para comunicacion con MQTT
    flow_data = xQueueCreate(10, sizeof(char*));
    
    global_uart.NUM_PORT = UART_MAIN;

    esp_err_t ret;
    ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //inicamos UART 
    uart_init(UART_MAIN,115200, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    //aqui esta el demon que esta ecuchando a UART 
    xTaskCreate(task_uart, "task_uart", 4096, &global_uart, 9, NULL);

    xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL, 8, NULL);

    //inicamos stack WIFI, solo hardware sin conectar 
    wifi_stack_init(); // ¡Ojo! Asegúrate de que esta función esté declarada en modules/WIFI/wifi_lib.h

    //tarea que maneja conexion con wifi - conexion con mqtt idicnadoq que el esp
    //esta listo para operar 
    xTaskCreate(task_wifi_manager, "task_wifi_mgr", 4096, NULL, 7, NULL);
    
    xTaskCreate(task_sensors, "task_sensors", 4098, NULL, 8, NULL);

    //itentamos conectarnos a wifi por defecto
    update_setup_cred((char *)default_profiles[0].ssid,
                      (char *)default_profiles[0].pswd,
                      NULL, "SSID");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
}

void task_wifi_manager(void *params) {
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CREDS_READY, pdTRUE, pdTRUE, portMAX_DELAY);

    wifi_connect();

    waiting_for_broker = 1;
    uart_write_bytes(UART_MAIN, "REQUEST_BROKER\n", 15);

    // 4. esperar la IP del broker (la setea task_cmd_uart al recibirla)
    // xEventGroupWaitBits(s_wifi_event_group, BROKER_RECEIVED, pdFALSE, pdTRUE, portMAX_DELAY);
    waiting_for_broker = 0;

    //conectar MQTT usando las credenciales y el broker default de HiveMQ configurados
    mqtt_start();

    vTaskDelete(NULL);
}

void task_cmd_uart(void *params) {
    char *cmd_receive;

    while (1) {
        if (!xQueueReceive(flow_data_queue, &cmd_receive, portMAX_DELAY)) continue;

        char **tokens = parse_input(cmd_receive);

        if (tokens == NULL || tokens[0] == NULL) {
            uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
            free(cmd_receive);
            continue;
        }

        // ---- SET_WIFI -----------------------------------------------------
        if (strcmp(tokens[0], "SET_WIFI") == 0) {
            if (tokens[1] == NULL) {
                uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
            } else if (strncmp(tokens[1], "DFT_", 4) == 0) {
                int found = 0;
                for (int i = 0; i < (int)DEFAULT_PROFILES_COUNT; i++) {
                    if (strcmp(tokens[1], default_profiles[i].alias) == 0) {
                        found = 1;
                        if (update_setup_cred((char *)default_profiles[i].ssid,
                                              (char *)default_profiles[i].pswd,
                                              NULL, "SSID") == ESP_OK) {
                            xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                        } else {
                            uart_write_bytes(UART_MAIN, "ERR:CREDS\n", 10);
                        }
                        break;
                    }
                }
                if (!found) uart_write_bytes(UART_MAIN, "ERR:DFT_NOT_FOUND\n", 18);
            } else {
                if (tokens[2] == NULL) {
                    uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
                } else if (update_setup_cred(tokens[1], tokens[2],
                                             NULL, "SSID") == ESP_OK) {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                } else {
                    uart_write_bytes(UART_MAIN, "ERR:CREDS\n", 10);
                }
            }

        // ---- ENT_WIFI -----------------------------------------------------
        } else if (strcmp(tokens[0], "ENT_WIFI") == 0) {
            if (tokens[1] == NULL) {
                uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
            } else if (strncmp(tokens[1], "DFT_ENT_", 8) == 0) {
                int found = 0;
                for (int i = 0; i < (int)DEFAULT_ENT_PROFILES_COUNT; i++) {
                    if (strcmp(tokens[1], default_ent_profiles[i].alias) == 0) {
                        found = 1;
                        if (update_setup_cred((char *)default_ent_profiles[i].ssid,
                                              (char *)default_ent_profiles[i].pswd,
                                              (char *)default_ent_profiles[i].user,
                                              "ENT") == ESP_OK) {
                            xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                        } else {
                            uart_write_bytes(UART_MAIN, "ERR:CREDS\n", 10);
                        }
                        break;
                    }
                }
                if (!found) uart_write_bytes(UART_MAIN, "ERR:DFT_NOT_FOUND\n", 18);
            } else {
                if (tokens[2] == NULL || tokens[3] == NULL) {
                    uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
                } else if (update_setup_cred(tokens[1], tokens[3],
                                             tokens[2], "ENT") == ESP_OK) {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                } else {
                    uart_write_bytes(UART_MAIN, "ERR:CREDS\n", 10);
                }
            }
        } else {
            uart_write_bytes(UART_MAIN, "ERR:CMD_UNKNOWN\n", 16);
        }

        for (int i = 0; tokens[i] != NULL; i++) free(tokens[i]);
        free(tokens);
        free(cmd_receive);
    }
}

char **parse_input(char *line){ // ¡Corregido el nombre aquí también!
    char **tokens = malloc(6 * sizeof(char*));
    char *token; 
    int position=0;

    //separamos los tokens por ':' que es el delimitador 
    token = strtok(line, ":");

    while(token != NULL){
        if(position >= 5 ) break;
        tokens[position++] = strdup(token);
        token = strtok(NULL, ":");
    }

    tokens[position] = NULL;
    
    return tokens;
}

esp_err_t update_setup_cred(char *ssid, char *pswd, char *user, char *type) {
    if (strcmp(type, "SSID") == 0) {
        esp_wifi.ssid = realloc(esp_wifi.ssid, strlen(ssid) + 1);
        esp_wifi.pswd = realloc(esp_wifi.pswd, strlen(pswd) + 1);

        if (esp_wifi.ssid != NULL && esp_wifi.pswd != NULL) {
            strcpy(esp_wifi.ssid, ssid);
            strcpy(esp_wifi.pswd, pswd);
            // limpiar usuario si habia una sesion enterprise previa
            free(esp_wifi.user_name);
            esp_wifi.user_name      = NULL;
            esp_wifi.type_connected = 0;
            return ESP_OK;
        } else {
            uart_write_bytes(UART_MAIN, "ERR:NO_MEM\n", 11);
            return ESP_FAIL;
        }
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
        } else {
            uart_write_bytes(UART_MAIN, "ERR:NO_MEM\n", 11);
            return ESP_FAIL;
        }
    }

    return ESP_FAIL;
}

void task_sensors(void *params) {
    hx711_init(&shelf_1);
    
    shelf_1.offset = hx711_read_average(&shelf_1, 10);
    
    while(1) {
        // leemos el peso promedio de 3 muestras para estabilizar
        float current_weight = hx711_get_weight(&shelf_1, 3);
        
        //para enviar por mqtt 
        char pub_mqttp[64];
        
        // ¡Corregido el error de snprintf! Ahora guardamos los datos en pub_mqttp
        int len = snprintf(pub_mqttp, sizeof(pub_mqttp), "Estante 1: %.2f gramos\n", current_weight);
        
        //pendiente mqtt: usarás 'pub_mqttp' (y 'len' si tu función lo requiere) para enviar el mensaje
        

        vTaskDelay(pdMS_TO_TICKS(1000)); // Leer cada segundo
    }
}