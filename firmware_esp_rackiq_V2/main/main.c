/**
 * @author: erik garcia chavez - ingenieria con computacion 
 * @fileinfo: archivo principal del proyecto, este archivo se encarga de iniciar el programa, y de llamar a las funciones necesarias para el funcionamiento del programa, como la conexion a la red, la configuracion del wifi, etc.
 * 
 * @date: 23 de abril del 2026
 * 
 * 
 */


#include <stdio.h>
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

// #include<mqtt_client.h>  // MQTT comentado por ahora

//++++++++++++++++++++librerias propias
#include"global.h"
#include"modules/WIFI/wifi_lib.h"
#include"modules/UART/uart_lib.h"
#include"modules/HX711/hx711.h"
// #include"modules/MQTT/mqtt_lib.h"  // MQTT comentado por ahora

#define NVS_NAMESPACE "hx711_cal"
#define NVS_KEY_TARA    "tara_adc"
#define NVS_KEY_FACTOR  "factor_kg" 






//+++++++++++++++++colas 
//cola en donde se manda datos desde la tarea de uart hacia el main
QueueHandle_t flow_data_queue;
//cola encarcada de recibir los eventos del uart, como cuando se recibe un byte, o cuando se llena el buffer, etc.
QueueHandle_t uart_event;

//+++++++++++++++++++++ variables globales 

static task_uart_port_t user_uart;

//perfil por defecto para wifi nromal
static const wifi_default_profile_t default_profiles[] = {
    { "DFT_1", "INFINITUMF4AF", "nFukH34MPW" },
};
#define DEFAULT_PROFILES_COUNT (sizeof(default_profiles) / sizeof(default_profiles[0]))

//perfil para la uni 

static const wifi_default_ent_profile_t default_ent_profiles[] = {
    { "DFT_ENT_1", "UABC-5G", "erik.garcia.chavez", "MarKI?1T04A" },
    // { "DFT_ENT_2", "UABC-2.4G", "erik.garcia.chavez", "MarKI?1T04A" },
};
#define DEFAULT_ENT_PROFILES_COUNT (sizeof(default_ent_profiles) / sizeof(default_ent_profiles[0]))

static int32_t s_tara_adc = 0;
static float   s_factor_kg = 1.0f;   // valor por defecto (luego se calibra)
static hx711_t s_hx711_dev;          // descriptor global para acceso desde comandos

//++++++++++++++++++++++++++++++++++++ grupos de eventos
EventGroupHandle_t s_wifi_event_group;


//+++++++++++++++++++++estrucutras 
esp_wifi_t esp_wifi;

//++++++++++++++++++++++enums


//+++++++++++++++++++++funciones

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

//+++++++++++++++++++++tareas
/**
 * @brief tarea encargada de recibir lo que se introdujo por el buffer de uart, ya sea UART0, 1 o 2
 * se encarga de seprar los tokens y decidir que es lo que el usuario o el sisteme intenta hacer 
 * es una tarea de alta prioridad, ya que esta en escucha de cambios que pueden pasar 
 * 
 */
void task_cmd_uart(void *params);


void task_monitoreo_estante(void *params);

static esp_err_t load_calibration(void);

static esp_err_t save_calibration(void);


void app_main(void)
{   


    //+++++++++++++++++UART
    //esta es la cola por donde la tarea que recibe de uart manda a la tarea que se encarga de ver que es lo que se envio, separar los tokens de los comandos 
    flow_data_queue = xQueueCreate(10, sizeof(char*));
    // task_uart_port_t user_uart;
    user_uart.NUM_PORT=UART_NUM_0;
    //vamos a debugear con el uart 0 por ahora, pero despues vamos a utilizar UART1 para comunicacion con la rasberry pi 
    uart_init(user_uart.NUM_PORT,115200, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //inicamos la tareas de uart, la encargada de esuchar el buffer, los pines fisicos que recibiran 

    //aqui esta el demon que esta ecuchando a UART 
    xTaskCreate(task_uart, "task_uart", 4096, &user_uart, 9, NULL);

    xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL, 8, NULL);
    //+++++++++++++++++++++
    //primero podemos definir inicar con los perfiles por defecto que ya deberiamos de crear 
    
    //+++++++++++++++++++++++++++++++WIFI
    s_wifi_event_group = xEventGroupCreate();
    esp_err_t ret;

    ret = nvs_flash_init();
    
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    load_calibration();

    update_setup_cred((char *)default_profiles[0].ssid,(char *)default_profiles[0].pswd,NULL, "SSID");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);

    wifi_init_sta();
    // mqtt_app_start();  // MQTT comentado por ahora

    //+++++++++++++tarea HX711
    xTaskCreate(task_monitoreo_estante,"task_monitoreo_estante", 8192, NULL, 9, NULL);

}




//
void task_cmd_uart(void *params){
    
    char *cmd_receive;
    char error_msg[80];
    int len;
    while (1)
    {
        if(xQueueReceive(flow_data_queue, &cmd_receive, portMAX_DELAY)){

            char **tokens = parse_input(cmd_receive);

            if (tokens == NULL || tokens[0] == NULL) {

                int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: No se recibieron comandos\r\n");
                uart_write_bytes(UART_NUM_0,error_msg,len);
                free(cmd_receive);
                continue;
            }

            //vamos a ver que es lo se recivio 

            if(strcmp(tokens[0],"SET_WIFI") == 0){
                //indicia que se quire actualizar wifi 
                if(tokens[1] == NULL){
                    //ubbo un error en el parsear la cadena que se ingreso 

                    int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN: error al parcerar\r\n");
                    uart_write_bytes(UART_NUM_0, error_msg, len);
                }
                //ahora debemos de ver si se ecogio un perfil por defecto.
                else if (strncmp(tokens[1], "DFT_", 4) == 0) {
                    int found = 0;
                    for (int i = 0; i < (int)DEFAULT_PROFILES_COUNT; i++) {
                        if (strcmp(tokens[1], default_profiles[i].alias) == 0) {
                            found = 1;
                            if (update_setup_cred((char *)default_profiles[i].ssid,(char *)default_profiles[i].pswd,NULL, "SSID") == ESP_OK) {
                                xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                            } else {

                                int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error al cargar PERFIL_1\r\n");
                                uart_write_bytes(UART_NUM_0,error_msg, len);
                            }
                            break;
                        }
                        if (!found){
                            int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error no se encontraron perfiles\r\n");
                            uart_write_bytes(UART_NUM_0, error_msg, len);
                        }
                    }
                }
                else {
                    if (tokens[2] == NULL) {

                        int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
                        uart_write_bytes(UART_NUM_0,error_msg,len);
                        // uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
                    } else if (update_setup_cred(tokens[1], tokens[2],NULL, "SSID") == ESP_OK) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                    } else {
                        int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: no memoria\r\n");
                        uart_write_bytes(UART_NUM_0,error_msg,len);
                    }
                }
            }

            //red de empresa
            else if(strcmp(tokens[0], "ENT_WIFI") == 0){
                if (tokens[1] == NULL) {
                    int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
                    uart_write_bytes(UART_NUM_0,error_msg,len);
                } 
                else if (strncmp(tokens[1], "DFT_ENT_", 8) == 0) {
                    int found = 0;
                    for (int i = 0; i < (int)DEFAULT_ENT_PROFILES_COUNT; i++) {
                        if (strcmp(tokens[1], default_ent_profiles[i].alias) == 0) {
                            found = 1;
                            if (update_setup_cred((char *)default_ent_profiles[i].ssid,(char *)default_ent_profiles[i].pswd,(char *)default_ent_profiles[i].user,"ENT") == ESP_OK) {
                                xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                            } else {
                                len = snprintf(error_msg, sizeof(error_msg), "MAIN:ERRO - > credenciles incorrectas");
                                uart_write_bytes(UART_NUM_0, error_msg, len);
                            }
                            break;
                        }
                    }
                    if (!found){
                        int len = snprintf(error_msg, sizeof(error_msg),"\r\nMAIN:error no se encontraron perfiles\r\n");
                        uart_write_bytes(UART_NUM_0, error_msg, len);
                    } 
                } 
                else {
                    if (tokens[2] == NULL || tokens[3] == NULL) {
                        int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: formato incorrecto\r\n");
                        uart_write_bytes(UART_NUM_0,error_msg,len);
                    } else if (update_setup_cred(tokens[1], tokens[3],
                                                tokens[2], "ENT") == ESP_OK) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
                    } else {
                        int len = snprintf(error_msg, sizeof(error_msg), "\r\nMAIN -> ERR: no memoria\r\n");
                        uart_write_bytes(UART_NUM_0,error_msg,len);
                    }
                }
            }

            // Comando: TARA  -> fija el peso actual como cero
            else if (strcmp(tokens[0], "TARA") == 0) {
                int32_t raw;
                if (hx711_read_average(&s_hx711_dev, 10, &raw) == ESP_OK) {
                    s_tara_adc = raw;
                    save_calibration();
                    uart_write_bytes(UART_NUM_0, "TARA OK\n", 8);
                } else {
                    uart_write_bytes(UART_NUM_0, "TARA ERR\n", 9);
                }
            }
            // Comando: CAL:<peso_en_kg>   ejemplo CAL:0.5
            else if (strcmp(tokens[0], "CAL") == 0) {
                if (tokens[1] == NULL) {
                    uart_write_bytes(UART_NUM_0, "ERR: uso CAL:peso(kg)\n", 22);
                } else {
                    float peso_conocido = atof(tokens[1]);
                    if (peso_conocido <= 0) {
                        uart_write_bytes(UART_NUM_0, "ERR: peso invalido\n", 19);
                    } else {
                        int32_t raw;
                        if (hx711_read_average(&s_hx711_dev, 10, &raw) == ESP_OK) {
                            int32_t adc_tareado = raw - s_tara_adc;
                            s_factor_kg = (float)adc_tareado / peso_conocido;
                            save_calibration();
                            char resp[64];
                            snprintf(resp, sizeof(resp), "CAL OK factor=%.2f u/kg\n", s_factor_kg);
                            uart_write_bytes(UART_NUM_0, resp, strlen(resp));
                        } else {
                            uart_write_bytes(UART_NUM_0, "ERR: lectura ADC\n", 17);
                        }
                    }
                }
            }
            // Comando: GET_PESO  -> devuelve el peso actual
            else if (strcmp(tokens[0], "GET_PESO") == 0) {
                int32_t raw;
                if (hx711_read_average(&s_hx711_dev, 5, &raw) == ESP_OK) {
                    float peso = (float)(raw - s_tara_adc) / s_factor_kg;
                    char resp[32];
                    snprintf(resp, sizeof(resp), "PESO:%.2f kg\n", peso);
                    uart_write_bytes(UART_NUM_0, resp, strlen(resp));
                } else {
                    uart_write_bytes(UART_NUM_0, "ERR\n", 4);
                }
            }
        }
    }
}



//+++++++++++++++++++++++++++++++++++++++++++++++++HX711
void task_monitoreo_estante(void *params) {
    s_hx711_dev.dout = DOUT_PIN;
    s_hx711_dev.pd_sck = PD_SCK_PIN;
    s_hx711_dev.gain = HX711_GAIN_A_128;

    gpio_set_level(PD_SCK_PIN, 0);   // mantener SCK bajo
    vTaskDelay(pdMS_TO_TICKS(100));  // 100 ms adicionales
    
    esp_err_t init_err = hx711_init(&s_hx711_dev);
    if (init_err != ESP_OK) {
        ESP_LOGW("BASCULA", "Error inicializando HX711: 0x%x. Continuando...", init_err);
    }

    ESP_LOGI("BASCULA", "HX711 listo. Tara=%ld  Factor=%.2f", (long)s_tara_adc, s_factor_kg);

    int32_t raw_adc = 0;
    int32_t last_valid_adc = 0;  // Guardar última lectura válida

    while (1) {
        bool ready = false;
        hx711_is_ready(&s_hx711_dev, &ready);
        
        esp_err_t read_err = hx711_read_average(&s_hx711_dev, 3, &raw_adc);
        
        if (read_err == ESP_OK && raw_adc != 0) {  // Ignorar ceros (errores)
            last_valid_adc = raw_adc;  // Guardar como válida
            int32_t adc_tareado = raw_adc - s_tara_adc;
            float peso_kg = (float)adc_tareado / s_factor_kg;
            
            ESP_LOGI("BASCULA", "ADC: raw=%ld, tara=%ld, tareado=%ld, peso=%.3f kg", 
                     raw_adc, s_tara_adc, adc_tareado, peso_kg);
        } else if (read_err != ESP_OK) {
            ESP_LOGW("BASCULA", "Error lectura: 0x%x", read_err);
        }
        // Si raw_adc == 0, simplemente no imprimimos (es un error)
        
        vTaskDelay(pdMS_TO_TICKS(500));  // Espera 500ms entre lecturas
    }
}
//++++++++++++++++++++++funciones   

char **parse_input(char *line){
    char **tokens = malloc(6 * sizeof(char*));
    char *token; 
    int position=0;

    //separamos los tokens por ':' que es el delimitador 
    token = strtok(line, ":");

    while(token != NULL){
        //guarda la utlima posicion para el NULL, indicando el final de los tokens. 
        if(position >= 5 ) break;
        tokens[position++] = strdup(token);
        token = strtok(NULL, ":");
    }

    tokens[position] = NULL;
    
    return tokens;
}


//puede que aqui este el error en el otro codigo. 
esp_err_t update_setup_cred(char *ssid, char *pswd, char *user, char *type){
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
            uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
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
            uart_write_bytes(UART_NUM_0, "ERR:NO_MEM\n", 11);
            return ESP_FAIL;
        }
    }

    return ESP_FAIL;
}

static esp_err_t load_calibration(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("CALIB", "No hay datos de calibración, usando valores por defecto");
        return ESP_OK;
    }
    if (err != ESP_OK) return err;

    nvs_get_i32(nvs, NVS_KEY_TARA, &s_tara_adc);
    float factor = 1.0f;
    
    // CREAR VARIABLE DE TAMAÑO Y PASAR SU DIRECCIÓN (&)
    size_t required_size = sizeof(float);
    if (nvs_get_blob(nvs, NVS_KEY_FACTOR, &factor, &required_size) == ESP_OK) {
        s_factor_kg = factor;
    }
    
    nvs_close(nvs);
    ESP_LOGI("CALIB", "Cargado: tara=%ld  factor=%.2f u/kg", (long)s_tara_adc, s_factor_kg);
    return ESP_OK;
}

static esp_err_t save_calibration(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    nvs_set_i32(nvs, NVS_KEY_TARA, s_tara_adc);
    nvs_set_blob(nvs, NVS_KEY_FACTOR, &s_factor_kg, sizeof(float));
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI("CALIB", "Guardado: tara=%ld  factor=%.2f", (long)s_tara_adc, s_factor_kg);
    return ESP_OK;
}