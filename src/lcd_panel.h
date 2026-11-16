#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "display_profile.h"
#define LCD_WIDTH ((RETRO_TFT_ROTATION & 1) ? 320 : 240)
#define LCD_HEIGHT ((RETRO_TFT_ROTATION & 1) ? 240 : 320)
#define LCD_STRIP_PIXELS (LCD_WIDTH * 8)
esp_err_t lcd_panel_open(void);
/* Called only after every welcome strip has completed. First visible image
 * is complete; the screen task starts its three-second timer afterwards. */
esp_err_t lcd_panel_show(void);
uint16_t *lcd_panel_pixels(void);
/* Owns its internal DMA buffer through completion, including on timeout. */
esp_err_t lcd_panel_write_row(unsigned y);
