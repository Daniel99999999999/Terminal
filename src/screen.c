#include "screen.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "app_health.h"
#include "app_memory.h"
#include "lcd_panel.h"
#include "terminal.h"
#include "terminal_history.h"
#include "terminal_raster.h"
#include "xpt2046.h"
#include "touch_ui.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define HAS_LCD (RETRO_TFT_PROFILE != RETRO_TFT_NONE)
#define COLS (HAS_LCD ? LCD_WIDTH / 8 : 80)
#define ROWS (HAS_LCD ? LCD_HEIGHT / 8 - 1 : 24)
#ifdef CONFIG_RETRO_DISPLAY_CR_AS_NEWLINE
#define DEFAULT_CR_NEWLINE true
#else
#define DEFAULT_CR_NEWLINE false
#endif
typedef struct { uint32_t sequence; uint16_t length; uint8_t data[128]; } input_t;
static QueueHandle_t s_input;
static uint32_t s_sequence, s_drops;
static uint32_t s_parsed, s_crc, s_controls;
static uint32_t s_cursor, s_view;
static terminal_t s_terminal;
static terminal_cell_t *s_previous;
#if RETRO_TOUCH_ENABLE
static terminal_history_t s_history;
#endif
static uint32_t s_history_offset, s_history_lines;

void screen_stream_gap(void) { ++s_sequence; }

void screen_input(const uint8_t *data, size_t length)
{
    if (!s_input) { return; }
    while (length) {
        input_t input = { .sequence = ++s_sequence };
        size_t n = length < sizeof(input.data) ? length : sizeof(input.data);
        input.length = (uint16_t)n;
        memcpy(input.data, data, n);
        if (xQueueSend(s_input, &input, 0) != pdTRUE) {
            __atomic_fetch_add(&s_drops, (uint32_t)length, __ATOMIC_RELAXED);
            return;
        }
        length -= n; data += n;
    }
}

static void parse_input(uint32_t *previous)
{
    input_t input;
    for (unsigned i = 0; i < 32 && xQueueReceive(s_input, &input, 0) == pdTRUE; ++i) {
        if (input.sequence != *previous + 1) { terminal_stream_gap(&s_terminal); }
        terminal_feed(&s_terminal, input.data, input.length);
        __atomic_fetch_add(&s_parsed, input.length, __ATOMIC_RELAXED);
        uint32_t crc = __atomic_load_n(&s_crc, __ATOMIC_RELAXED);
        __atomic_store_n(&s_crc, esp_rom_crc32_le(crc, input.data, input.length), __ATOMIC_RELAXED);
        __atomic_store_n(&s_controls, s_terminal.control_sequences, __ATOMIC_RELAXED);
        *previous = input.sequence;
    }
    const terminal_cursor_t *cursor = &s_terminal.page[s_terminal.active].cursor;
    __atomic_store_n(&s_cursor, ((uint32_t)cursor->y << 16) | cursor->x, __ATOMIC_RELAXED);
}

void screen_log(void)
{
    uint32_t cursor = __atomic_load_n(&s_cursor, __ATOMIC_RELAXED);
    uint32_t view = __atomic_load_n(&s_view, __ATOMIC_RELAXED);
    ESP_LOGI("SCREEN", "parsed=%lu drop=%lu controls=%lu CRC32=%08lx cursor=%u,%u view=%s CR=%s history=%lu/%lu",
        (unsigned long)__atomic_load_n(&s_parsed, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_drops, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_controls, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_crc, __ATOMIC_RELAXED),
        (unsigned)(cursor & 0xffff), (unsigned)(cursor >> 16),
        view & 1 ? "PAUSE" : "LIVE", view & 2 ? "NL" : "CR",
        (unsigned long)__atomic_load_n(&s_history_offset, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_history_lines, __ATOMIC_RELAXED));
}

#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
static esp_err_t draw(unsigned row, const terminal_cell_t *cells,
                      unsigned cursor_x, bool cursor, bool phase)
{
    if (!terminal_raster_row(cells, COLS, cursor_x, cursor, phase,
                             lcd_panel_pixels(), LCD_STRIP_PIXELS)) { return ESP_ERR_INVALID_SIZE; }
    return lcd_panel_write_row(row * 8);
}

static esp_err_t welcome(uint32_t *sequence)
{
    char version[48];
    snprintf(version, sizeof(version), "Firmware v%.31s", esp_app_get_description()->version);
    const char *text[] = {"ESP32 RetroTerminal", version, "HC-05 UART1 115200", "ANSI / VT100"};
    for (unsigned row = 0; row < LCD_HEIGHT / 8; ++row) {
        app_health_beat(HEALTH_SCREEN);
        parse_input(sequence);
        terminal_cell_t cells[COLS];
        for (unsigned x = 0; x < COLS; ++x) {
            cells[x] = (terminal_cell_t){ .codepoint = ' ', .fg = 0xffffff };
        }
        for (unsigned line = 0; line < 4; ++line) {
            if (row != LCD_HEIGHT / 16 - 4 + line * 2) { continue; }
            size_t n = strlen(text[line]);
            if (n > COLS) { n = COLS; }
            for (size_t x = 0; x < n; ++x) { cells[(COLS - n) / 2 + x].codepoint = (uint8_t)text[line][x]; }
        }
        esp_err_t err = draw(row, cells, 0, false, true);
        if (err != ESP_OK) { return err; }
    }
    return ESP_OK;
}

static esp_err_t status_line(bool paused, bool cr_newline)
{
    char text[COLS + 1];
#if RETRO_TOUCH_ENABLE
    char cal[15];
    char pause_text[24];
    unsigned offset = terminal_history_offset(&s_history);
    if (paused && offset) { snprintf(pause_text, sizeof(pause_text), "[WZNOW %u]", offset); }
    else { snprintf(pause_text, sizeof(pause_text), "%s", paused ? "[WZNOW]" : "[PAUZA]"); }
    unsigned progress = touch_ui_cal_progress();
    if (progress > 100) { progress = 100; }
    if (progress) { snprintf(cal, sizeof(cal), "[CAL %u%%]", progress); }
    else { snprintf(cal, sizeof(cal), "[CAL:2s]"); }
    snprintf(text, sizeof(text), "%-13.13s%-13.13s%-14.14s",
             pause_text, cr_newline ? "[CR=NL]" : "[CR=CR]", cal);
#else
    (void)paused;
    snprintf(text, sizeof(text), "HC05 115200 8N1 | CR=%s", cr_newline ? "NL" : "CR");
#endif
    size_t length = strlen(text);
    terminal_cell_t cells[COLS];
    for (unsigned x = 0; x < COLS; ++x) {
        cells[x] = (terminal_cell_t){ .codepoint = x < length ? (uint8_t)text[x] : ' ',
            .fg = 0xaaaaaa, .bg = 0x101010 };
    }
    return draw(ROWS, cells, 0, false, true);
}
#endif

static void screen_task(void *arg)
{
    (void)arg;
    uint32_t sequence = 0, old_drops = 0;
    bool cr_newline = DEFAULT_CR_NEWLINE;
    bool paused = false;
    __atomic_store_n(&s_view, cr_newline ? 2u : 0u, __ATOMIC_RELAXED);
    esp_err_t lcd_error = lcd_panel_open();
    int64_t welcome_until = 0;
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    if (lcd_error == ESP_OK) { lcd_error = welcome(&sequence); }
    if (lcd_error == ESP_OK) { lcd_error = lcd_panel_show(); }
    if (lcd_error == ESP_OK) {
        welcome_until = esp_timer_get_time() + 3000000;
        ESP_LOGI("TFT", "Welcome visible; hold 3000 ms");
    }
    bool old_cursor[ROWS] = {0}, old_phase[ROWS] = {0};
    unsigned old_x[ROWS] = {0}, next_row = 0;
    bool status_drawn = false;
#endif
#if RETRO_TOUCH_ENABLE
    bool touch = xpt2046_init() == ESP_OK;
    if (!touch) { app_health_fault(); ESP_LOGE("TOUCH", "Initialization failed"); }
    else { touch_ui_init(cr_newline); }
    uint32_t ui_rows[LCD_HEIGHT / 8] = {0};
    unsigned ui_next = 0;
    bool overlay_was_visible = false;
    unsigned cal_progress = 0;
    int64_t touch_log_at = 0;
#endif
    if (lcd_error != ESP_OK) {
        app_health_fault(); ESP_LOGE("TFT", "Initialization stopped: %s", esp_err_to_name(lcd_error));
    }
    for (;;) {
        app_health_beat(HEALTH_SCREEN);
        parse_input(&sequence);
        uint32_t drops = __atomic_load_n(&s_drops, __ATOMIC_RELAXED);
        if (drops != old_drops) {
            ESP_LOGW("SCREEN", "Input dropped=%lu bytes", (unsigned long)drops); old_drops = drops;
        }
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
        int64_t now = esp_timer_get_time();
        if (welcome_until && now >= welcome_until) {
            ESP_LOGI("TFT", "Welcome complete; opening terminal"); welcome_until = 0;
        }
#if RETRO_TOUCH_ENABLE
        if (touch && !welcome_until) {
            xpt2046_sample_t sample;
            esp_err_t error = xpt2046_read(&sample);
            if (error != ESP_OK) {
                touch = false; app_health_fault();
                ESP_LOGE("TOUCH", "Read failed: %s; terminal continues", esp_err_to_name(error));
            } else { touch_ui_update(&sample, (uint32_t)(now / 1000)); }
        }
        unsigned next_cal_progress = touch_ui_cal_progress();
        if (next_cal_progress != cal_progress) { cal_progress = next_cal_progress; status_drawn = false; }
        if (touch && now - touch_log_at >= 2000000) {
            touch_log_at = now;
            xpt2046_stats_t stats; xpt2046_get_stats(&stats);
            ESP_LOGI("TOUCH_RAW", "mode=IRQ samples=%lu irq_low=%lu valid=%lu weak=%lu bounds=%lu end_high=%lu io=%lu raw=%u,%u z=%u z1=%u z2=%u irq=%u edges=%lu",
                (unsigned long)stats.polls, (unsigned long)stats.irq_low, (unsigned long)stats.valid,
                (unsigned long)stats.weak, (unsigned long)stats.bounds, (unsigned long)stats.end_high,
                (unsigned long)stats.io_errors, stats.last_x, stats.last_y, stats.last_pressure,
                stats.last_z1, stats.last_z2, stats.last_irq_high ? 1u : 0u, (unsigned long)stats.irq_edges);
        }
        bool next_paused = touch && touch_ui_paused();
        if (next_paused != paused) {
            paused = next_paused; status_drawn = false;
            memset(s_previous, 0, COLS * ROWS * sizeof(*s_previous));
        }
        terminal_history_pause(&s_history, &s_terminal, paused);
        int scroll_rows = touch_ui_take_scroll();
        if (paused && scroll_rows) { terminal_history_move(&s_history, &s_terminal, scroll_rows); }
        unsigned offset = terminal_history_offset(&s_history);
        if (offset != __atomic_load_n(&s_history_offset, __ATOMIC_RELAXED)) { status_drawn = false; }
        __atomic_store_n(&s_history_offset, offset, __ATOMIC_RELAXED);
        __atomic_store_n(&s_history_lines, (uint32_t)(s_history.total < s_history.capacity ? s_history.total : s_history.capacity), __ATOMIC_RELAXED);
        bool next_cr = touch ? touch_ui_cr_newline() : cr_newline;
        if (next_cr != cr_newline) {
            cr_newline = next_cr; terminal_set_cr_newline(&s_terminal, cr_newline); status_drawn = false;
        }
        __atomic_store_n(&s_view, (paused ? 1u : 0u) | (cr_newline ? 2u : 0u), __ATOMIC_RELAXED);
        bool overlay = touch && touch_ui_visible();
        if (overlay_was_visible && !overlay) {
            memset(s_previous, 0, COLS * ROWS * sizeof(*s_previous));
            status_drawn = false;
        }
        overlay_was_visible = overlay;
        if (lcd_error == ESP_OK && !welcome_until && overlay) {
            uint32_t revision = touch_ui_revision();
            for (unsigned budget = 0; budget < 4; ++budget) {
                unsigned row = ui_next++ % (LCD_HEIGHT / 8);
                if (ui_rows[row] == revision) { continue; }
                if (!touch_ui_render_row(row, lcd_panel_pixels(), LCD_STRIP_PIXELS)) { lcd_error = ESP_FAIL; break; }
                lcd_error = lcd_panel_write_row(row * 8);
                if (lcd_error != ESP_OK) { app_health_fault(); ESP_LOGE("TFT", "Touch UI render stopped"); break; }
                ui_rows[row] = revision;
            }
        }
#else
        bool overlay = false;
#endif
        if (lcd_error == ESP_OK && !welcome_until && !overlay) {
            const terminal_cursor_t *cursor = &s_terminal.page[s_terminal.active].cursor;
            bool phase = paused || ((now / 500000) & 1) == 0;
            for (unsigned budget = 0; budget < 4; ++budget) {
                unsigned row = next_row++ % ROWS;
#if RETRO_TOUCH_ENABLE
                const terminal_cell_t *cells = terminal_history_row(&s_history, &s_terminal, row);
#else
                const terminal_cell_t *cells = terminal_cell(&s_terminal, 0, row);
#endif
                terminal_cell_t *old = s_previous + row * COLS;
                bool visible = !paused && s_terminal.cursor_visible && cursor->y == row && phase;
                bool blinking = false;
                for (unsigned x = 0; x < COLS; ++x) { blinking |= (cells[x].fg >> 24) & TERM_BLINK; }
                if (memcmp(old, cells, COLS * sizeof(*old)) == 0 && old_cursor[row] == visible &&
                    (!visible || old_x[row] == cursor->x) && (!blinking || old_phase[row] == phase)) { continue; }
                lcd_error = draw(row, cells, cursor->x, visible, phase);
                if (lcd_error != ESP_OK) { break; }
                memcpy(old, cells, COLS * sizeof(*old));
                old_cursor[row] = visible; old_x[row] = cursor->x; old_phase[row] = phase;
            }
            if (lcd_error == ESP_OK && !status_drawn) { lcd_error = status_line(paused, cr_newline); status_drawn = true; }
            if (lcd_error != ESP_OK) {
                app_health_fault(); ESP_LOGE("TFT", "Rendering stopped: %s; UART continues", esp_err_to_name(lcd_error));
            }
        }
#else
        (void)welcome_until;
        (void)paused;
#endif
#if RETRO_TOUCH_ENABLE
        /* Rendering/UART still progress every 10 ms; a GPIO IRQ wakes this
         * task earlier. Idle iterations never start touch ADC transactions. */
        if (touch) { (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)); }
        else { vTaskDelay(pdMS_TO_TICKS(10)); }
#else
        vTaskDelay(pdMS_TO_TICKS(10));
#endif
    }
}

esp_err_t screen_start(void)
{
    size_t count = COLS * ROWS;
    terminal_cell_t *primary = heap_caps_calloc(count, sizeof(*primary), APP_MEMORY_CAPS);
    terminal_cell_t *alternate = heap_caps_calloc(count, sizeof(*alternate), APP_MEMORY_CAPS);
    s_previous = heap_caps_calloc(count, sizeof(*s_previous), APP_MEMORY_CAPS);
    s_input = xQueueCreate(64, sizeof(input_t));
    if (!primary || !alternate || !s_previous || !s_input) { return ESP_ERR_NO_MEM; }
    if (!terminal_init(&s_terminal, COLS, ROWS, primary, alternate)) { return ESP_ERR_INVALID_ARG; }
#if RETRO_TOUCH_ENABLE
    terminal_cell_t *history = heap_caps_calloc(RETRO_SCROLLBACK_LINES * COLS, sizeof(*history), APP_MEMORY_CAPS);
    terminal_cell_t *snapshot = heap_caps_calloc(count, sizeof(*snapshot), APP_MEMORY_CAPS);
    if (!history || !snapshot) { return ESP_ERR_NO_MEM; }
    if (!terminal_history_init(&s_history, &s_terminal, history, RETRO_SCROLLBACK_LINES, snapshot)) { return ESP_ERR_INVALID_ARG; }
#endif
    terminal_set_cr_newline(&s_terminal, DEFAULT_CR_NEWLINE);
    app_health_beat(HEALTH_SCREEN);
    if (xTaskCreate(screen_task, "screen", 6144, NULL, 4, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    ESP_LOGI("SCREEN", "ANSI/VT100 %dx%d; %s cells; profile=%d touch=%d", COLS, ROWS, APP_MEMORY_NAME, RETRO_TFT_PROFILE, RETRO_TOUCH_ENABLE);
    return ESP_OK;
}
