#include "tft_display.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "board_pins.h"
#include "debug_led.h"
#include "hc05_bridge.h"
#include "terminal_raster.h"
#include "terminal_view.h"
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

#define TFT_WIDTH  ((CONFIG_RETRO_TFT_ROTATION & 1) ? 320 : 240)
#define TFT_HEIGHT ((CONFIG_RETRO_TFT_ROTATION & 1) ? 240 : 320)
#define STRIP_PIXELS (TFT_WIDTH * 8)

static esp_lcd_i80_bus_handle_t s_bus;
static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_done;
static uint16_t *s_pixels;
static bool s_started;

bool tft_display_enabled(void)
{
#ifdef CONFIG_RETRO_TFT_ENABLE
    return true;
#else
    return false;
#endif
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
    /* Set inactive values before enabling outputs. RD remains HIGH: no unsafe
     * automatic ID read through unverified bidirectional HC245 paths. */
    const int idle[] = {TFT_CS_GPIO, TFT_RD_GPIO, TFT_WR_GPIO, TFT_RST_GPIO, TFT_DC_GPIO};
    for (size_t i = 0; i < sizeof(idle) / sizeof(idle[0]); ++i) {
        esp_err_t err = gpio_set_level(idle[i], 1);
        if (err != ESP_OK) { return err; }
        err = gpio_set_direction(idle[i], GPIO_MODE_OUTPUT);
        if (err != ESP_OK) { return err; }
    }
    const esp_lcd_i80_bus_config_t bus = {
        .dc_gpio_num = TFT_DC_GPIO, .wr_gpio_num = TFT_WR_GPIO,
        .data_gpio_nums = {TFT_D0_GPIO, TFT_D1_GPIO, TFT_D2_GPIO, TFT_D3_GPIO,
                          TFT_D4_GPIO, TFT_D5_GPIO, TFT_D6_GPIO, TFT_D7_GPIO},
        .bus_width = 8, .max_transfer_bytes = STRIP_PIXELS * sizeof(uint16_t),
    };
    esp_err_t err = esp_lcd_new_i80_bus(&bus, &s_bus);
    if (err != ESP_OK) { return err; }
    const esp_lcd_panel_io_i80_config_t io = {
        .cs_gpio_num = TFT_CS_GPIO, .pclk_hz = CONFIG_RETRO_TFT_CLOCK_HZ,
        .trans_queue_depth = 1, .on_color_trans_done = color_done, .user_ctx = s_done,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .dc_levels = { .dc_idle_level = 1, .dc_cmd_level = 0,
                       .dc_dummy_level = 0, .dc_data_level = 1 },
        .flags.swap_color_bytes = 1,
    };
    err = esp_lcd_new_panel_io_i80(s_bus, &io, &s_io);
    if (err != ESP_OK) { return err; }
    s_pixels = esp_lcd_i80_alloc_draw_buffer(s_io, STRIP_PIXELS * sizeof(*s_pixels),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_pixels) { return ESP_ERR_NO_MEM; }
    const esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = TFT_RST_GPIO, .bits_per_pixel = 16,
#ifdef CONFIG_RETRO_TFT_BGR
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
#else
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
    };
    err = esp_lcd_new_panel_ili9341(s_io, &panel, &s_panel);
    if (err == ESP_OK) { err = esp_lcd_panel_reset(s_panel); }
    if (err == ESP_OK) { err = esp_lcd_panel_init(s_panel); }
    const bool mx[] = {false, true, true, false}, my[] = {false, false, true, true};
    if (err == ESP_OK) { err = esp_lcd_panel_swap_xy(s_panel, (CONFIG_RETRO_TFT_ROTATION & 1) != 0); }
    if (err == ESP_OK) { err = esp_lcd_panel_mirror(s_panel,
        mx[CONFIG_RETRO_TFT_ROTATION], my[CONFIG_RETRO_TFT_ROTATION]); }
#ifdef CONFIG_RETRO_TFT_INVERT
    if (err == ESP_OK) { err = esp_lcd_panel_invert_color(s_panel, true); }
#endif
    if (err == ESP_OK) { err = esp_lcd_panel_disp_on_off(s_panel, true); }
    return err;
}

static esp_err_t show_color_test(void)
{
#ifdef CONFIG_RETRO_TFT_COLOR_TEST
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
    esp_err_t err = panel_init();
    if (err == ESP_OK) { err = show_color_test(); }
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
        ESP_LOGI("TFT", "ILI9341 selected by user; I80 DMA %dx%d, %ux%u cells; physical image unverified",
                 TFT_WIDTH, TFT_HEIGHT, (unsigned)cols, (unsigned)rows);
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
        xTaskDelayUntil(&last, pdMS_TO_TICKS(33));
    }
    debug_led_fault();
    ESP_LOGE("TFT", "Rendering stopped: %s; UART bridge remains active; DMA resources retained", esp_err_to_name(err));
    /* The handle stays valid, no live DMA buffer is released after a timeout. */
    for (;;) { vTaskSuspend(NULL); }
}

esp_err_t tft_display_start(void)
{
    if (!tft_display_enabled()) {
        ESP_LOGW("TFT", "Disabled: verify shield/HC245/3.3V and enable RetroTerminal TFT in menuconfig; no LCD GPIO touched");
        return ESP_OK;
    }
    if (s_started) { return ESP_ERR_INVALID_STATE; }
    if (xTaskCreate(display_task, "tft", 6144, NULL, 4, NULL) != pdPASS) { return ESP_ERR_NO_MEM; }
    s_started = true;
    return ESP_OK;
}
