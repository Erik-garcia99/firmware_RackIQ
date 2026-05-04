// /**
//  * @author: erik garcia chavez - ingenieria con computacion 
//  * @fileinfo: archivo principal del proyecto, este archivo se encarga de iniciar el programa, y de llamar a las funciones necesarias para el funcionamiento del programa, como la conexion a la red, la configuracion del wifi, etc.
//  * 
//  * @date: 23 de abril del 2026
//  * 
//  * 
//  */


// #include <stdio.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <ctype.h>

// //freertos
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include <freertos/event_groups.h>

// //drivers
// #include <driver/uart.h>

// //logs
// #include <esp_log.h>
// #include <esp_err.h>

// //wifi
// #include <esp_wifi.h>
// #include <nvs_flash.h>
// #include <lwip/err.h>
// #include <lwip/sys.h>

// //++++++++++++++++++++librerias propias
// #include"global.h"
// #include"modules/WIFI/wifi_lib.h"
// #include"modules/UART/uart_lib.h"
// #include"modules/HX711/hx711_lib.h"





// //+++++++++++++++++colas 
// //cola en donde se manda datos desde la tarea de uart hacia el main
// QueueHandle_t flow_data_queue;
// //cola encarcada de recibir los eventos del uart, como cuando se recibe un byte, o cuando se llena el buffer, etc.
// QueueHandle_t uart_event;

// //+++++++++++++++++++++ variables globales 

// static task_uart_port_t user_uart;

// //perfil por defecto para wifi nromal
// static const wifi_default_profile_t default_profiles[] = {
//     { "DFT_1", "INFINITUMF4AF", "nFukH34MPW" },
// };
// #define DEFAULT_PROFILES_COUNT (sizeof(default_profiles) / sizeof(default_profiles[0]))

// //perfil para la uni 

// static const wifi_default_ent_profile_t default_ent_profiles[] = {
//     { "DFT_ENT_1", "UABC-5G", "erik.garcia.chavez", "MarKI?1T04A" },
//     // { "DFT_ENT_2", "UABC-2.4G", "erik.garcia.chavez", "MarKI?1T04A" },
// };
// #define DEFAULT_ENT_PROFILES_COUNT (sizeof(default_ent_profiles) / sizeof(default_ent_profiles[0]))

// //++++++++++++++++++++++++++++++++++++ grupos de eventos
// EventGroupHandle_t s_wifi_event_group;


// //+++++++++++++++++++++estrucutras 
// esp_wifi_t esp_wifi;

// //hx711
// hx711_t hx;

// //++++++++++++++++++++++enums


// //+++++++++++++++++++++funciones

// /**
//  * @brief funcion que se encargara de serparar los tokens ingresador por le usuario
//  * este solo es serparar entre los comandos, mas no extrae los token necesarios 
//  * * @param line recibe un apuntador a los datos que ingresaron por UART 
//  *
//  * @return regresa la lista de tokens  
//  * */ 
// char **parse_input(char *line); // Â¡Corregido el nombre aquÃ­!


// /**
//  * @brief funcion encargada de actualizar las distittas credencuales mediante comandos 
//  * * @param key donde va el nombre del nuevo SSID o la nueva IP.
//  * @param anchor deberia de ir la contrasenia de la nueva red o el nombre del usuario en caso de ser una red de empresa
//  * @param pswd_ent si este tiene datos es que se actualizara una red de empresa o unversidad, va la contrasenia del usuario
//  * @param identificator le dira a la funcion que tiepo de actualizacion sera, puede ser <SSID, HOST_IP, SSID_ENT, MAT>
//  * * @return ESP_OK si se pudo actualziar 
//  * @return ESP_FAIL si no fue posible 
//  * */
// esp_err_t update_setup_cred(char *key, char *anchor ,char *pswd_ent,  char *identificator);

// //+++++++++++++++++++++tareas
// /**
//  * @brief tarea encargada de recibir lo que se introdujo por el buffer de uart, ya sea UART0, 1 o 2
//  * se encarga de seprar los tokens y decidir que es lo que el usuario o el sisteme intenta hacer 
//  * es una tarea de alta prioridad, ya que esta en escucha de cambios que pueden pasar 
//  * 
//  */
// void task_cmd_uart(void *params);

// //mostrar lo que lee la bascula 
// void task_hx711_uart(void *params);


// void app_main(void)
// {   


//     //+++++++++++++++++UART
//     //esta es la cola por donde la tarea que recibe de uart manda a la tarea que se encarga de ver que es lo que se envio, separar los tokens de los comandos 
//     flow_data_queue = xQueueCreate(10, sizeof(char*));
//     // task_uart_port_t user_uart;
//     user_uart.NUM_PORT=UART_NUM_0;
//     //vamos a debugear con el uart 0 por ahora, pero despues vamos a utilizar UART1 para comunicacion con la rasberry pi 
//     uart_init(user_uart.NUM_PORT,115200, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

//     //inicamos la tareas de uart, la encargada de esuchar el buffer, los pines fisicos que recibiran 

//     //aqui esta el demon que esta ecuchando a UART 
//     xTaskCreate(task_uart, "task_uart", 4096, &user_uart, 9, NULL);

//     xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL, 8, NULL);
//     //+++++++++++++++++++++
//     //primero podemos definir inicar con los perfiles por defecto que ya deberiamos de crear 
    
//     //+++++++++++++++++++++++++++++++WIFI
//     s_wifi_event_group = xEventGroupCreate();
//     esp_err_t ret;

//     ret = nvs_flash_init();
    
    
//     if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     // HX711 INIT
//     hx711_init(&hx, DOUT_PIN, PD_SCK_PIN);

//     // intentar cargar calibración
//     if (!hx711_load_calibration(&hx)) {

//         char msg[80];
//         int len;

//         len = snprintf(msg, sizeof(msg), "\r\nHX711: Calibrando...\r\n");
//         uart_write_bytes(UART_NUM_0, msg, len);

//         hx711_tare(&hx, 20);

//         vTaskDelay(pdMS_TO_TICKS(3000));

//         int32_t raw = hx711_read_average(&hx, 20);
//         float scale = (raw - hx.offset) / 1.0;

//         hx711_set_scale(&hx, scale);
//         hx711_save_calibration(&hx);

//         len = snprintf(msg, sizeof(msg), "\r\nHX711: Calibrado\r\n");
//         uart_write_bytes(UART_NUM_0, msg, len);
//     }

//     // iniciar tarea HX711
//     hx711_start_task(&hx);

//     update_setup_cred((char *)default_profiles[0].ssid,(char *)default_profiles[0].pswd,NULL, "SSID");
//     xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);

//     wifi_init_sta();


//     //tareas: 
//     xTaskCreate(task_hx711_uart, "task_hx711_uart", 4096, NULL, 5, NULL);

// }



// //
// void task_cmd_uart(void *params){
    
//     char *cmd_receive;
//     char error_msg[80];
//     int len;
//     while (1)
//     {
//         if(xQueueReceive(flow_data_queue, &cmd_receive, portMAX_DELAY)){

//             char **tokens = parse_input(cmd_receive);

//             if (tokens == NULL || tokens[0] == NULL) {

//                 int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: No se recibieron comandos\r\n");
//                 uart_write_bytes(UART_NUM_0,error_msg,len);
//                 free(cmd_receive);
//                 continue;
//             }

//             //vamos a ver que es lo se recivio 

//             if(strcmp(tokens[0],"SET_WIFI") == 0){
//                 //indicia que se quire actualizar wifi 
//                 if(tokens[1] == NULL){
//                     //ubbo un error en el parsear la cadena que se ingreso 

//                     int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN: error al parcerar\r\n");
//                     uart_write_bytes(UART_NUM_0, error_msg, len);
//                 }
//                 //ahora debemos de ver si se ecogio un perfil por defecto.
//                 else if (strncmp(tokens[1], "DFT_", 4) == 0) {
//                     int found = 0;
//                     for (int i = 0; i < (int)DEFAULT_PROFILES_COUNT; i++) {
//                         if (strcmp(tokens[1], default_profiles[i].alias) == 0) {
//                             found = 1;
//                             if (update_setup_cred((char *)default_profiles[i].ssid,(char *)default_profiles[i].pswd,NULL, "SSID") == ESP_OK) {
//                                 xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
//                             } else {

//                                 int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error al cargar PERFIL_1\r\n");
//                                 uart_write_bytes(UART_NUM_0,error_msg, len);
//                             }
//                             break;
//                         }
//                         if (!found){
//                             int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error no se encontraron perfiles\r\n");
//                             uart_write_bytes(UART_NUM_0, error_msg, len);
//                         }
//                     }
//                 }
//                 else {
//                     if (tokens[2] == NULL) {

//                         int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
//                         uart_write_bytes(UART_NUM_0,error_msg,len);
//                         // uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
//                     } else if (update_setup_cred(tokens[1], tokens[2],NULL, "SSID") == ESP_OK) {
//                         xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
//                     } else {
//                         int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: no memoria\r\n");
//                         uart_write_bytes(UART_NUM_0,error_msg,len);
//                     }
//                 }
//             }

//             //red de empresa
//             else if(strcmp(tokens[0], "ENT_WIFI") == 0){
//                 if (tokens[1] == NULL) {
//                     int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
//                     uart_write_bytes(UART_NUM_0,error_msg,len);
//                 } 
//                 else if (strncmp(tokens[1], "DFT_ENT_", 8) == 0) {
//                     int found = 0;
//                     for (int i = 0; i < (int)DEFAULT_ENT_PROFILES_COUNT; i++) {
//                         if (strcmp(tokens[1], default_ent_profiles[i].alias) == 0) {
//                             found = 1;
//                             if (update_setup_cred((char *)default_ent_profiles[i].ssid,(char *)default_ent_profiles[i].pswd,(char *)default_ent_profiles[i].user,"ENT") == ESP_OK) {
//                                 xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
//                             } else {
//                                 len = snprintf(error_msg, sizeof(error_msg), "MAIN:ERRO - > credenciles incorrectas");
//                                 uart_write_bytes(UART_NUM_0, error_msg, len);
//                             }
//                             break;
//                         }
//                     }
//                     if (!found){
//                         int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error no se encontraron perfiles\r\n");
//                         uart_write_bytes(UART_NUM_0, error_msg, len);
//                     } 
//                 } 
//                 else {
//                     if (tokens[2] == NULL || tokens[3] == NULL) {
//                         int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
//                         uart_write_bytes(UART_NUM_0,error_msg,len);
//                     } else if (update_setup_cred(tokens[1], tokens[3],
//                                                 tokens[2], "ENT") == ESP_OK) {
//                         xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
//                     } else {
//                         int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: no memoria\r\n");
//                         uart_write_bytes(UART_NUM_0,error_msg,len);
//                     }
//                 }
//             }
//         }
//     }
// }



// //+++++++++++++++++++++++++++++++++++++++++++++++++HX711

// //++++++++++++++++++++++funciones   

// char **parse_input(char *line){
//     char **tokens = malloc(6 * sizeof(char*));
//     char *token; 
//     int position=0;

//     //separamos los tokens por ':' que es el delimitador 
//     token = strtok(line, ":");

//     while(token != NULL){
//         //guarda la utlima posicion para el NULL, indicando el final de los tokens. 
//         if(position >= 5 ) break;
//         tokens[position++] = strdup(token);
//         token = strtok(NULL, ":");
//     }

//     tokens[position] = NULL;
    
//     return tokens;
// }


// //puede que aqui este el error en el otro codigo. 
// esp_err_t update_setup_cred(char *ssid, char *pswd, char *user, char *type){
//     if (strcmp(type, "SSID") == 0) {
//         esp_wifi.ssid = realloc(esp_wifi.ssid, strlen(ssid) + 1);
//         esp_wifi.pswd = realloc(esp_wifi.pswd, strlen(pswd) + 1);

//         if (esp_wifi.ssid != NULL && esp_wifi.pswd != NULL) {
//             strcpy(esp_wifi.ssid, ssid);
//             strcpy(esp_wifi.pswd, pswd);
//             // limpiar usuario si habia una sesion enterprise previa
//             free(esp_wifi.user_name);
//             esp_wifi.user_name      = NULL;
//             esp_wifi.type_connected = 0;
//             return ESP_OK;
//         } else {
//             uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
//             return ESP_FAIL;
//         }
//     }

//     if (strcmp(type, "ENT") == 0) {
//         esp_wifi.ssid      = realloc(esp_wifi.ssid,      strlen(ssid) + 1);
//         esp_wifi.pswd      = realloc(esp_wifi.pswd,      strlen(pswd) + 1);
//         esp_wifi.user_name = realloc(esp_wifi.user_name, strlen(user) + 1);

//         if (esp_wifi.ssid != NULL && esp_wifi.pswd != NULL && esp_wifi.user_name != NULL) {
//             strcpy(esp_wifi.ssid,      ssid);
//             strcpy(esp_wifi.pswd,      pswd);
//             strcpy(esp_wifi.user_name, user);
//             esp_wifi.type_connected = 1;
//             return ESP_OK;
//         } else {
//             uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
//             return ESP_FAIL;
//         }
//     }

//     return ESP_FAIL;
// }


// //enviar el peso pro uart: 
// void task_hx711_uart(void *params){

//     char msg[100];
//     int len;

//     while(1){

//         float peso = hx711_get_weight(&hx);

//         if(hx711_is_stable(&hx)){
//             len = snprintf(msg, sizeof(msg),
//                 "STABLE: %.2f kg\r\n",
//                 hx711_get_stable_weight(&hx));
//         }else{
//             len = snprintf(msg, sizeof(msg),
//                 "RAW: %.2f kg\r\n",
//                 peso);
//         }

//         uart_write_bytes(UART_NUM_0, msg, len);

//         vTaskDelay(pdMS_TO_TICKS(300));
//     }
// }



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

// logs
#include <esp_log.h>
#include <esp_err.h>

// wifi
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>

// librerías propias
#include "global.h"
#include "modules/WIFI/wifi_lib.h"
#include "modules/UART/uart_lib.h"
#include "modules/HX711/hx711_lib.h"
#include "modules/MQTT/mqtt_lib.h"

QueueHandle_t flow_data_queue;
QueueHandle_t uart_event;

static task_uart_port_t user_uart;
float g_cal_weight_kg = 0.0f;

static const wifi_default_profile_t default_profiles[] = {
    { "DFT_1", "INFINITUMF4AF", "nFukH34MPW" },
};
#define DEFAULT_PROFILES_COUNT (sizeof(default_profiles) / sizeof(default_profiles[0]))

static const wifi_default_ent_profile_t default_ent_profiles[] = {
    { "DFT_ENT_1", "UABC-5G", "erik.garcia.chavez", "MarKI?1T04A" },
};
#define DEFAULT_ENT_PROFILES_COUNT (sizeof(default_ent_profiles) / sizeof(default_ent_profiles[0]))

EventGroupHandle_t s_wifi_event_group;
esp_wifi_t         esp_wifi;
hx711_t            hx;

char    **parse_input(char *line);
esp_err_t update_setup_cred(char *key, char *anchor, char *pswd_ent, char *identificator);
void      task_cmd_uart(void *params);
void      task_hx711_uart(void *params);
void      task_hx711_calibrate(void *params);


void app_main(void)
{

    flow_data_queue    = xQueueCreate(10, sizeof(char *));
    user_uart.NUM_PORT = UART_NUM_0;
    uart_init(user_uart.NUM_PORT, 115200,
              UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1,
              UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(task_uart,     "task_uart",     4096, &user_uart, 9, NULL);
    xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL,       8, NULL);
    s_wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
    update_setup_cred((char *)default_profiles[0].ssid,
                      (char *)default_profiles[0].pswd, NULL, "SSID");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
    wifi_init_sta();


    xEventGroupWaitBits(s_wifi_event_group,WIFI_CONNECTED_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
    mqtt_init(s_wifi_event_group); 


    xTaskCreate(task_hx711_uart, "task_hx711_uart", 4096, NULL, 5, NULL);

    xTaskCreate(task_mqtt_weight_publisher,"mqtt_weight",       4096, &hx, 5, NULL); 
    xTaskCreate(task_mqtt_heartbeat,       "mqtt_heartbeat",    2048, NULL, 4, NULL);

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