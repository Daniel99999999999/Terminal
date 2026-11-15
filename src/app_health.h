#pragma once
#include <stdbool.h>
#include "esp_attr.h"
#include "esp_err.h"

typedef enum { HEALTH_MAIN, HEALTH_UART1, HEALTH_SCREEN, HEALTH_COUNT } health_channel_t;
esp_err_t app_health_init(void);
void app_health_beat(health_channel_t channel);
void app_health_connected(bool connected);
void IRAM_ATTR app_health_fault(void);
