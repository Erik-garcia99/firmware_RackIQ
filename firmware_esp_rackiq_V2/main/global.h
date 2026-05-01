#ifndef GLOBAL_H
#define GLOBAL_H

//funciones varibales que utilizaran las diferencues funciones, o funciones genreales para los distintos archivos

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#define TRUE 1
#define FALSE 0

//definamos grupos de eventos para procesos en wifi que son necesarios 

#define WIFI_CONNECTED_BIT BIT0 //indica que se pudo conecya r
#define WIFI_FAIL_BIT BIT1 //indica que no se pudo conectar 
#define WIFI_UPDATE BIT10 //idica que las credenciales fueron actualizadas
#define BREAK_UPDATE_WIFI BIT11 //indicamos que debemos de romper el flujo del sistrema para conectarnos a la nueva red actualizada 

#define WIFI_CREDS_READY BIT12  
#define BROKER_RECEIVED BIT3  


#define ESP_MAX_RETRY 5

//pines para el hx711
#define DOUT_PIN   GPIO_NUM_4
#define PD_SCK_PIN GPIO_NUM_5

// Parámetros para el Debouncing (ajustables según tus pruebas físicas)
#define UMBRAL_RUIDO 5000       // Variación mínima "cruda" para considerar que alguien tocó el estante
#define CICLOS_ESTABILIZACION 3 // Cuántos ciclos seguidos debe mantenerse igual el peso para consolidarlo


//cola que controlara el flujo de la ifnromacion entre los diferners archivos 
extern QueueHandle_t flow_data_queue; 

typedef struct {
    uart_port_t NUM_PORT;
}task_uart_port_t;

//una red wifi normal
typedef struct {
    const char *alias;
    const char *ssid;
    const char *pswd;
} wifi_default_profile_t;

//para un perfil de uan red wifi de empresa - universidad
typedef struct {
    const char *alias;
    const char *ssid;
    const char *user;
    const char *pswd;
} wifi_default_ent_profile_t;





#endif