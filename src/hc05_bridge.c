#include "hc05_bridge.h"
#include <string.h>
#include "sdkconfig.h"
#include "board_pins.h"
#include "debug_led.h"
#include "terminal_view.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#ifdef CONFIG_RETRO_HC05_STATE_ENABLE
#define HAS_STATE true
#else
#define HAS_STATE false
#endif
#ifdef CONFIG_RETRO_HC05_STATE_ACTIVE_HIGH
#define STATE_LEVEL 1
#else
#define STATE_LEVEL 0
#endif
#define RX_BYTES 4096
#define PACKETS 32

typedef struct {
    uint32_t generation;
    uint16_t length;
    bool at;
    uint8_t data[HC05_PACKET_BYTES];
} packet_t;

typedef struct {
    uart_port_t source, dest;
    QueueHandle_t events, output;
    debug_led_channel_t rx_beat, tx_beat;
    uint32_t received, sent, dropped, boundary, errors;
} direction_t;

static direction_t s_dir[2] = {
    { .source = UART_NUM_2, .dest = UART_NUM_1,
      .rx_beat = DEBUG_LED_UART_RX, .tx_beat = DEBUG_LED_HC_TX },
    { .source = UART_NUM_1, .dest = UART_NUM_2,
      .rx_beat = DEBUG_LED_HC_RX, .tx_beat = DEBUG_LED_UART_TX },
};
static hc05_link_t s_link;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_io_mutex;
static QueueHandle_t s_controls, s_at_rx;
static bool s_at_ready, s_ready;

static void count(uint32_t *value, uint32_t n)
{
    portENTER_CRITICAL(&s_lock); *value += n; portEXIT_CRITICAL(&s_lock);
}

static hc05_link_t snapshot(void)
{
    portENTER_CRITICAL(&s_lock);
    hc05_link_t link = s_link;
    portEXIT_CRITICAL(&s_lock);
    return link;
}

hc05_status_t hc05_bridge_status(void)
{
    hc05_link_t link = snapshot();
    return hc05_link_status(&link);
}

debug_led_mode_t hc05_bridge_led_mode(void)
{
    return hc05_bridge_status() == HC05_CONNECTED ?
           DEBUG_LED_CONNECTED : DEBUG_LED_DISCONNECTED;
}

static bool current(const packet_t *packet)
{
    portENTER_CRITICAL(&s_lock);
    bool valid = hc05_link_current(&s_link, packet->generation, packet->at) &&
                 (!packet->at || s_at_ready);
    portEXIT_CRITICAL(&s_lock);
    return valid;
}

static void rx_task(void *arg)
{
    direction_t *d = arg;
    packet_t packet = {0};
    uint32_t generation = snapshot().generation;
    size_t boundary = 0;
    for (;;) {
        debug_led_heartbeat(d->rx_beat);
        uart_event_t event;
        if (xQueueReceive(d->events, &event, pdMS_TO_TICKS(100)) == pdTRUE &&
            (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
             event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR)) {
            count(&d->errors, 1);
            debug_led_fault();
            if (d->source == UART_NUM_2) { terminal_view_stream_gap(); }
            /* This reader owns RX. AT transitions also use the same mutex. */
            if (xSemaphoreTake(s_io_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                (void)uart_flush_input(d->source);
                xSemaphoreGive(s_io_mutex);
            }
        }
        for (unsigned chunk = 0; chunk < RX_BYTES / HC05_PACKET_BYTES; ++chunk) {
            if (xSemaphoreTake(s_io_mutex, pdMS_TO_TICKS(20)) != pdTRUE) { break; }
            hc05_link_t link = snapshot();
            if (generation != link.generation) {
                /* Drain pre-boundary bytes, still feeding the local display.
                 * Never deliver buffered bytes to a newly connected peer. */
                if (link.at && d->source == UART_NUM_1) {
                    /* The AT control transition already flushed UART1 RX.
                     * Never discard the first fresh OK response as old data. */
                    boundary = 0;
                } else if (uart_get_buffered_data_len(d->source, &boundary) != ESP_OK) {
                    boundary = RX_BYTES; debug_led_fault();
                }
                generation = link.generation;
            }
            int n = uart_read_bytes(d->source, packet.data, sizeof(packet.data), 0);
            xSemaphoreGive(s_io_mutex);
            if (n <= 0) {
                if (n < 0) {
                    count(&d->errors, 1); debug_led_fault();
                    if (d->source == UART_NUM_2) { terminal_view_stream_gap(); }
                }
                break;
            }
            count(&d->received, (uint32_t)n);
            if (d->source == UART_NUM_2) { terminal_view_feed(packet.data, (size_t)n); }
            size_t discard = boundary < (size_t)n ? boundary : (size_t)n;
            boundary -= discard;
            count(&d->boundary, (uint32_t)discard);
            if (discard == (size_t)n) { continue; }
            packet.generation = generation;
            packet.at = link.at;
            packet.length = (uint16_t)(n - discard);
            if (discard) { memmove(packet.data, packet.data + discard, packet.length); }
            /* In AT mode, only HC-05 replies reach the dedicated response queue. */
            QueueHandle_t queue = link.at ? s_at_rx : d->output;
            if ((link.at && d->source != UART_NUM_1) || !current(&packet) ||
                xQueueSend(queue, &packet, 0) != pdTRUE) {
                count(&d->dropped, packet.length);
            }
        }
    }
}

static void tx_task(void *arg)
{
    direction_t *d = arg;
    packet_t packet;
    for (;;) {
        debug_led_heartbeat(d->tx_beat);
        if (xQueueReceive(d->output, &packet, pdMS_TO_TICKS(250)) != pdTRUE) { continue; }
        size_t offset = 0;
        uint32_t baud = d->dest == UART_NUM_2 ? 115200 : CONFIG_RETRO_HC05_BAUD;
#ifdef CONFIG_RETRO_HC05_KEY_ENABLE
        if (packet.at) { baud = CONFIG_RETRO_HC05_AT_BAUD; }
#endif
        int64_t deadline = esp_timer_get_time() +
            (int64_t)packet.length * 10000000 / baud + 100000;
        while (offset < packet.length && current(&packet)) {
            debug_led_heartbeat(d->tx_beat);
            if (xSemaphoreTake(s_io_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                /* No TX ring buffer: uart_tx_chars is a zero-wait FIFO write. */
                int n = current(&packet) ? uart_tx_chars(d->dest,
                    (const char *)packet.data + offset, packet.length - offset) : 0;
                xSemaphoreGive(s_io_mutex);
                if (n < 0) { count(&d->errors, 1); debug_led_fault(); break; }
                offset += (size_t)n;
            }
            if (offset == packet.length) { break; }
            if (esp_timer_get_time() >= deadline) {
                count(&d->errors, 1); debug_led_fault(); break;
            }
            /* Interrupt-backed bounded wait; no spin or delay-based TX polling. */
            esp_err_t err = uart_wait_tx_done(d->dest, pdMS_TO_TICKS(20));
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                count(&d->errors, 1); debug_led_fault(); break;
            }
        }
        count(&d->sent, (uint32_t)offset);
        count(&d->dropped, packet.length - (uint32_t)offset);
    }
}

#ifdef CONFIG_RETRO_HC05_KEY_ENABLE
static void change_at_mode(bool enable)
{
    portENTER_CRITICAL(&s_lock);
    if (s_link.at == enable && (enable ? s_at_ready : true)) {
        portEXIT_CRITICAL(&s_lock); return;
    }
    s_at_ready = false;
    hc05_link_at(&s_link, true); /* Quiesce both bridge directions first. */
    portEXIT_CRITICAL(&s_lock);
    esp_err_t err = ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(s_io_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        err = uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(200));
        if (err == ESP_OK) { err = gpio_set_level(HC05_KEY_GPIO, enable); }
        if (err == ESP_OK) { err = uart_set_baudrate(UART_NUM_1,
            enable ? CONFIG_RETRO_HC05_AT_BAUD : CONFIG_RETRO_HC05_BAUD); }
        if (err == ESP_OK) { err = uart_flush_input(UART_NUM_1); }
        packet_t stale;
        while (xQueueReceive(s_at_rx, &stale, 0) == pdTRUE) {}
        if (err == ESP_OK) {
            portENTER_CRITICAL(&s_lock);
            hc05_link_at(&s_link, enable);
            s_at_ready = enable;
            portEXIT_CRITICAL(&s_lock);
        }
        xSemaphoreGive(s_io_mutex);
    }
    if (err != ESP_OK) { debug_led_fault(); }
    ESP_LOGI("HC05", "AT request=%d result=%s; bridge %s; module response not verified",
             enable, esp_err_to_name(err), enable || err != ESP_OK ? "paused" : "active");
}
#endif

static void control_task(void *arg)
{
    (void)arg;
    TickType_t last_report = xTaskGetTickCount();
    hc05_status_t previous = HC05_UNKNOWN;
    for (;;) {
        debug_led_heartbeat(DEBUG_LED_HC_CONTROL);
        bool at;
        if (xQueueReceive(s_controls, &at, pdMS_TO_TICKS(50)) == pdTRUE) {
#ifdef CONFIG_RETRO_HC05_KEY_ENABLE
            change_at_mode(at);
#else
            (void)at;
#endif
        }
        if (HAS_STATE) {
            bool active = gpio_get_level(HC05_STATE_GPIO) == STATE_LEVEL;
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            portENTER_CRITICAL(&s_lock);
            hc05_link_sample(&s_link, active, now);
            portEXIT_CRITICAL(&s_lock);
        }
        hc05_status_t status = hc05_bridge_status();
        if (status != previous) {
            ESP_LOGI("HC05", "STATE status=%d (0 unknown / 1 disconnected / 2 connected / 3 AT)", status);
            previous = status;
        }
        if ((TickType_t)(xTaskGetTickCount() - last_report) >= pdMS_TO_TICKS(5000)) {
            for (unsigned i = 0; i < 2; ++i) {
                portENTER_CRITICAL(&s_lock);
                direction_t d = s_dir[i];
                portEXIT_CRITICAL(&s_lock);
                ESP_LOGI("BRIDGE", "UART%d->UART%d RX=%lu TX_fifo=%lu drop=%lu boundary=%lu errors=%lu",
                         d.source, d.dest, (unsigned long)d.received, (unsigned long)d.sent,
                         (unsigned long)d.dropped, (unsigned long)d.boundary, (unsigned long)d.errors);
            }
            terminal_view_log_stats();
            last_report = xTaskGetTickCount();
        }
    }
}

esp_err_t hc05_bridge_request_at(bool enable)
{
#ifndef CONFIG_RETRO_HC05_KEY_ENABLE
    (void)enable; return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_ready) { return ESP_ERR_INVALID_STATE; }
    return xQueueSend(s_controls, &enable, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
#endif
}

esp_err_t hc05_bridge_at_send(const uint8_t *data, size_t length)
{
    if (!data || !length || length > HC05_PACKET_BYTES) { return ESP_ERR_INVALID_ARG; }
    packet_t p = { .at = true, .generation = snapshot().generation, .length = (uint16_t)length };
    if (!s_ready || !current(&p)) { return ESP_ERR_INVALID_STATE; }
    memcpy(p.data, data, length);
    return xQueueSend(s_dir[0].output, &p, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t hc05_bridge_at_receive(uint8_t *data, size_t capacity, size_t *length)
{
    if (!data || capacity < HC05_PACKET_BYTES || !length) { return ESP_ERR_INVALID_ARG; }
    *length = 0;
    if (!s_ready) { return ESP_ERR_INVALID_STATE; }
    packet_t p;
    if (xQueueReceive(s_at_rx, &p, 0) != pdTRUE) { return ESP_ERR_NOT_FOUND; }
    if (!current(&p)) { return ESP_ERR_INVALID_STATE; }
    memcpy(data, p.data, p.length); *length = p.length;
    return ESP_OK;
}

esp_err_t hc05_bridge_init(void)
{
    if (s_ready) { return ESP_ERR_INVALID_STATE; }
    portENTER_CRITICAL(&s_lock);
    hc05_link_init(&s_link, HAS_STATE);
    portEXIT_CRITICAL(&s_lock);
    s_io_mutex = xSemaphoreCreateMutex();
    s_controls = xQueueCreate(4, sizeof(bool));
    s_at_rx = xQueueCreate(8, sizeof(packet_t));
    if (!s_io_mutex || !s_controls || !s_at_rx) { return ESP_ERR_NO_MEM; }
    for (unsigned i = 0; i < 2; ++i) {
        direction_t *d = &s_dir[i];
        d->output = xQueueCreate(PACKETS, sizeof(packet_t));
        if (!d->output) { return ESP_ERR_NO_MEM; }
        uart_config_t config = { .baud_rate = i == 0 ? 115200 : CONFIG_RETRO_HC05_BAUD,
            .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT };
        esp_err_t err = uart_driver_install(d->source, RX_BYTES, 0, 32, &d->events, 0);
        if (err != ESP_OK) { return err; }
        err = uart_param_config(d->source, &config);
        if (err != ESP_OK) { return err; }
        err = uart_set_pin(d->source, i == 0 ? TARGET_UART_TX_GPIO : HC05_UART_TX_GPIO,
            i == 0 ? TARGET_UART_RX_GPIO : HC05_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (err != ESP_OK) { return err; }
    }
    if (HAS_STATE) {
        const gpio_config_t config = { .pin_bit_mask = 1ULL << HC05_STATE_GPIO,
            .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_DISABLE };
        esp_err_t err = gpio_config(&config);
        if (err != ESP_OK) { return err; }
    }
#ifdef CONFIG_RETRO_HC05_KEY_ENABLE
    /* Explicit opt-in only: default firmware never touches strap GPIO12. */
    esp_err_t err = gpio_set_level(HC05_KEY_GPIO, 0);
    if (err != ESP_OK) { return err; }
    err = gpio_set_direction(HC05_KEY_GPIO, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) { return err; }
#endif
    s_ready = true;
    for (unsigned i = 0; i < 2; ++i) {
        if (xTaskCreate(rx_task, i == 0 ? "target_rx" : "hc05_rx", 4096,
                        &s_dir[i], 8, NULL) != pdPASS ||
            xTaskCreate(tx_task, i == 0 ? "hc05_tx" : "target_tx", 3072,
                        &s_dir[i], 8, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    }
    if (xTaskCreate(control_task, "hc05_control", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI("HC05", "UART1 TX32/RX34 %d 8N1 <-> UART2 TX25/RX26 115200; STATE=%s; internal BT not started",
             CONFIG_RETRO_HC05_BAUD, HAS_STATE ? "enabled" : "unwired/unknown");
    return ESP_OK;
}
