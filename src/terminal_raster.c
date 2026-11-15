#include "terminal_raster.h"
#include "../vendor/font8x8/font8x8_basic.h"
#include "../vendor/font8x8/font8x8_box.h"
#include "../vendor/font8x8/font8x8_ext_latin.h"

uint16_t terminal_rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f));
}

void terminal_raster_swap_bytes(uint16_t *pixels, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        pixels[i] = (uint16_t)((pixels[i] >> 8) | (pixels[i] << 8));
    }
}

uint8_t terminal_glyph_row(uint32_t cp, unsigned row)
{
    if (row >= 8) { return 0; }
    if (cp < 128) { return font8x8_basic[cp][row]; }
    if (cp >= 0x2500 && cp < 0x2580) { return font8x8_box[cp - 0x2500][row]; }
    if (cp >= 0xa0 && cp < 0x100) { return font8x8_ext_latin[cp - 0xa0][row]; }
    /* Small composites supplement the public-domain font with Polish letters. */
    char base = 0; unsigned accent = 0;
    switch (cp) {
    case 0x104: base = 'A'; accent = 3; break;
    case 0x105: base = 'a'; accent = 3; break;
    case 0x106: base = 'C'; accent = 1; break;
    case 0x107: base = 'c'; accent = 1; break;
    case 0x118: base = 'E'; accent = 3; break;
    case 0x119: base = 'e'; accent = 3; break;
    case 0x141: base = 'L'; accent = 4; break;
    case 0x142: base = 'l'; accent = 4; break;
    case 0x143: base = 'N'; accent = 1; break;
    case 0x144: base = 'n'; accent = 1; break;
    case 0x15a: base = 'S'; accent = 1; break;
    case 0x15b: base = 's'; accent = 1; break;
    case 0x179: base = 'Z'; accent = 1; break;
    case 0x17a: base = 'z'; accent = 1; break;
    case 0x17b: base = 'Z'; accent = 2; break;
    case 0x17c: base = 'z'; accent = 2; break;
    default: break;
    }
    if (base) {
        const uint8_t *glyph = font8x8_basic[(unsigned char)base];
        if (accent <= 2) { return row == 0 ? (accent == 1 ? 0x18 : 0x0c) : glyph[row - 1]; }
        return glyph[row] | (accent == 3 && row == 7 ? 0x0c : accent == 4 && row == 3 ? 0x1c : 0);
    }
    if (cp == 0x2588) { return 0xff; }
    if (cp == 0x2580) { return row < 4 ? 0xff : 0; }
    if (cp == 0x2584) { return row >= 4 ? 0xff : 0; }
    if (cp >= 0x23ba && cp <= 0x23bd) { return row == (cp - 0x23ba) * 2 ? 0xff : 0; }
    if (cp == 0x3c0) {
        static const uint8_t pi[8] = {0, 0, 0x7f, 0x36, 0x36, 0x36, 0x66, 0};
        return pi[row];
    }
    if (cp == 0x25c6) {
        static const uint8_t diamond[8] = {0, 0x08, 0x1c, 0x3e, 0x7f, 0x3e, 0x1c, 0x08};
        return diamond[row];
    }
    if (cp == 0xe000) { /* Private-use Bluetooth status icon. */
        static const uint8_t bt[8] = {0x08, 0x18, 0x2a, 0x1c, 0x1c, 0x2a, 0x18, 0x08};
        return bt[row];
    }
    return font8x8_basic['?'][row];
}

bool terminal_raster_row(const terminal_cell_t *cells, size_t cols,
                         size_t cursor_col, bool cursor_visible, bool blink_on,
                         uint16_t *pixels, size_t capacity)
{
    if (!cells || !pixels || !cols || cols > TERMINAL_MAX_COLS || capacity < cols * 64) {
        return false;
    }
    size_t width = cols * 8;
    for (size_t x = 0; x < cols; ++x) {
        uint32_t fg, bg;
        terminal_cell_colors(&cells[x], &fg, &bg);
        uint16_t foreground = terminal_rgb565(fg), background = terminal_rgb565(bg);
        unsigned flags = cells[x].fg >> 24;
        for (unsigned y = 0; y < 8; ++y) {
            uint8_t bits = terminal_glyph_row(cells[x].codepoint, y);
            if (flags & TERM_BOLD) { bits |= (uint8_t)(bits << 1); }
            if ((flags & TERM_UNDERLINE) && y == 7) { bits = 0xff; }
            if ((flags & TERM_BLINK) && !blink_on) { bits = 0; }
            if (cursor_visible && x == cursor_col && y >= 6) { bits ^= 0xff; }
            for (unsigned bit = 0; bit < 8; ++bit) {
                pixels[y * width + x * 8 + bit] = (bits & (1U << bit)) ? foreground : background;
            }
        }
    }
    return true;
}
