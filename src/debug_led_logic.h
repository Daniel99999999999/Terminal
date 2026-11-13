#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DEBUG_LED_DISCONNECTED,
    DEBUG_LED_CONNECTED,
    DEBUG_LED_ERROR,
} debug_led_mode_t;

typedef enum {
    DEBUG_LED_UART_TX,
    DEBUG_LED_UART_RX,
    DEBUG_LED_TERMINAL,
    DEBUG_LED_HC_RX,
    DEBUG_LED_HC_TX,
    DEBUG_LED_HC_CONTROL,
    DEBUG_LED_LCD,
    DEBUG_LED_CHANNEL_COUNT,
} debug_led_channel_t;

typedef struct {
    debug_led_mode_t mode;
    uint32_t phase_ms;
    bool initialized;
    bool on;
} debug_led_pattern_t;

typedef struct {
    uint32_t watched;
    uint32_t last_ms[DEBUG_LED_CHANNEL_COUNT];
} debug_led_health_t;

/* Caller serializes access. All time differences tolerate uint32_t wrap. */
bool debug_led_pattern_step(debug_led_pattern_t *pattern, debug_led_mode_t mode,
                            uint32_t now_ms);
void debug_led_health_beat(debug_led_health_t *health, debug_led_channel_t channel,
                           uint32_t now_ms);
uint32_t debug_led_health_stale(const debug_led_health_t *health, uint32_t now_ms);
