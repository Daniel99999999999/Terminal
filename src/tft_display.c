#include "tft_display.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "board_pins.h"
#include "debug_led.h"
#include "hc05_bridge.h"
#include "terminal_raster.h"
#include "terminal_view.h"
#include "tft_bus.h"
#include "xpt2046.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TFT_WIDTH  ((RETRO_TFT_ROTATION & 1) ? 320 : 240)
#define TFT_HEIGHT ((RETRO_TFT_ROTATION & 1) ? 240 : 320)
#define STRIP_PIXELS (TFT_WIDTH * 8)

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_done;
static uint16_t *s_pixels;
static bool s_started;

bool tft_display_enabled(void)
{
    return RETRO_TFT_PROFILE != RETRO_TFT_NONE;
}

uint16_t tft_display_cols(void) { return TFT_WIDTH / 8; }
uint16_t tft_display_rows(void) { return TFT_HEIGHT / 8 - 1; }

static bool IRAM_ATTR color_done(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t *event, void *context)
{
    (void)io; (void)event;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)context, &wake);
    return wake == pdTRUE;
}

static esp_err_t draw_strip(unsigned y)
{
    tft_bus_prepare_pixels(s_pixels, STRIP_PIXELS);
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, (int)y,
        TFT_WIDTH, (int)y + 8, s_pixels);
    if (err != ESP_OK) { return err; }
    /* The buffer remains owned by DMA until this completion. On timeout we
     * stop rendering and retain all resources; never free/reuse a live buffer. */
    return xSemaphoreTake(s_done, pdMS_TO_TICKS(250)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t panel_init(void)
{
    s_done = xSemaphoreCreateBinary();
    if (!s_done) { return ESP_ERR_NO_MEM; }
    esp_err_t err = tft_bus_open(STRIP_PIXELS * sizeof(*s_pixels), color_done,
                                 s_done, &s_io, &s_pixels);
    if (err != ESP_OK) { return err; }
    const esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = TFT_RST_GPIO, .bits_per_pixel = 16,
#if RETRO_TFT_BGR
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
#else
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
    };
    err = esp_lcd_new_panel_ili9341(s_io, &panel, &s_panel);
    if (err == ESP_OK) { err = esp_lcd_panel_reset(s_panel); }
    if (err == ESP_OK) { err = esp_lcd_panel_init(s_panel); }
    const bool mx[] = {false, true, true, false}, my[] = {false, false, true, true};
    if (err == ESP_OK) { err = esp_lcd_panel_swap_xy(s_panel, (RETRO_TFT_ROTATION & 1) != 0); }
    if (err == ESP_OK) { err = esp_lcd_panel_mirror(s_panel,
        mx[RETRO_TFT_ROTATION], my[RETRO_TFT_ROTATION]); }
#if RETRO_TFT_INVERT
    if (err == ESP_OK) { err = esp_lcd_panel_invert_color(s_panel, true); }
#endif
    if (err == ESP_OK) { err = esp_lcd_panel_disp_on_off(s_panel, true); }
    return err;
}

static esp_err_t show_color_test(void)
{
#if RETRO_TFT_COLOR_TEST
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0};
    for (unsigned y = 0; y < TFT_HEIGHT; y += 8) {
        debug_led_heartbeat(DEBUG_LED_LCD);
        for (unsigned p = 0; p < STRIP_PIXELS; ++p) {
            s_pixels[p] = colors[(p % TFT_WIDTH) * 5 / TFT_WIDTH];
        }
        esp_err_t err = draw_strip(y);
        if (err != ESP_OK) { return err; }
    }
    for (unsigned tick = 0; tick < 8; ++tick) {
        debug_led_heartbeat(DEBUG_LED_LCD);
        vTaskDelay(pdMS_TO_TICKS(250)); /* Test display hold, not a communication loop. */
    }
#endif
    return ESP_OK;
}

static esp_err_t draw_status(hc05_status_t state)
{
    terminal_cell_t cells[TFT_WIDTH / 8];
    char label[48];
    const char *text = state == HC05_CONNECTED ? "CONNECTED" : state == HC05_DISCONNECTED ?
        "OFFLINE" : state == HC05_AT ? "AT MODE" : "? NO STATE";
    snprintf(label, sizeof(label), "  BT %s  UART2 115200", text);
    size_t n = strlen(label);
    uint32_t color = state == HC05_CONNECTED ? 0x3399ff :
        state == HC05_AT ? 0xffff00 : state == HC05_DISCONNECTED ? 0xff5555 : 0xaaaaaa;
    for (unsigned x = 0; x < TFT_WIDTH / 8; ++x) {
        cells[x] = (terminal_cell_t){ .codepoint = x == 0 ? 0xe000 : x < n ? (uint8_t)label[x] : ' ',
                                     .fg = color, .bg = 0x101010 };
    }
    if (!terminal_raster_row(cells, TFT_WIDTH / 8, 0, false, true, s_pixels, STRIP_PIXELS)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return draw_strip(TFT_HEIGHT - 8);
}

static void display_task(void *arg)
{
    (void)arg;
    debug_led_heartbeat(DEBUG_LED_LCD);
    ESP_LOGI("TFT", "Init begin: %s; CS=%d DC=%d RESET=%d; touch=%d",
             tft_bus_name(), TFT_CS_GPIO, TFT_DC_GPIO, TFT_RST_GPIO, RETRO_TOUCH_ENABLE);
    esp_err_t err = panel_init();
    if (err == ESP_OK) { ESP_LOGI("TFT", "Controller init complete; color_test=%d", RETRO_TFT_COLOR_TEST); }
    if (err == ESP_OK) { err = tft_bus_backlight(true); }
    if (err == ESP_OK) { err = show_color_test(); }
#if RETRO_TOUCH_ENABLE
    bool touch_active = false, touch_pressed = false;
    if (err == ESP_OK) {
        esp_err_t touch_err = xpt2046_init();
        touch_active = touch_err == ESP_OK;
        if (!touch_active) {
            debug_led_fault();
            ESP_LOGE("TOUCH", "Init failed: %s; display continues", esp_err_to_name(touch_err));
        }
    }
#endif
    size_t cols = tft_display_cols(), rows = tft_display_rows();
    terminal_cell_t *previous = heap_caps_calloc(cols * rows, sizeof(*previous),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!previous) { err = ESP_ERR_NO_MEM; }
    terminal_cell_t cells[TFT_WIDTH / 8];
    uint16_t old_cursor[TFT_HEIGHT / 8] = {0};
    bool old_visible[TFT_HEIGHT / 8] = {0}, old_phase[TFT_HEIGHT / 8] = {0};
    hc05_status_t old_status = (hc05_status_t)-1;
    unsigned next_row = 0;
    TickType_t last = xTaskGetTickCount();
    if (err == ESP_OK) {
        ESP_LOGI("TFT", "ILI9341 selected by user; %s %dx%d, %ux%u cells; physical image unverified",
                 tft_bus_name(), TFT_WIDTH, TFT_HEIGHT, (unsigned)cols, (unsigned)rows);
    }
    while (err == ESP_OK) {
        debug_led_heartbeat(DEBUG_LED_LCD);
        bool phase = ((esp_timer_get_time() / 500000) & 1) == 0;
        for (unsigned budget = 0; budget < 8; ++budget) {
            unsigned row = next_row++ % rows;
            terminal_view_info_t info;
            if (!terminal_view_copy_row(row, cells, cols, &info)) { continue; }
            if (info.cols != cols || info.rows != rows) { err = ESP_ERR_INVALID_SIZE; break; }
            bool visible = info.cursor_visible && info.cursor_y == row && phase;
            bool blinking = false;
            for (size_t x = 0; x < cols; ++x) {
                blinking |= ((cells[x].fg >> 24) & TERM_BLINK) != 0;
            }
            terminal_cell_t *old = previous + row * cols;
            if (memcmp(old, cells, cols * sizeof(*old)) == 0 &&
                old_visible[row] == visible && (!visible || old_cursor[row] == info.cursor_x) &&
                (!blinking || old_phase[row] == phase)) { continue; }
            if (!terminal_raster_row(cells, cols, info.cursor_x, visible, phase,
                                      s_pixels, STRIP_PIXELS)) { err = ESP_ERR_INVALID_SIZE; break; }
            err = draw_strip(row * 8);
            if (err != ESP_OK) { break; }
            memcpy(old, cells, cols * sizeof(*old));
            old_visible[row] = visible; old_cursor[row] = info.cursor_x; old_phase[row] = phase;
        }
        hc05_status_t status = hc05_bridge_status();
        if (err == ESP_OK && status != old_status) { err = draw_status(status); old_status = status; }
#if RETRO_TOUCH_ENABLE
        if (touch_active) {
            xpt2046_sample_t touch;
            esp_err_t touch_err = xpt2046_read(&touch);
            if (touch_err != ESP_OK) {
                touch_active = false;
                debug_led_fault();
                ESP_LOGE("TOUCH", "Read failed: %s; display continues", esp_err_to_name(touch_err));
            } else if (touch.pressed != touch_pressed) {
                touch_pressed = touch.pressed;
                ESP_LOGI("TOUCH", "%s raw x=%u y=%u pressure=%u; calibration/UI pending",
                         touch.pressed ? "DOWN" : "UP", touch.x, touch.y, touch.pressure);
            }
        }
#endif
        xTaskDelayUntil(&last, pdMS_TO_TICKS(33));
    }
    debug_led_fault();
    (void)tft_bus_backlight(false);
    ESP_LOGE("TFT", "Rendering stopped: %s; UART bridge remains active; DMA resources retained", esp_err_to_name(err));
    /* The handle stays valid, no live DMA buffer is released after a timeout. */
    for (;;) { vTaskSuspend(NULL); }
}

esp_err_t tft_display_start(void)
{
    if (!tft_display_enabled()) {
        ESP_LOGW("TFT", "Disabled: RETRO_TFT_PROFILE=NONE; select verified I80 or SPI wiring in display_profile.h/build_flags; no LCD GPIO touched");
        return ESP_OK;
    }
    if (s_started) { return ESP_ERR_INVALID_STATE; }
    if (xTaskCreate(display_task, "tft", 6144, NULL, 4, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    s_started = true;
    return ESP_OK;
}
