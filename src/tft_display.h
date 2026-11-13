#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

bool tft_display_enabled(void);
uint16_t tft_display_cols(void);
uint16_t tft_display_rows(void);
/* Initializes no GPIOs when disabled. Panel/DMA work runs in its own task. */
esp_err_t tft_display_start(void);
