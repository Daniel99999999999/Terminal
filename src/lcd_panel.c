#include "lcd_panel.h"
#include "board_pins.h"
#include "terminal_raster.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_done;
static uint16_t *s_pixels;
static bool s_failed;
static bool s_visible;
static uint64_t s_written;
static int64_t s_command_ready;
static esp_err_t s_color_error;
static bool completed(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event, void *ctx);

/* Public esp_lcd IO interface, not a cast of an SDK private SPI structure.
 * The unchanged upstream driver keeps this stable handle while the real
 * transport is recreated for slow diagnostics and 10 MHz pixel writes. */
static void wait_until(int64_t deadline)
{
    int64_t remaining;
    while ((remaining = deadline - esp_timer_get_time()) > 0) {
        vTaskDelay(pdMS_TO_TICKS((remaining + 999) / 1000) + 1);
    }
}

static esp_err_t guarded_param(esp_lcd_panel_io_t *io, int cmd, const void *data, size_t size)
{
    (void)io;
    if (!s_io) { return ESP_ERR_INVALID_STATE; }
    wait_until(s_command_ready);
    esp_err_t err = esp_lcd_panel_io_tx_param(s_io, cmd, data, size);
    if (err == ESP_OK && cmd == 0x11) {
        s_command_ready = esp_timer_get_time() + 150000;
        ESP_LOGI("TFT", "SLPOUT guard >=150 ms before next command/data");
    }
    return err;
}

static esp_err_t guarded_color(esp_lcd_panel_io_t *io, int cmd, const void *data, size_t size)
{
    (void)io;
    wait_until(s_command_ready);
    s_color_error = s_io ? esp_lcd_panel_io_tx_color(s_io, cmd, data, size) : ESP_ERR_INVALID_STATE;
    return s_color_error;
}

static esp_err_t guarded_read(esp_lcd_panel_io_t *io, int cmd, void *data, size_t size)
{
    (void)io;
    wait_until(s_command_ready);
    return s_io ? esp_lcd_panel_io_rx_param(s_io, cmd, data, size) : ESP_ERR_INVALID_STATE;
}

static esp_lcd_panel_io_t s_guard = {
    .tx_param = guarded_param, .tx_color = guarded_color, .rx_param = guarded_read,
};

#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
static unsigned s_clock;
static esp_err_t spi_clock(unsigned hz)
{
    if (s_io && s_clock == hz) { return ESP_OK; }
    if (s_io) {
        esp_err_t err = esp_lcd_panel_io_del(s_io); /* Drain completed DMA descriptors. */
        if (err != ESP_OK) { return err; }
        s_io = NULL;
    }
    const esp_lcd_panel_io_spi_config_t io = { .cs_gpio_num = TFT_CS_GPIO,
        .dc_gpio_num = TFT_DC_GPIO, .spi_mode = 0, .pclk_hz = (int)hz,
        .cs_ena_pretrans = 2, .cs_ena_posttrans = 2,
        .trans_queue_depth = 1, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .on_color_trans_done = completed, .user_ctx = s_done };
    esp_err_t err = esp_lcd_new_panel_io_spi(SPI3_HOST, &io, &s_io);
    if (err == ESP_OK) {
        s_clock = hz;
        ESP_LOGI("TFT", "SPI3 requested clock=%u Hz; CS=GPIO%d setup/hold=2 cycles", hz, TFT_CS_GPIO);
    }
    return err;
}

static void registers(const char *stage)
{
    const struct { unsigned cmd, size; } reads[] = {{0xd3,4},{0x09,5},{0x0a,2},{0x0b,2},{0x0c,2}};
    for (unsigned i = 0; i < sizeof(reads) / sizeof(reads[0]); ++i) {
        uint8_t raw[5] = {0};
        esp_err_t err = esp_lcd_panel_io_rx_param(&s_guard, (int)reads[i].cmd, raw, reads[i].size);
        ESP_LOGI("LCD_REG", "%s cmd=%02x n=%u status=%s raw=%02x %02x %02x %02x %02x (dummy included; ID unverified)",
            stage, reads[i].cmd, reads[i].size, esp_err_to_name(err), raw[0],raw[1],raw[2],raw[3],raw[4]);
    }
}
#endif

static bool IRAM_ATTR completed(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *event, void *ctx)
{
    (void)io; (void)event;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)ctx, &wake);
    return wake == pdTRUE;
}

static esp_err_t high_output(int pin)
{
    esp_err_t err = gpio_set_level(pin, 1);
    return err == ESP_OK ? gpio_set_direction(pin, GPIO_MODE_OUTPUT) : err;
}
#endif

uint16_t *lcd_panel_pixels(void)
{
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    return s_pixels;
#else
    return NULL;
#endif
}

esp_err_t lcd_panel_write_row(unsigned y)
{
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    if (!s_panel || s_failed || y >= LCD_HEIGHT || (y & 7)) { return ESP_ERR_INVALID_STATE; }
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    terminal_raster_swap_bytes(s_pixels, LCD_STRIP_PIXELS);
#endif
    s_color_error = ESP_OK;
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, (int)y, LCD_WIDTH, (int)y + 8, s_pixels);
    if (err == ESP_OK) { err = s_color_error; } /* Upstream omits tx_color return. */
    if (err == ESP_OK && xSemaphoreTake(s_done, pdMS_TO_TICKS(250)) != pdTRUE) {
        err = ESP_ERR_TIMEOUT;
    }
    /* A timed-out transfer can still own the buffer: never reuse or free it. */
    s_failed = err != ESP_OK;
    if (err == ESP_OK) { s_written |= 1ULL << (y / 8); }
    return err;
#else
    (void)y;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t lcd_panel_open(void)
{
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    s_done = xSemaphoreCreateBinary();
    if (!s_done) { return ESP_ERR_NO_MEM; }
    esp_err_t err = high_output(TFT_CS_GPIO);
    if (err == ESP_OK) { err = high_output(TFT_RST_GPIO); }
    if (err != ESP_OK) { return err; }
    const size_t bytes = LCD_STRIP_PIXELS * sizeof(*s_pixels);
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    const spi_bus_config_t bus = { .sclk_io_num = TFT_SPI_SCLK_GPIO,
        .mosi_io_num = TFT_SPI_MOSI_GPIO, .miso_io_num = TFT_SPI_MISO_GPIO,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = bytes };
    err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) { return err; }
    err = spi_clock(1000000); /* Initialization and register reads below read limit. */
#else
    err = high_output(TFT_RD_GPIO); /* Write-only bus: RD stays inactive. */
    if (err != ESP_OK) { return err; }
    esp_lcd_i80_bus_handle_t bus_handle;
    const esp_lcd_i80_bus_config_t bus = { .dc_gpio_num = TFT_DC_GPIO,
        .wr_gpio_num = TFT_WR_GPIO, .data_gpio_nums = {TFT_D0_GPIO, TFT_D1_GPIO,
        TFT_D2_GPIO, TFT_D3_GPIO, TFT_D4_GPIO, TFT_D5_GPIO, TFT_D6_GPIO, TFT_D7_GPIO},
        .bus_width = 8, .max_transfer_bytes = bytes };
    err = esp_lcd_new_i80_bus(&bus, &bus_handle);
    if (err != ESP_OK) { return err; }
    const esp_lcd_panel_io_i80_config_t io = { .cs_gpio_num = TFT_CS_GPIO,
        .pclk_hz = RETRO_TFT_I80_HZ, .trans_queue_depth = 1,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8, .on_color_trans_done = completed,
        .user_ctx = s_done, .dc_levels = { .dc_idle_level = 1, .dc_data_level = 1 },
        .flags.swap_color_bytes = 1 };
    err = esp_lcd_new_panel_io_i80(bus_handle, &io, &s_io);
#endif
    if (err != ESP_OK) { return err; }
    s_pixels = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!s_pixels) { return ESP_ERR_NO_MEM; }
    const esp_lcd_panel_dev_config_t panel = { .reset_gpio_num = TFT_RST_GPIO,
        .bits_per_pixel = 16,
        .rgb_ele_order = RETRO_TFT_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB };
    err = esp_lcd_new_panel_ili9341(&s_guard, &panel, &s_panel);
    if (err == ESP_OK) { err = esp_lcd_panel_reset(s_panel); }
    if (err == ESP_OK) {
        /* ILI9341 DS 15.4: after resetting an awake panel, do not send
         * SLPOUT for 120 ms after RESX rises. The upstream reset returns
         * after only 10 ms; keep a margin before init sends SLPOUT first.
         * Only this screen task waits; UART reception continues in parallel. */
        int64_t reset_returned = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(150));
        ESP_LOGI("TFT", "LCD reset GPIO%d; init guard=%lu ms (minimum 120 ms); panel response unverified",
            TFT_RST_GPIO, (unsigned long)((esp_timer_get_time() - reset_returned) / 1000));
    }
    if (err == ESP_OK) { err = esp_lcd_panel_disp_on_off(s_panel, false); }
    if (err == ESP_OK) { err = esp_lcd_panel_init(s_panel); }
    /* ILI9341 rotations 0..3: MADCTL 0x48/0x28/0x88/0xe8 with BGR.
     * Inverting this MX table produces mirrored text, including landscape. */
    const bool mx[] = {true, false, false, true}, my[] = {false, false, true, true};
    if (err == ESP_OK) { err = esp_lcd_panel_swap_xy(s_panel, RETRO_TFT_ROTATION & 1); }
    if (err == ESP_OK) { err = esp_lcd_panel_mirror(s_panel, mx[RETRO_TFT_ROTATION], my[RETRO_TFT_ROTATION]); }
    if (err == ESP_OK) { err = esp_lcd_panel_invert_color(s_panel, RETRO_TFT_INVERT); }
    /* Keep the display OFF until the entire welcome image is in GRAM. */
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    if (err == ESP_OK) { registers("configured/off"); err = spi_clock(RETRO_TFT_SPI_HZ); }
#endif
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t lcd_panel_show(void)
{
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    if (!s_panel || s_failed || s_written != ((1ULL << (LCD_HEIGHT / 8)) - 1)) { return ESP_ERR_INVALID_STATE; }
    if (s_visible) { return ESP_OK; }
    esp_err_t err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err == ESP_OK) {
        wait_until(esp_timer_get_time() + 150000);
        ESP_LOGI("TFT", "DISPON settled >=150 ms; complete welcome frame submitted");
    }
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    if (err == ESP_OK) { err = spi_clock(1000000); }
    if (err == ESP_OK) { registers("display/on"); err = spi_clock(RETRO_TFT_SPI_HZ); }
#endif
#if RETRO_TFT_BACKLIGHT_CONTROL
    if (err == ESP_OK) { err = gpio_set_level(TFT_BACKLIGHT_EN_GPIO, RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL); }
    if (err == ESP_OK) { err = gpio_set_direction(TFT_BACKLIGHT_EN_GPIO, GPIO_MODE_OUTPUT); }
#endif
    s_visible = err == ESP_OK;
    return err;
#else
    return ESP_OK;
#endif
}
