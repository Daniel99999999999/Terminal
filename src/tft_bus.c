#include "tft_bus.h"
#include "board_pins.h"
#include "terminal_raster.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"

#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
static esp_err_t idle_high(int pin)
{
    esp_err_t err = gpio_set_level(pin, 1);
    return err == ESP_OK ? gpio_set_direction(pin, GPIO_MODE_OUTPUT) : err;
}
#endif

const char *tft_bus_name(void)
{
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    return "SPI3 DMA";
#elif RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_I80
    return "I80 DMA";
#else
    return "NONE";
#endif
}

esp_err_t tft_bus_backlight(bool on)
{
#if RETRO_TFT_BACKLIGHT_CONTROL
    return gpio_set_level(TFT_BACKLIGHT_EN_GPIO,
        on ? RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL : !RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL);
#else
    (void)on;
    return ESP_OK;
#endif
}

void tft_bus_prepare_pixels(uint16_t *pixels, size_t count)
{
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    terminal_raster_swap_bytes(pixels, count);
#else
    (void)pixels; (void)count; /* I80 hardware swaps bytes. */
#endif
}

esp_err_t tft_bus_open(size_t bytes, esp_lcd_panel_io_color_trans_done_cb_t done,
                       void *context, esp_lcd_panel_io_handle_t *io, uint16_t **pixels)
{
#if RETRO_TFT_PROFILE != RETRO_TFT_NONE
    if (!io || !pixels || !done || !bytes) { return ESP_ERR_INVALID_ARG; }
    *io = NULL; *pixels = NULL;
    esp_err_t err;
#if RETRO_TFT_BACKLIGHT_CONTROL
    err = tft_bus_backlight(false);
    if (err != ESP_OK) { return err; }
    err = gpio_set_direction(TFT_BACKLIGHT_EN_GPIO, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) { return err; }
#endif
    const int idle[] = {TFT_CS_GPIO, TFT_RST_GPIO, TFT_DC_GPIO,
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_I80
        TFT_RD_GPIO, TFT_WR_GPIO,
#endif
    };
    for (size_t i = 0; i < sizeof(idle) / sizeof(idle[0]); ++i) {
        err = idle_high(idle[i]);
        if (err != ESP_OK) { return err; }
    }
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    const spi_bus_config_t bus = {
        .sclk_io_num = TFT_SPI_SCLK_GPIO, .mosi_io_num = TFT_SPI_MOSI_GPIO,
        .miso_io_num = TFT_SPI_MISO_GPIO, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = (int)bytes,
    };
    err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) { return err; }
    const esp_lcd_panel_io_spi_config_t config = {
        .cs_gpio_num = TFT_CS_GPIO, .dc_gpio_num = TFT_DC_GPIO, .spi_mode = 0,
        .pclk_hz = RETRO_TFT_SPI_HZ, .trans_queue_depth = 1,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .on_color_trans_done = done, .user_ctx = context,
    };
    err = esp_lcd_new_panel_io_spi(SPI3_HOST, &config, io);
    if (err != ESP_OK) { return err; }
    *pixels = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    static esp_lcd_i80_bus_handle_t handle;
    const esp_lcd_i80_bus_config_t bus = {
        .dc_gpio_num = TFT_DC_GPIO, .wr_gpio_num = TFT_WR_GPIO,
        .data_gpio_nums = {TFT_D0_GPIO, TFT_D1_GPIO, TFT_D2_GPIO, TFT_D3_GPIO,
                          TFT_D4_GPIO, TFT_D5_GPIO, TFT_D6_GPIO, TFT_D7_GPIO},
        .bus_width = 8, .max_transfer_bytes = bytes,
    };
    err = esp_lcd_new_i80_bus(&bus, &handle);
    if (err != ESP_OK) { return err; }
    const esp_lcd_panel_io_i80_config_t config = {
        .cs_gpio_num = TFT_CS_GPIO, .pclk_hz = RETRO_TFT_I80_HZ,
        .trans_queue_depth = 1, .on_color_trans_done = done, .user_ctx = context,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .dc_levels = {.dc_idle_level = 1, .dc_cmd_level = 0, .dc_dummy_level = 0, .dc_data_level = 1},
        .flags.swap_color_bytes = 1,
    };
    err = esp_lcd_new_panel_io_i80(handle, &config, io);
    if (err != ESP_OK) { return err; }
    *pixels = esp_lcd_i80_alloc_draw_buffer(*io, bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
    return *pixels ? ESP_OK : ESP_ERR_NO_MEM;
#else
    (void)bytes; (void)done; (void)context; (void)io; (void)pixels;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
