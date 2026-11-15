#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
esp_err_t screen_start(void);
/* Single HC-05 UART1 RX producer; never waits for LCD, parsing or rendering. */
void screen_input(const uint8_t *data, size_t length);
void screen_stream_gap(void);
void screen_log(void);
