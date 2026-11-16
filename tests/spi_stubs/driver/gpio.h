#pragma once
#include <stdint.h>
#include "esp_err.h"
#define GPIO_MODE_OUTPUT 2
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_ANYEDGE 3
typedef struct {
    uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
} gpio_config_t;
esp_err_t gpio_set_level(int pin, unsigned level);
esp_err_t gpio_set_direction(int pin, int direction);
esp_err_t gpio_config(const gpio_config_t *config);
int gpio_get_level(int pin);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(int pin, void (*handler)(void *), void *ctx);
