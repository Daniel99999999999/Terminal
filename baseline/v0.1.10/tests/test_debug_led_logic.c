#include <stdint.h>
#include <stdio.h>
#include "debug_led_logic.h"

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        return 1; \
    } \
} while (0)

static int disconnected_timing(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 0));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 499));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 500));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 999));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 1000));
    return 0;
}

static int connected_timing(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 100));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 1099));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 1100));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 2100));
    return 0;
}

static int mode_change_restarts_phase(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 0));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 500));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 600));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 1599));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 1600));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 1700));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 2200));
    return 0;
}

static int error_is_solid_and_recovery_restarts_phase(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 0));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 500));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_ERROR, 600));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_ERROR, 10000));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_ERROR, 11000));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 12000));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_CONNECTED, 13000));
    return 0;
}

static int delayed_callback_does_not_accumulate_drift(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 0));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 1790));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 2000));
    return 0;
}

static int millisecond_counter_wrap(void)
{
    debug_led_pattern_t p = {0};
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, UINT32_MAX - 200));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 298));
    CHECK(!debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 299));
    CHECK(debug_led_pattern_step(&p, DEBUG_LED_DISCONNECTED, 799));
    return 0;
}

static int unstarted_tasks_are_not_reported_as_hung(void)
{
    debug_led_health_t h = {0};
    CHECK(debug_led_health_stale(&h, 100000) == 0);
    return 0;
}

static int uart_blocked_task_detection(void)
{
    debug_led_health_t h = {0};
    debug_led_health_beat(&h, DEBUG_LED_UART_RX, 100);
    CHECK(debug_led_health_stale(&h, 2099) == 0);
    CHECK(debug_led_health_stale(&h, 2100) == (1U << DEBUG_LED_UART_RX));
    debug_led_health_beat(&h, DEBUG_LED_UART_RX, 2200);
    CHECK(debug_led_health_stale(&h, 2200) == 0);
    return 0;
}

static int spp_recovery_wait_is_healthy(void)
{
    debug_led_health_t h = {0};
    debug_led_health_beat(&h, DEBUG_LED_BT_TX, 0);
    CHECK(debug_led_health_stale(&h, 5000) == 0);
    debug_led_health_beat(&h, DEBUG_LED_BT_TX, 5000);
    CHECK(debug_led_health_stale(&h, 10000) == 0);
    CHECK(debug_led_health_stale(&h, 10999) == 0);
    CHECK(debug_led_health_stale(&h, 11000) == (1U << DEBUG_LED_BT_TX));
    return 0;
}

static int health_wrap_and_independent_channels(void)
{
    debug_led_health_t h = {0};
    debug_led_health_beat(&h, DEBUG_LED_UART_TX, UINT32_MAX - 1000);
    debug_led_health_beat(&h, DEBUG_LED_BT_TX, UINT32_MAX - 1000);
    CHECK(debug_led_health_stale(&h, 998) == 0);
    CHECK(debug_led_health_stale(&h, 999) == (1U << DEBUG_LED_UART_TX));
    CHECK(debug_led_health_stale(&h, 4999) ==
          ((1U << DEBUG_LED_UART_TX) | (1U << DEBUG_LED_BT_TX)));
    return 0;
}

static int terminal_idle_heartbeat_and_stall(void)
{
    debug_led_health_t h = {0};
    for (uint32_t now = 0; now <= 2000; now += 250) {
        debug_led_health_beat(&h, DEBUG_LED_TERMINAL, now);
        CHECK(debug_led_health_stale(&h, now) == 0);
    }
    CHECK(debug_led_health_stale(&h, 3999) == 0);
    CHECK(debug_led_health_stale(&h, 4000) == (1U << DEBUG_LED_TERMINAL));
    return 0;
}

int main(void)
{
    int (*tests[])(void) = {
        disconnected_timing, connected_timing, mode_change_restarts_phase,
        error_is_solid_and_recovery_restarts_phase,
        delayed_callback_does_not_accumulate_drift, millisecond_counter_wrap,
        unstarted_tasks_are_not_reported_as_hung, uart_blocked_task_detection,
        spp_recovery_wait_is_healthy, health_wrap_and_independent_channels,
        terminal_idle_heartbeat_and_stall,
    };
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i]()) {
            return 1;
        }
    }
    puts("PASS: 11 LED timing/heartbeat regression scenarios");
    return 0;
}
