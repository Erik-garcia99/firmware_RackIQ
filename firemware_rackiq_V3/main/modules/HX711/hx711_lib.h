#ifndef HX711_LIB_H
#define HX711_LIB_H

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    gpio_num_t dout;
    gpio_num_t sck;

    int32_t offset;
    float scale;

    float weight;              // valor filtrado
    SemaphoreHandle_t mutex;   // protección

    // -------- estabilidad --------
    float last_weight;
    float stable_weight;
    int stable_time_ms;
    bool is_stable;

} hx711_t;

// Inicialización
void hx711_init(hx711_t *hx, gpio_num_t dout, gpio_num_t sck);

// Lectura base
int32_t hx711_read(hx711_t *hx);
int32_t hx711_read_average(hx711_t *hx, int times);

// Calibración
void hx711_tare(hx711_t *hx, int times);
void hx711_set_scale(hx711_t *hx, float scale);

// Peso directo
float hx711_get_units(hx711_t *hx, int times);

// -------- MULTITAREA --------
void hx711_start_task(hx711_t *hx);
float hx711_get_weight(hx711_t *hx); // thread-safe

// NVS
bool hx711_save_calibration(hx711_t *hx);
bool hx711_load_calibration(hx711_t *hx);

bool hx711_is_stable(hx711_t *hx);
float hx711_get_stable_weight(hx711_t *hx);

#endif