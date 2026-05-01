#include "hx711_lib.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "HX711";
#define NVS_NAMESPACE "hx711"

#define STABLE_THRESHOLD 0.02f   // kg (20 gramos)
#define STABLE_TIME_MS   1000    // 1 segundo



// -------- LOW LEVEL --------

void hx711_init(hx711_t *hx, gpio_num_t dout, gpio_num_t sck) {
    hx->dout = dout;
    hx->sck = sck;

    hx->offset = 0;
    hx->scale = 1.0;
    hx->weight = 0.0;

    gpio_set_direction(hx->dout, GPIO_MODE_INPUT);
    gpio_set_direction(hx->sck, GPIO_MODE_OUTPUT);
    gpio_set_level(hx->sck, 0);

    hx->mutex = xSemaphoreCreateMutex();
}

static void hx711_wait_ready(hx711_t *hx) {
    while (gpio_get_level(hx->dout) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

int32_t hx711_read(hx711_t *hx) {
    int32_t data = 0;

    hx711_wait_ready(hx);

    for (int i = 0; i < 24; i++) {
        gpio_set_level(hx->sck, 1);
        data <<= 1;
        gpio_set_level(hx->sck, 0);

        if (gpio_get_level(hx->dout)) data++;
    }

    gpio_set_level(hx->sck, 1);
    gpio_set_level(hx->sck, 0);

    if (data & 0x800000) {
        data |= ~0xFFFFFF;
    }

    return data;
}

int32_t hx711_read_average(hx711_t *hx, int times) {
    int64_t sum = 0;
    for (int i = 0; i < times; i++) {
        sum += hx711_read(hx);
    }
    return sum / times;
}

void hx711_tare(hx711_t *hx, int times) {
    hx->offset = hx711_read_average(hx, times);
}

void hx711_set_scale(hx711_t *hx, float scale) {
    hx->scale = scale;
}

float hx711_get_units(hx711_t *hx, int times) {
    int32_t val = hx711_read_average(hx, times);
    return (val - hx->offset) / hx->scale;
}

//
// -------- TASK HX711 --------
//

static void hx711_task(void *arg) {
    hx711_t *hx = (hx711_t *)arg;

    float alpha = 0.2;
    const int dt = 100; // ms

    while (1) {
        float raw = hx711_get_units(hx, 5);

        xSemaphoreTake(hx->mutex, portMAX_DELAY);

        // Filtro EMA
        hx->weight = alpha * raw + (1 - alpha) * hx->weight;

        // -------- DETECCIÓN DE ESTABILIDAD --------
        float diff = fabs(hx->weight - hx->last_weight);

        if (diff < STABLE_THRESHOLD) {
            hx->stable_time_ms += dt;

            if (hx->stable_time_ms >= STABLE_TIME_MS) {
                hx->is_stable = true;
                hx->stable_weight = hx->weight;
            }
        } else {
            hx->stable_time_ms = 0;
            hx->is_stable = false;
        }

        hx->last_weight = hx->weight;

        xSemaphoreGive(hx->mutex);

        vTaskDelay(pdMS_TO_TICKS(dt));
    }
}

void hx711_start_task(hx711_t *hx) {
    xTaskCreate(
        hx711_task,
        "hx711_task",
        2048,
        hx,
        5,
        NULL
    );
}

float hx711_get_weight(hx711_t *hx) {
    float w;

    xSemaphoreTake(hx->mutex, portMAX_DELAY);
    w = hx->weight;
    xSemaphoreGive(hx->mutex);

    return w;
}

//
// -------- NVS --------
//

bool hx711_save_calibration(hx711_t *hx) {
    nvs_handle_t handle;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }

    nvs_set_i32(handle, "offset", hx->offset);
    nvs_set_blob(handle, "scale", &hx->scale, sizeof(float));
    nvs_commit(handle);

    nvs_close(handle);
    return true;
}

bool hx711_load_calibration(hx711_t *hx) {
    nvs_handle_t handle;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t size = sizeof(float);

    if (nvs_get_i32(handle, "offset", &hx->offset) != ESP_OK ||
        nvs_get_blob(handle, "scale", &hx->scale, &size) != ESP_OK) {

        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    return true;
}


bool hx711_is_stable(hx711_t *hx) {
    bool stable;

    xSemaphoreTake(hx->mutex, portMAX_DELAY);
    stable = hx->is_stable;
    xSemaphoreGive(hx->mutex);

    return stable;
}

float hx711_get_stable_weight(hx711_t *hx) {
    float w;

    xSemaphoreTake(hx->mutex, portMAX_DELAY);
    w = hx->stable_weight;
    xSemaphoreGive(hx->mutex);

    return w;
}