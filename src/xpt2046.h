#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t x, y, pressure; /* Raw 12-bit ADC values, NOT calibrated pixels. */
    bool pressed;
} xpt2046_sample_t;

/* Single owner: TFT task. Separate SPI2 bus, no DMA; bounded IRQ-backed waits.
 * After a timeout, keep the descriptor alive and stop until reset. */
esp_err_t xpt2046_init(void);
esp_err_t xpt2046_read(xpt2046_sample_t *sample);
