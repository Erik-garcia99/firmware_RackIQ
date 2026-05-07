#ifndef HTTP_SETUP_H
#define HTTP_SETUP_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

void start_http_setup_server(EventGroupHandle_t wifi_event_group);
void stop_http_setup_server(void);

#endif