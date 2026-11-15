#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_lcd_panel_io.h"

/* Called once by the display task. Retain resources after a DMA timeout. */
esp_err_t tft_bus_open(size_t bytes, esp_lcd_panel_io_color_trans_done_cb_t done,
                       void *context, esp_lcd_panel_io_handle_t *io, uint16_t **pixels);
void tft_bus_prepare_pixels(uint16_t *pixels, size_t count);
esp_err_t tft_bus_backlight(bool on);
const char *tft_bus_name(void);
