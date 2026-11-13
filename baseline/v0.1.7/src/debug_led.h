#pragma once

#include "esp_attr.h"
#include "esp_err.h"
#include "debug_led_logic.h"

/* NodeMCU-32 V1.3 reference schematic: blue LED1, GPIO2 -> 470R -> LED -> GND.
 * Verify the actual board. These definitions can be overridden by build flags. */
#ifndef DEBUG_LED_GPIO
#define DEBUG_LED_GPIO 2
#endif
#ifndef DEBUG_LED_ACTIVE_LEVEL
#define DEBUG_LED_ACTIVE_LEVEL 1
#endif

typedef debug_led_mode_t (*debug_led_mode_provider_t)(void);

esp_err_t debug_led_init(debug_led_mode_provider_t provider);
void debug_led_heartbeat(debug_led_channel_t channel);
/* Latched until reset. ISR/panic-safe, no allocation, locks or logging. */
void IRAM_ATTR debug_led_fault(void);
