#ifndef GLOBAL_H
#define GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#define TRUE 1
#define FALSE 0

// ─── Bits del grupo de eventos WiFi ─────────────────────
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1
#define WIFI_UPDATE          BIT10
#define BREAK_UPDATE_WIFI    BIT11
#define WIFI_CREDS_READY     BIT12
#define BROKER_RECEIVED      BIT3   // usado para ACK de SET_WIFI

// ─── Bits del grupo de eventos HX711 ────────────────────
#define HX711_PAUSE_PRINT    BIT4
#define HX711_CAL_TARE_READY BIT5
#define HX711_CAL_WEIGHT_OK  BIT6

// ─── Nuevos bits ────────────────────────────────────────
#define WIFI_CREDS_RECEIVED  BIT13  // El servidor HTTP capturó SSID/PSWD
// BROKER_IP_RECEIVED eliminado

// ─── Constantes generales ───────────────────────────────
#define ESP_MAX_RETRY 5

// ─── Pines HX711 ────────────────────────────────────────
#define DOUT_PIN   GPIO_NUM_4
#define PD_SCK_PIN GPIO_NUM_5

// ─── Roles de dispositivo ───────────────────────────────
#define ROLE_MASTER  1
#define ROLE_SLAVE   2
#define ROLE_PIN  GPIO_NUM_23

extern uint8_t g_device_role;

// ─── Colas ──────────────────────────────────────────────
extern QueueHandle_t flow_data_queue;
extern QueueHandle_t uart1_tx_queue;

// ─── Variable compartida para calibración ──────────────
extern float g_cal_weight_kg;

// ─── Estructuras ────────────────────────────────────────
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

extern char g_wifi_ssid[33];
extern char g_wifi_pass[65];
extern char g_broker_ip[16];

extern esp_err_t nvs_save_str(const char *key, const char *value);
extern esp_err_t nvs_load_str(const char *key, char *buf, size_t len);

#endif