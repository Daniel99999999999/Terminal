#include "screen.h"
#include <stdio.h>
#include <string.h>
#include "app_health.h"
#include "app_memory.h"
#include "lcd_panel.h"
#include "terminal.h"
#include "terminal_raster.h"
#include "xpt2046.h"
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
typedef struct { uint32_t sequence; uint16_t length; uint8_t data[128]; } input_t;
static QueueHandle_t s_input;
static uint32_t s_sequence, s_drops;
static uint32_t s_parsed, s_crc, s_controls;
static terminal_t s_terminal;
static terminal_cell_t *s_previous;

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
}

void screen_log(void)
{
    ESP_LOGI("SCREEN", "parsed=%lu drop=%lu controls=%lu CRC32=%08lx",
        (unsigned long)__atomic_load_n(&s_parsed, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_drops, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_controls, __ATOMIC_RELAXED),
        (unsigned long)__atomic_load_n(&s_crc, __ATOMIC_RELAXED));
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

static esp_err_t status_line(void)
{
    const char *text = "HC-05 UART1 115200 8N1";
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
    esp_err_t lcd_error = lcd_panel_open();
    int64_t welcome_until = 0;
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    if (lcd_error == ESP_OK) { lcd_error = welcome(&sequence); }
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
    bool pressed = false;
    if (!touch) { app_health_fault(); ESP_LOGE("TOUCH", "Initialization failed"); }
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
        if (lcd_error == ESP_OK && !welcome_until) {
            const terminal_cursor_t *cursor = &s_terminal.page[s_terminal.active].cursor;
            bool phase = ((now / 500000) & 1) == 0;
            for (unsigned budget = 0; budget < 4; ++budget) {
                unsigned row = next_row++ % ROWS;
                const terminal_cell_t *cells = terminal_cell(&s_terminal, 0, row);
                terminal_cell_t *old = s_previous + row * COLS;
                bool visible = s_terminal.cursor_visible && cursor->y == row && phase;
                bool blinking = false;
                for (unsigned x = 0; x < COLS; ++x) { blinking |= (cells[x].fg >> 24) & TERM_BLINK; }
                if (memcmp(old, cells, COLS * sizeof(*old)) == 0 && old_cursor[row] == visible &&
                    (!visible || old_x[row] == cursor->x) && (!blinking || old_phase[row] == phase)) { continue; }
                lcd_error = draw(row, cells, cursor->x, visible, phase);
                if (lcd_error != ESP_OK) { break; }
                memcpy(old, cells, COLS * sizeof(*old));
                old_cursor[row] = visible; old_x[row] = cursor->x; old_phase[row] = phase;
            }
            if (lcd_error == ESP_OK && !status_drawn) { lcd_error = status_line(); status_drawn = true; }
            if (lcd_error != ESP_OK) {
                app_health_fault(); ESP_LOGE("TFT", "Rendering stopped: %s; UART continues", esp_err_to_name(lcd_error));
            }
        }
#else
        (void)welcome_until;
#endif
#if RETRO_TOUCH_ENABLE
        if (touch) {
            xpt2046_sample_t sample;
            if (xpt2046_read(&sample) != ESP_OK) { touch = false; app_health_fault(); }
            else if (sample.pressed != pressed) {
                pressed = sample.pressed;
                ESP_LOGI("TOUCH", "%s raw x=%u y=%u pressure=%u", pressed ? "DOWN" : "UP", sample.x, sample.y, sample.pressure);
            }
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
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
    app_health_beat(HEALTH_SCREEN);
    if (xTaskCreate(screen_task, "screen", 6144, NULL, 4, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    ESP_LOGI("SCREEN", "ANSI/VT100 %dx%d; %s cells; profile=%d touch=%d", COLS, ROWS, APP_MEMORY_NAME, RETRO_TFT_PROFILE, RETRO_TOUCH_ENABLE);
    return ESP_OK;
}
