#pragma once
#include "terminal.h"

#define TERMINAL_GLYPH_WIDTH 8
#define TERMINAL_GLYPH_HEIGHT 8
uint16_t terminal_rgb565(uint32_t rgb);
/* In-place RGB565 byte order conversion for SPI wire format on ESP32. */
void terminal_raster_swap_bytes(uint16_t *pixels, size_t count);
uint8_t terminal_glyph_row(uint32_t codepoint, unsigned row);
/* Native-endian RGB565; transport swaps bytes for the 8-bit panel bus.
 * Capacity is measured in pixels. No I/O, allocation, or mutation of cells. */
bool terminal_raster_row(const terminal_cell_t *cells, size_t cols,
                         size_t cursor_col, bool cursor_visible, bool blink_on,
                         uint16_t *pixels, size_t capacity);
