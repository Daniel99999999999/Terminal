#include "uart_input.h"
#include "sdkconfig.h"
#include "board_pins.h"
#include "app_health.h"
#include "screen.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static QueueHandle_t s_events;
static uint32_t s_received, s_errors, s_crc;

static void receive(void *arg)
{
    (void)arg;
    uint8_t data[256];
    for (;;) {
        app_health_beat(HEALTH_UART1);
        uart_event_t event;
        if (xQueueReceive(s_events, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR) {
                __atomic_fetch_add(&s_errors, 1, __ATOMIC_RELAXED);
                screen_stream_gap();
                app_health_fault();
                if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                    (void)uart_flush_input(UART_NUM_1);
                    xQueueReset(s_events);
                }
            }
        }
        /* Read a bounded batch even if an event was lost from the queue. */
        for (unsigned i = 0; i < 16; ++i) {
            int n = uart_read_bytes(UART_NUM_1, data, sizeof(data), 0);
            if (n < 0) {
                __atomic_fetch_add(&s_errors, 1, __ATOMIC_RELAXED);
                screen_stream_gap(); app_health_fault(); break;
            }
            if (!n) { break; }
            __atomic_fetch_add(&s_received, (uint32_t)n, __ATOMIC_RELAXED);
            uint32_t crc = __atomic_load_n(&s_crc, __ATOMIC_RELAXED);
            __atomic_store_n(&s_crc, esp_rom_crc32_le(crc, data, (uint32_t)n), __ATOMIC_RELAXED);
            screen_input(data, (size_t)n);
        }
        /* Explicit yield also under a continuous stream. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void uart_input_log(void)
{
    ESP_LOGI("HC05", "RX=%lu errors=%lu CRC32=%08lx; UART2 inactive",
        (unsigned long)__atomic_load_n(&s_received, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_errors, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_crc, __ATOMIC_RELAXED));
    screen_log();
}

void uart_input_poll(void)
{
#ifdef CONFIG_RETRO_HC05_STATE_ENABLE
    static bool rising;
    static uint32_t since;
    bool high = gpio_get_level(HC05_STATE_GPIO) ==
#ifdef CONFIG_RETRO_HC05_STATE_ACTIVE_HIGH
        1;
#else
        0;
#endif
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (high && !rising) { since = now; }
    rising = high;
    app_health_connected(high && now - since >= 50);
#endif
}

esp_err_t uart_input_start(void)
{
#ifdef CONFIG_RETRO_HC05_STATE_ENABLE
    const gpio_config_t state = { .pin_bit_mask = 1ULL << HC05_STATE_GPIO, .mode = GPIO_MODE_INPUT };
    esp_err_t state_err = gpio_config(&state);
    if (state_err != ESP_OK) { return state_err; }
#endif
    const uart_config_t cfg = { .baud_rate = CONFIG_RETRO_HC05_BAUD,
        .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT };
    esp_err_t err = uart_param_config(UART_NUM_1, &cfg);
    if (err == ESP_OK) {
        err = uart_set_pin(UART_NUM_1, HC05_UART_TX_GPIO, HC05_UART_RX_GPIO,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK) { err = uart_driver_install(UART_NUM_1, 4096, 0, 32, &s_events, 0); }
    if (err != ESP_OK) { return err; }
    app_health_beat(HEALTH_UART1);
    if (xTaskCreate(receive, "hc05", 4096, NULL, 6, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    ESP_LOGI("HC05", "TX32/RX34 %d 8N1; HC-05 input -> TFT; UART2 untouched", CONFIG_RETRO_HC05_BAUD);
    return ESP_OK;
}
