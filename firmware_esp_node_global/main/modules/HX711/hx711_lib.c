#include "hx711_lib.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>
#include <esp_log.h>

static const char *TAG = "HX711";

esp_err_t hx711_init(hx711_t *hx) {

    gpio_config_t io_conf_sck = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << hx->pd_sck_pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf_sck);


    gpio_config_t io_conf_dout = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << hx->dout_pin),
        .pull_down_en = 0,
        .pull_up_en = 0 
    };
    gpio_config(&io_conf_dout);

    gpio_set_level(hx->pd_sck_pin, 0);

    if(hx->gain == 0) hx->gain = 128;
    hx->offset = 0;
    hx->scale = 1.0;

    ESP_LOGI(TAG, "HX711 inicializado en pines DOUT:%d, SCK:%d", hx->dout_pin, hx->pd_sck_pin);
    return ESP_OK;
}

static bool hx711_is_ready(hx711_t *hx) {
    // El HX711 está listo cuando DOUT se pone en bajo
    return gpio_get_level(hx->dout_pin) == 0;
}

int32_t hx711_read_raw(hx711_t *hx) {
    // Esperar a que el chip esté listo (timeout de seguridad)
    uint32_t timeout = 0;
    while (!hx711_is_ready(hx)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout++;
        if(timeout > 100) {
            ESP_LOGE(TAG, "Timeout esperando al HX711");
            return 0;
        }
    }

    uint32_t count = 0;
    
    // Deshabilitar interrupciones brevemente para no arruinar el timing de los pulsos
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&mux);

    // Leer los 24 bits
    for (int i = 0; i < 24; i++) {
        gpio_set_level(hx->pd_sck_pin, 1);
        ets_delay_us(1);
        
        count = count << 1;
        gpio_set_level(hx->pd_sck_pin, 0);
        ets_delay_us(1);
        
        if (gpio_get_level(hx->dout_pin)) {
            count++;
        }
    }

    // Pulsos extra para configurar la ganancia de la siguiente lectura
    uint8_t p = 1; 
    if(hx->gain == 64) p = 3;
    if(hx->gain == 32) p = 2;

    for (int i = 0; i < p; i++) {
        gpio_set_level(hx->pd_sck_pin, 1);
        ets_delay_us(1);
        gpio_set_level(hx->pd_sck_pin, 0);
        ets_delay_us(1);
    }

    taskEXIT_CRITICAL(&mux);

    // Convertir a complemento a 2 (el HX711 manda datos con signo de 24 bits)
    if (count & 0x800000) {
        count |= 0xFF000000;
    }

    return (int32_t)count;
}

int32_t hx711_read_average(hx711_t *hx, uint8_t times) {
    int64_t sum = 0;
    for (uint8_t i = 0; i < times; i++) {
        sum += hx711_read_raw(hx);
    }
    return (int32_t)(sum / times);
}

float hx711_get_weight(hx711_t *hx, uint8_t times) {
    int32_t raw_avg = hx711_read_average(hx, times);
    return (float)(raw_avg - hx->offset) / hx->scale;
}