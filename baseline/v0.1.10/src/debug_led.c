#include <stdint.h>

#include "debug_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/gpio_struct.h"

#if DEBUG_LED_GPIO < 0 || DEBUG_LED_GPIO > 33 || \
    (DEBUG_LED_GPIO >= 6 && DEBUG_LED_GPIO <= 11) || \
    DEBUG_LED_GPIO == 16 || DEBUG_LED_GPIO == 17 || \
    DEBUG_LED_GPIO == 1 || DEBUG_LED_GPIO == 3 || \
    DEBUG_LED_GPIO == 25 || DEBUG_LED_GPIO == 26 || DEBUG_LED_GPIO == 20 || \
    (DEBUG_LED_GPIO >= 28 && DEBUG_LED_GPIO <= 31)
#error "Debug LED GPIO conflicts with reserved/unavailable ESP32-WROVER pins"
#endif
#if DEBUG_LED_ACTIVE_LEVEL != 0 && DEBUG_LED_ACTIVE_LEVEL != 1
#error "DEBUG_LED_ACTIVE_LEVEL must be 0 or 1"
#endif

static DRAM_ATTR uint32_t s_gpio_ready;
static DRAM_ATTR uint32_t s_latched_fault;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static debug_led_health_t s_health;
static debug_led_pattern_t s_pattern;
static debug_led_mode_provider_t s_provider;
static esp_timer_handle_t s_timer;

static inline __attribute__((always_inline)) void set_led_level(bool on)
{
    bool level = on ? DEBUG_LED_ACTIVE_LEVEL : !DEBUG_LED_ACTIVE_LEVEL;
#if DEBUG_LED_GPIO < 32
    if (level) {
        GPIO.out_w1ts = 1U << DEBUG_LED_GPIO;
    } else {
        GPIO.out_w1tc = 1U << DEBUG_LED_GPIO;
    }
#else
    if (level) {
        GPIO.out1_w1ts.val = 1U << (DEBUG_LED_GPIO - 32);
    } else {
        GPIO.out1_w1tc.val = 1U << (DEBUG_LED_GPIO - 32);
    }
#endif
}

/* IRAM section is inherited from the declaration in debug_led.h. */
void debug_led_fault(void)
{
    __atomic_store_n(&s_latched_fault, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&s_gpio_ready, __ATOMIC_ACQUIRE)) {
        set_led_level(true);
    }
}

/* Called from the existing TWDT ISR; its configuration is left unchanged. */
void IRAM_ATTR esp_task_wdt_isr_user_handler(void)
{
    debug_led_fault();
}

void __attribute__((noreturn)) __real_esp_system_abort(const char *details);

void IRAM_ATTR __attribute__((noreturn)) __wrap_esp_system_abort(const char *details)
{
    debug_led_fault();
    __real_esp_system_abort(details);
}

void debug_led_heartbeat(debug_led_channel_t channel)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&s_lock);
    debug_led_health_beat(&s_health, channel, now_ms);
    portEXIT_CRITICAL(&s_lock);
}

static void update_led(void *arg)
{
    (void)arg;
    debug_led_mode_t mode = s_provider(); /* Provider takes its own short lock. */
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&s_lock);
    uint32_t stale = debug_led_health_stale(&s_health, now_ms);
    if (stale) {
        /* Set the latch before any log which could block on the console. */
        bool first_fault = !__atomic_load_n(&s_latched_fault, __ATOMIC_ACQUIRE);
        debug_led_fault();
        portEXIT_CRITICAL(&s_lock);
        if (first_fault) {
            ESP_LOGE("DEBUG_LED", "Heartbeat stale mask=0x%lx (TX=1 RX=2 BT=4); LED latched ON",
                     (unsigned long)stale);
        }
        portENTER_CRITICAL(&s_lock);
    }
    if (__atomic_load_n(&s_latched_fault, __ATOMIC_ACQUIRE)) {
        mode = DEBUG_LED_ERROR;
    }
    bool on = debug_led_pattern_step(&s_pattern, mode, now_ms);
    portEXIT_CRITICAL(&s_lock);

    set_led_level(on);
    /* An ISR on either core may have latched a fault after the snapshot.
     * Its ON write must not be left overwritten by this timer's OFF write. */
    if (__atomic_load_n(&s_latched_fault, __ATOMIC_ACQUIRE)) {
        set_led_level(true);
    }
}

esp_err_t debug_led_init(debug_led_mode_provider_t provider)
{
    if (!provider || __atomic_load_n(&s_gpio_ready, __ATOMIC_ACQUIRE)) {
        return ESP_ERR_INVALID_ARG;
    }
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << DEBUG_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_set_level(DEBUG_LED_GPIO, !DEBUG_LED_ACTIVE_LEVEL);
    if (err != ESP_OK) {
        return err;
    }
    err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    s_provider = provider;
    __atomic_store_n(&s_gpio_ready, 1, __ATOMIC_RELEASE);
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
        reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
        debug_led_fault(); /* Keep evidence visible after automatic panic reset. */
    }
    const esp_timer_create_args_t timer_args = {
        .callback = update_led,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "debug_led",
        .skip_unhandled_events = true,
    };
    err = esp_timer_create(&timer_args, &s_timer);
    if (err == ESP_OK) {
        update_led(NULL);
        err = esp_timer_start_periodic(s_timer, 20000);
    }
    if (err != ESP_OK) {
        debug_led_fault();
        if (s_timer) {
            (void)esp_timer_delete(s_timer);
            s_timer = NULL;
        }
        return err;
    }
    ESP_LOGI("DEBUG_LED", "GPIO%d active=%d; toggle 500/1000 ms; reset_reason=%d latched=%lu",
             DEBUG_LED_GPIO, DEBUG_LED_ACTIVE_LEVEL, reason,
             (unsigned long)__atomic_load_n(&s_latched_fault, __ATOMIC_ACQUIRE));
    return ESP_OK;
}
