#include "app_health.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "soc/gpio_struct.h"

/* GPIO2 is the on-board active-high LED; never touch the memory pins. */
static uint32_t s_fault, s_ready, s_connected;
static uint32_t s_last[HEALTH_COUNT], s_watched;
static esp_timer_handle_t s_timer;

void app_health_fault(void)
{
    __atomic_store_n(&s_fault, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&s_ready, __ATOMIC_ACQUIRE)) { GPIO.out_w1ts = 1U << 2; }
}

/* Official task-watchdog extension point, no linker interception. */
void IRAM_ATTR esp_task_wdt_isr_user_handler(void) { app_health_fault(); }

void app_health_connected(bool connected)
{
    __atomic_store_n(&s_connected, connected, __ATOMIC_RELEASE);
}

void app_health_beat(health_channel_t channel)
{
    if ((unsigned)channel >= HEALTH_COUNT) { return; }
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    __atomic_store_n(&s_last[channel], now, __ATOMIC_RELEASE);
    __atomic_fetch_or(&s_watched, 1U << channel, __ATOMIC_RELEASE);
}

static void tick(void *arg)
{
    (void)arg;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t watched = __atomic_load_n(&s_watched, __ATOMIC_ACQUIRE);
    for (unsigned i = 0; i < HEALTH_COUNT; ++i) {
        if ((watched & (1U << i)) &&
            now - __atomic_load_n(&s_last[i], __ATOMIC_ACQUIRE) >= 2000) {
            app_health_fault();
        }
    }
    bool fault = __atomic_load_n(&s_fault, __ATOMIC_ACQUIRE);
    unsigned half_period = __atomic_load_n(&s_connected, __ATOMIC_ACQUIRE) ? 1000 : 500;
    bool on = fault || ((now / half_period) & 1U) == 0;
    if (on) { GPIO.out_w1ts = 1U << 2; } else { GPIO.out_w1tc = 1U << 2; }
    /* Preserve an ISR fault that raced the OFF write. */
    if (__atomic_load_n(&s_fault, __ATOMIC_ACQUIRE)) { GPIO.out_w1ts = 1U << 2; }
}

esp_err_t app_health_init(void)
{
    gpio_config_t cfg = { .pin_bit_mask = 1ULL << 2, .mode = GPIO_MODE_OUTPUT };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) { return err; }
    __atomic_store_n(&s_ready, 1, __ATOMIC_RELEASE);
    GPIO.out_w1ts = 1U << 2;
    const esp_timer_create_args_t args = { .callback = tick, .name = "health" };
    err = esp_timer_create(&args, &s_timer);
    return err == ESP_OK ? esp_timer_start_periodic(s_timer, 50000) : err;
}
