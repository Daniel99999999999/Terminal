#include "terminal_view.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "debug_led.h"

#define VIEW_CHUNK 128
#define VIEW_QUEUE 32

typedef struct {
    uint32_t serial;
    uint16_t length;
    uint8_t data[VIEW_CHUNK];
} view_packet_t;

static terminal_t s_terminal;
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_mutex;
static uint32_t s_serial; /* Sole UART RX producer, including gap markers. */
static uint32_t s_dropped, s_gaps;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static void count(uint32_t *counter, uint32_t n)
{
    portENTER_CRITICAL(&s_stats_lock);
    *counter += n;
    portEXIT_CRITICAL(&s_stats_lock);
}

void terminal_view_stream_gap(void)
{
    ++s_serial;
    count(&s_gaps, 1);
}

void terminal_view_feed(const uint8_t *data, size_t length)
{
    if (!s_queue || (!data && length)) { return; }
    view_packet_t packet;
    while (length) {
        size_t n = length > VIEW_CHUNK ? VIEW_CHUNK : length;
        packet.serial = ++s_serial;
        packet.length = (uint16_t)n;
        memcpy(packet.data, data, n);
        if (xQueueSend(s_queue, &packet, 0) != pdTRUE) {
            count(&s_dropped, (uint32_t)length);
            count(&s_gaps, 1);
            return;
        }
        data += n; length -= n;
    }
}

static void view_task(void *arg)
{
    (void)arg;
    view_packet_t packet;
    uint32_t previous = 0;
    for (;;) {
        debug_led_heartbeat(DEBUG_LED_TERMINAL);
        if (xQueueReceive(s_queue, &packet, pdMS_TO_TICKS(250)) != pdTRUE) { continue; }
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
            count(&s_dropped, packet.length);
            count(&s_gaps, 1);
            continue; /* Serial discontinuity is handled on the next packet. */
        }
        if (packet.serial != previous + 1) { terminal_stream_gap(&s_terminal); }
        terminal_feed(&s_terminal, packet.data, packet.length);
        previous = packet.serial;
        xSemaphoreGive(s_mutex);
    }
}

esp_err_t terminal_view_init(uint16_t cols, uint16_t rows)
{
    if (s_queue) { return ESP_ERR_INVALID_STATE; }
    if (!cols || !rows || cols > TERMINAL_MAX_COLS || rows > TERMINAL_MAX_ROWS) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t cells = (size_t)cols * rows;
    terminal_cell_t *a = heap_caps_calloc(cells, sizeof(*a), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    terminal_cell_t *b = heap_caps_calloc(cells, sizeof(*b), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_mutex = xSemaphoreCreateMutex();
    s_queue = xQueueCreate(VIEW_QUEUE, sizeof(view_packet_t));
    bool ready = a && b && s_mutex && s_queue && terminal_init(&s_terminal, cols, rows, a, b);
    if (ready && xTaskCreate(view_task, "terminal", 4096, NULL, 6, NULL) == pdPASS) {
        ESP_LOGI("TERMINAL", "ANSI/VT100 display model %ux%u, white/black; PSRAM %u B; LCD not initialized",
                 (unsigned)cols, (unsigned)rows, (unsigned)(2 * cells * sizeof(*a)));
        return ESP_OK;
    }
    if (s_queue) { vQueueDelete(s_queue); s_queue = NULL; }
    if (s_mutex) { vSemaphoreDelete(s_mutex); s_mutex = NULL; }
    heap_caps_free(a); heap_caps_free(b);
    return ESP_ERR_NO_MEM;
}

bool terminal_view_copy_row(uint16_t row, terminal_cell_t *dest, size_t capacity,
                            terminal_view_info_t *info)
{
    if (!s_mutex || !dest || !info || row >= s_terminal.rows ||
        capacity < s_terminal.cols || xSemaphoreTake(s_mutex, 0) != pdTRUE) { return false; }
    memcpy(dest, terminal_cell(&s_terminal, 0, row), s_terminal.cols * sizeof(*dest));
    const terminal_cursor_t *c = &s_terminal.page[s_terminal.active].cursor;
    *info = (terminal_view_info_t){ .cols = s_terminal.cols, .rows = s_terminal.rows,
        .cursor_x = c->x, .cursor_y = c->y, .cursor_visible = s_terminal.cursor_visible,
        .alternate = s_terminal.active != 0, .revision = s_terminal.revision,
        .control_sequences = s_terminal.control_sequences };
    xSemaphoreGive(s_mutex);
    portENTER_CRITICAL(&s_stats_lock);
    info->dropped_bytes = s_dropped; info->stream_gaps = s_gaps;
    portEXIT_CRITICAL(&s_stats_lock);
    return true;
}

void terminal_view_log_stats(void)
{
    uint32_t dropped, gaps;
    portENTER_CRITICAL(&s_stats_lock);
    dropped = s_dropped; gaps = s_gaps;
    portEXIT_CRITICAL(&s_stats_lock);
    if (dropped || gaps) {
        ESP_LOGW("TERMINAL", "display stream dropped=%lu B, gaps=%lu; screen may need redraw by sender",
                 (unsigned long)dropped, (unsigned long)gaps);
    }
}
