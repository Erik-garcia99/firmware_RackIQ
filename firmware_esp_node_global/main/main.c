/**
 * en este proyecto no habra logs ya que el ESP no tendra coenxion con una pantalla
 * el unico que puede tener conexion a una pantalla seria la rasberry, por medio de uart o de mqtt
 * vamos a tramisitri logs, o aspectos importantes del esp, puede que aspectos de bajo nivel 
 * otro conceptos se deberan de mostrar en la app 
 * 
*/

//libereias estandars
#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>

//freertos
#include<freertos/FreeRTOS.h>


//drivers
#include<driver/uart.h>

//logs
#include<esp_log.h>
#include <esp_err.h>

//wifi
#include<esp_wifi.h>
#include<nvs_flash.h>
#include<lwip/err.h>
#include<lwip/sys.h>


//libreria interna 
#include"modules/WIFI/wifi_lib.h"
#include"modules/UART/uart_lib.h"
#include "modules/HX711/hx711_lib.h"
#include"global.h"



//++++++++++++++++ colas 
//cola para los eventos de UART
QueueHandle_t uart_event;
//cola que manerjara el flujo de datos de UART 
QueueHandle_t flow_data_queue;

//++++++++++++++++ grupos de eventos. 

EventGroupHandle_t s_wifi_event_group;


//++++++++++++++++++  estrucutra
//contiene valores de UART 
task_uart_port_t global_uart;
//estucuturua para red 
esp_wifi_t esp_wifi;
//+++++++++++++++++ vairbales globales

char broker_ip[64] = {0};

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
 * 
 * @param line recibe un apuntador a los datos que ingresaron por UART 
 *
 * @return regresa la lista de tokens  
 * 
*/ 
char **pasrse_input(char *line);


/**
 * @brief funcion encargada de actualizar las distittas credencuales mediante comandos 
 * 
 * 
 * @param key donde va el nombre del nuevo SSID o la nueva IP.
 * @param anchor deberia de ir la contrasenia de la nueva red o el nombre del usuario en caso de ser una red de empresa
 * @param pswd_ent si este tiene datos es que se actualizara una red de empresa o unversidad, va la contrasenia del usuario
 * 
 * @param identificator le dira a la funcion que tiepo de actualizacion sera, puede ser <SSID, HOST_IP, SSID_ENT, MAT>
 * 
 * @return ESP_OK si se pudo actualziar 
 * @return ESP_FAIL si no fue posible 
 * 
 * 
 */

/**
  * la funcion tratara de ser lo mas general, los parametros que no seran requeridos se les asginara un 0 o NULL 
  * 
  */
esp_err_t update_setup_cred(char *key, char *anchor ,char *pswd_ent,  char *identificator);

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
    xTaskCreate(task_uart, "task_uart", 4096,&global_uart, 9, NULL);

    xTaskCreate(task_cmd_uart, "task_cmd_uart", 4096, NULL, 8, NULL);

    //inicamos stack WIFI, solo hardware sin conectar 
    wifi_stack_init();

    //tarea que maneja conexion con wifi - conexion con mqtt idicnadoq que el esp
    //esta listo para operar 
    xTaskCreate(task_wifi_manager, "task_wifi_mgr", 4096, NULL, 7, NULL);
    
    xTaskCreate(task_sensors, "task_sensors", 4098, NULL, 8, NULL);

    /**
     * al encender el esp32 por primera lo que va a relaizar es mandar a la rasberry pi, que necesita las credenciales WIFI para poder conectarse al broker 
     * 
     * por lo que necesitaremos un fromato de comunicacion, va ser uno sensicllo no la gran cosa. 
     * 
     */

    //para solicitar las credenciales de WIFI necesitamos un protocolo basico
    //mandamos la solcitud
    char req_wifi[20];
    int len = snprintf(req_wifi, sizeof(req_wifi), "REQ_WIFI\n");
    uart_write_bytes(global_uart.NUM_PORT, req_wifi, len);


    //por lo que ahora lo que va a apsar es que vamoa a tener que esperar la respuesta y establecer las credencuales y publicamos por el broker que ya nos pudimos conectar. 



    //itentamos conectarnos a wifi
    // wifi_init_sta();
}

void task_wifi_manager(void *params) {

    
    
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CREDS_READY, pdTRUE, pdTRUE, portMAX_DELAY);

        wifi_connect();

    waiting_for_broker = 1;
    uart_write_bytes(UART_MAIN, "REQUEST_BROKER\n", 15);


    // 4. esperar la IP del broker (la setea task_cmd_uart al recibirla)
    xEventGroupWaitBits(s_wifi_event_group, BROKER_RECEIVED, pdFALSE, pdTRUE, portMAX_DELAY);
    waiting_for_broker = 0;

    //conectar MQTT
    //mqtt_init(broker_ip, 1883);
    //mqtt_publish("red/esp_LS", "connected");

    vTaskDelete(NULL);
}





/**
 * que espera UART?
 * 
 * --el esp estara esperando el comando de confirugracion de wifi 
 *  --- pero uart lo que recibira es un JSON de la sigueitne forma: {"shelf_id": 3, "weight": 142.5}\n
 * 
 * -- ip del broker 
 * 
 * 
*/
void task_cmd_uart(void *params){

    char* cmd_receive;
    esp_err_t ret;

    while(1){

        //esta tarea espera a que se reciban los datos de la tarea que se encarga de monitaorear UART
        //esta tarea se encargara de ver que entro y seguir con el flujo 
        if(xQueueReceive(flow_data_queue, &cmd_receive, portMAX_DELAY)){


            // esperando la IP del brocker
            if(waiting_for_broker) {
                strncpy(broker_ip, cmd_receive, sizeof(broker_ip) - 1);
                broker_ip[sizeof(broker_ip) - 1] = '\0';
                xEventGroupSetBits(s_wifi_event_group, BROKER_RECEIVED);
                // ESP_LOGI(TAG, "broker IP recibida: %s", broker_ip);
                free(cmd_receive);
                continue;
            }

            //se recibio una cadena con los comandos, por lo que es necesario parsear la cadena en sus diferentes tokens 
            //se hace uso de esta funcion que tokenisa la cadena y devuvle los comandos si son correctos el programa debe de seguir  
            char **tokens = pasrse_input(cmd_receive);

            //verificando que el fromato de recepcion sea el correcot
            if(tokens == NULL || tokens[0] == NULL || tokens[1] == NULL) {
                uart_write_bytes(UART_MAIN, "\r\nERR:FORMAT\r\n", 11);
                if(tokens) {
                    for(int i = 0; tokens[i] != NULL; i++) free(tokens[i]);
                    free(tokens);
                }
                free(cmd_receive);
                continue;
            }            

            char *ssid_ptr = strchr(tokens[0], ':');
            char *pswd_ptr = strchr(tokens[1], ':');

            if(ssid_ptr == NULL || pswd_ptr == NULL) {
                uart_write_bytes(UART_MAIN, "ERR:FORMAT\n", 11);
                for(int i = 0; tokens[i] != NULL; i++) free(tokens[i]);
                free(tokens);
                free(cmd_receive);
                continue;
            }

            ssid_ptr++;   // saltar ':'
            pswd_ptr++;

            ret = update_setup_cred(ssid_ptr, pswd_ptr, NULL, "SSID");
            if(ret == ESP_OK) {
                // avisa a task_wifi_manager que ya puede intentar conectarse
                xEventGroupSetBits(s_wifi_event_group, WIFI_CREDS_READY);
            } else {
                uart_write_bytes(UART_MAIN, "ERR:CREDS\n", 10);
            }

            for(int i = 0; tokens[i] != NULL; i++) free(tokens[i]);
            free(tokens);
            free(cmd_receive);
        }
    }
}


char **pasrse_input(char *line){

    char **tokens = malloc(5 * sizeof(char*));
    char *token; 
    int position=0;

    //separamos los tokens por ',' que es el delimitador de JSON 
    token = strtok(line, " ");

    while(token != NULL){

        if(position >= 5 ) break;

        tokens[position++] = strdup(token);
        
        token = strtok(NULL, " ");
    }

    tokens[position] = NULL;
    
    
    return tokens;
}

//ahora esta conexion sera mediante la estrucuutra para lleva run mejor controky que todo este agrupado en una sola varibale 
esp_err_t update_setup_cred(char *key, char *anchor, char *pswd_ent, char *identificator) {

    if(strcmp(identificator, "SSID") == 0) {

        esp_wifi.ssid = realloc(esp_wifi.ssid, strlen(key)    + 1);
        esp_wifi.pswd = realloc(esp_wifi.pswd, strlen(anchor) + 1);

        if(esp_wifi.ssid != NULL && esp_wifi.pswd != NULL) {
            strcpy(esp_wifi.ssid, key);
            strcpy(esp_wifi.pswd, anchor);
            esp_wifi.type_connected = 0;
            esp_wifi.user_name = NULL;
            // ESP_LOGI(TAG, "credenciales actualizadas - SSID: %s", esp_wifi.ssid);
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
        int len = snprintf(pasrse_input, sizeof(pasrse_input), "Estante 1: %.2f gramos\n", current_weight);
        //pendiente mqtt
        


        vTaskDelay(pdMS_TO_TICKS(1000)); // Leer cada segundo
    }
}