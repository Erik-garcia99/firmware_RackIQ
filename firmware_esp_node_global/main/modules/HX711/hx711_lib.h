#ifndef HX711_LIB_H
#define HX711_LIB_H

#include <stdint.h>
#include <driver/gpio.h>
#include <esp_err.h>

/**
 * @brief Estructura para agrupar la configuración de un módulo HX711
 */
typedef struct {
    gpio_num_t dout_pin;   // Pin de datos
    gpio_num_t pd_sck_pin; // Pin de reloj
    uint8_t gain;          // Ganancia (128, 64 o 32)
    int32_t offset;        // Valor de tara (peso cero)
    float scale;           // Factor de escala (para convertir a gramos/kilos)
} hx711_t;

/**
 * @brief Inicializa los pines GPIO para el HX711
 */
esp_err_t hx711_init(hx711_t *hx);

/**
 * @brief Lee el valor crudo (24 bits) del ADC del HX711
 * @return Valor crudo con signo. Si regresa 0, puede indicar error de lectura.
 */
int32_t hx711_read_raw(hx711_t *hx);

/**
 * @brief Realiza varias lecturas y saca el promedio para estabilizar el valor
 */
int32_t hx711_read_average(hx711_t *hx, uint8_t times);

/**
 * @brief Devuelve el peso ya convertido a tus unidades usando el offset y scale
 */
float hx711_get_weight(hx711_t *hx, uint8_t times);

#endif