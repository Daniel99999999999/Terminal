#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t x, y, pressure; /* Raw 12-bit ADC values, NOT calibrated pixels. */
    bool pressed;
} xpt2046_sample_t;

typedef struct {
    uint32_t polls, irq_low, valid, weak, bounds, end_high, io_errors;
    uint16_t last_x, last_y, last_pressure, last_z1, last_z2;
    bool last_irq_high;
} xpt2046_stats_t;

/* Single owner: TFT task. Separate SPI2 bus, no DMA; bounded IRQ-backed waits.
 * After a timeout, keep the descriptor alive and stop until reset. */
esp_err_t xpt2046_init(void);
esp_err_t xpt2046_read(xpt2046_sample_t *sample);
/* Same owner as read(); cumulative diagnostics, no I/O and no reset. */
void xpt2046_get_stats(xpt2046_stats_t *stats);
