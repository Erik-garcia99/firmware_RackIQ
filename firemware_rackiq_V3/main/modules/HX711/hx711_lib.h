// #ifndef HX711_LIB_H
// #define HX711_LIB_H

// #include "driver/gpio.h"
// #include <stdint.h>
// #include <stdbool.h>

// // FreeRTOS
// #include "freertos/FreeRTOS.h"
// #include "freertos/semphr.h"

// typedef struct {
//     gpio_num_t dout;
//     gpio_num_t sck;

//     int32_t offset;
//     float scale;

//     float weight;              // valor filtrado
//     SemaphoreHandle_t mutex;   // protección

//     // -------- estabilidad --------
//     float last_weight;
//     float stable_weight;
//     int stable_time_ms;
//     bool is_stable;

// } hx711_t;

// // Inicialización
// void hx711_init(hx711_t *hx, gpio_num_t dout, gpio_num_t sck);

// // Lectura base
// int32_t hx711_read(hx711_t *hx);
// int32_t hx711_read_average(hx711_t *hx, int times);

// // Calibración
// void hx711_tare(hx711_t *hx, int times);
// void hx711_set_scale(hx711_t *hx, float scale);

// // Peso directo
// float hx711_get_units(hx711_t *hx, int times);

// // -------- MULTITAREA --------
// void hx711_start_task(hx711_t *hx);
// float hx711_get_weight(hx711_t *hx); // thread-safe

// // NVS
// bool hx711_save_calibration(hx711_t *hx);
// bool hx711_load_calibration(hx711_t *hx);

// bool hx711_is_stable(hx711_t *hx);
// float hx711_get_stable_weight(hx711_t *hx);

// #endif



#ifndef HX711_LIB_H
#define HX711_LIB_H

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef struct {
    gpio_num_t dout;
    gpio_num_t sck;

    int32_t offset;
    float   scale;

    float weight;              // valor filtrado (EMA)
    SemaphoreHandle_t mutex;   // protege weight / is_stable / stable_weight

    // -------- estabilidad --------
    float last_weight;
    float stable_weight;
    int   stable_time_ms;
    bool  is_stable;

    // -------- handle de la tarea interna --------
    // NULL = tarea no arrancada todavía (primer boot sin NVS).
    // Válido = tarea corriendo o suspendida.
    TaskHandle_t task_handle;

} hx711_t;

// Inicialización
void hx711_init(hx711_t *hx, gpio_num_t dout, gpio_num_t sck);

// Lectura base (SIN protección de mutex — no llamar mientras la tarea corre)
int32_t hx711_read(hx711_t *hx);
int32_t hx711_read_average(hx711_t *hx, int times);

// Calibración (SIN protección de mutex — suspender tarea antes de llamar)
void hx711_tare(hx711_t *hx, int times);
void hx711_set_scale(hx711_t *hx, float scale);

// Peso directo (SIN protección de mutex — solo para uso en calibración)
float hx711_get_units(hx711_t *hx, int times);

// -------- MULTITAREA --------
// Arranca la tarea interna de lectura. Guarda el handle en hx->task_handle.
void hx711_start_task(hx711_t *hx);

// Suspende la tarea (para leer GPIO en calibración sin conflicto).
// Si task_handle == NULL no hace nada.
void hx711_suspend_task(hx711_t *hx);

// Reanuda la tarea después de una calibración.
// Si task_handle == NULL no hace nada.
void hx711_resume_task(hx711_t *hx);

// Lectura thread-safe del peso filtrado (usar durante operación normal)
float hx711_get_weight(hx711_t *hx);

// NVS
bool hx711_save_calibration(hx711_t *hx);
bool hx711_load_calibration(hx711_t *hx);

bool  hx711_is_stable(hx711_t *hx);
float hx711_get_stable_weight(hx711_t *hx);

#endif