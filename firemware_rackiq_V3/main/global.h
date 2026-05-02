// #ifndef GLOBAL_H
// #define GLOBAL_H

// //funciones varibales que utilizaran las diferencues funciones, o funciones genreales para los distintos archivos

// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"
// #include "driver/uart.h"

// #define TRUE 1
// #define FALSE 0

// //definamos grupos de eventos para procesos en wifi que son necesarios 

// #define WIFI_CONNECTED_BIT BIT0 //indica que se pudo conecya r
// #define WIFI_FAIL_BIT BIT1 //indica que no se pudo conectar 
// #define WIFI_UPDATE BIT10 //idica que las credenciales fueron actualizadas
// #define BREAK_UPDATE_WIFI BIT11 //indicamos que debemos de romper el flujo del sistrema para conectarnos a la nueva red actualizada 

// #define WIFI_CREDS_READY BIT12  
// #define BROKER_RECEIVED BIT3  


// #define ESP_MAX_RETRY 5

// //pines para el hx711
// #define DOUT_PIN   GPIO_NUM_4
// #define PD_SCK_PIN GPIO_NUM_5



// //cola que controlara el flujo de la ifnromacion entre los diferners archivos 
// extern QueueHandle_t flow_data_queue; 

// typedef struct {
//     uart_port_t NUM_PORT;
// }task_uart_port_t;

// //una red wifi normal
// typedef struct {
//     const char *alias;
//     const char *ssid;
//     const char *pswd;
// } wifi_default_profile_t;

// //para un perfil de uan red wifi de empresa - universidad
// typedef struct {
//     const char *alias;
//     const char *ssid;
//     const char *user;
//     const char *pswd;
// } wifi_default_ent_profile_t;






// #endif



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifndef GLOBAL_H
#define GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#define TRUE 1
#define FALSE 0

// ─── Bits del grupo de eventos WiFi ───────────────────────────────────────────
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1
#define WIFI_UPDATE          BIT10
#define BREAK_UPDATE_WIFI    BIT11
#define WIFI_CREDS_READY     BIT12
#define BROKER_RECEIVED      BIT3

// ─── Bits del grupo de eventos HX711 ──────────────────────────────────────────
// Cuando este bit está activo, task_hx711_uart deja de imprimir.
// Lo ponemos nosotros antes de calibrar y lo quitamos al terminar.
#define HX711_PAUSE_PRINT    BIT4

// La tarea de calibración espera este bit para saber que el usuario
// mandó el comando TARE (báscula vacía, lista para tarar).
#define HX711_CAL_TARE_READY BIT5

// La tarea de calibración espera este bit para saber que el usuario
// mandó CAL:<peso> y ya tenemos el peso de referencia en g_cal_weight_kg.
#define HX711_CAL_WEIGHT_OK  BIT6

// ─── Constantes generales ─────────────────────────────────────────────────────
#define ESP_MAX_RETRY 5

// ─── Pines HX711 ──────────────────────────────────────────────────────────────
#define DOUT_PIN   GPIO_NUM_4
#define PD_SCK_PIN GPIO_NUM_5

// ─── Colas ────────────────────────────────────────────────────────────────────
extern QueueHandle_t flow_data_queue;

// ─── Variable compartida para calibración ────────────────────────────────────
// task_cmd_uart escribe aquí el peso de referencia cuando recibe CAL:<x>.
// task_hx711_calibrate lo lee para calcular la escala.
extern float g_cal_weight_kg;

// ─── Estructuras ──────────────────────────────────────────────────────────────
typedef struct {
    uart_port_t NUM_PORT;
} task_uart_port_t;

typedef struct {
    const char *alias;
    const char *ssid;
    const char *pswd;
} wifi_default_profile_t;

typedef struct {
    const char *alias;
    const char *ssid;
    const char *user;
    const char *pswd;
} wifi_default_ent_profile_t;

#endif

