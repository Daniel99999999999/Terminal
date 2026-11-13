#include "debug_led_logic.h"

bool debug_led_pattern_step(debug_led_pattern_t *pattern, debug_led_mode_t mode,
                            uint32_t now_ms)
{
    if (!pattern->initialized || pattern->mode != mode) {
        pattern->initialized = true;
        pattern->mode = mode;
        pattern->phase_ms = now_ms;
        pattern->on = true;
    }
    if (mode == DEBUG_LED_ERROR) {
        return true;
    }
    uint32_t half_period = mode == DEBUG_LED_CONNECTED ? 1000 : 500;
    uint32_t steps = (uint32_t)(now_ms - pattern->phase_ms) / half_period;
    if (steps) {
        if (steps & 1) {
            pattern->on = !pattern->on;
        }
        pattern->phase_ms += steps * half_period;
    }
    return pattern->on;
}

void debug_led_health_beat(debug_led_health_t *health, debug_led_channel_t channel,
                           uint32_t now_ms)
{
    if ((unsigned)channel < DEBUG_LED_CHANNEL_COUNT) {
        health->last_ms[channel] = now_ms;
        health->watched |= 1U << channel;
    }
}

uint32_t debug_led_health_stale(const debug_led_health_t *health, uint32_t now_ms)
{
    uint32_t stale = 0;
    for (unsigned channel = 0; channel < DEBUG_LED_CHANNEL_COUNT; ++channel) {
        if ((health->watched & (1U << channel)) &&
            (uint32_t)(now_ms - health->last_ms[channel]) >= 2000) {
            stale |= 1U << channel;
        }
    }
    return stale;
}
