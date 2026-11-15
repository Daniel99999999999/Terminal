/* RetroTerminal 0.2: standard ESP-IDF startup, no memory-controller overrides. */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) || defined(CONFIG_ESP_WIFI_ENABLED)
#error "Native radio components must remain disabled"
#endif
#include <stdint.h>
#include "app_health.h"
#include "screen.h"
#include "uart_input.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifndef RETRO_STARTUP_STAGE
#define RETRO_STARTUP_STAGE 2
#endif

static bool checked(esp_err_t error, const char *step)
{
    if (error == ESP_OK) { ESP_LOGI("START", "%s OK", step); return true; }
    app_health_fault();
    ESP_LOGE("START", "%s failed: %s; LED latched ON", step, esp_err_to_name(error));
    return false;
}

static esp_err_t memory_check(void)
{
    if (!esp_psram_is_initialized()) { return ESP_ERR_INVALID_STATE; }
    volatile uint32_t *test = heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!test) { return ESP_ERR_NO_MEM; }
    esp_err_t result = ESP_OK;
    for (uint32_t pattern = 0; pattern < 3; ++pattern) {
        for (unsigned i = 0; i < 4096; ++i) { test[i] = (i * 2654435761u) ^ (pattern * 0x55555555u); }
        for (unsigned i = 0; i < 4096; ++i) {
            if (test[i] != ((i * 2654435761u) ^ (pattern * 0x55555555u))) { result = ESP_FAIL; break; }
        }
    }
    heap_caps_free((void *)test);
    ESP_LOGI("START", "PSRAM initialized=%d chip=%u bytes heap=%u bytes; 16 KiB check=%s",
             esp_psram_is_initialized(), (unsigned)esp_psram_get_size(),
             (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM), esp_err_to_name(result));
    return result;
}

void app_main(void)
{
    if (!checked(app_health_init(), "LED")) { return; }
    app_health_beat(HEALTH_MAIN);
    ESP_LOGI("START", "RetroTerminal v%s stage=%d; native BT/Wi-Fi OFF; PSRAM pins16/17 reserved",
             esp_app_get_description()->version, RETRO_STARTUP_STAGE);
    if (!checked(memory_check(), "PSRAM")) { return; }
#if RETRO_STARTUP_STAGE >= 1
    if (!checked(screen_start(), "screen task")) { return; }
#endif
#if RETRO_STARTUP_STAGE >= 2
    if (!checked(uart_input_start(), "HC-05 UART1")) { return; }
#endif
    ESP_LOGI("START", "READY stage=%d", RETRO_STARTUP_STAGE);
    unsigned ticks = 0;
    for (;;) {
        app_health_beat(HEALTH_MAIN);
#if RETRO_STARTUP_STAGE >= 2
        uart_input_poll();
#endif
        if (++ticks == 50) {
            ticks = 0;
            ESP_LOGI("HEALTH", "alive stage=%d internal_free=%u PSRAM_free=%u", RETRO_STARTUP_STAGE,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#if RETRO_STARTUP_STAGE >= 2
            uart_input_log();
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
