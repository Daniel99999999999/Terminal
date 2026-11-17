#include "terminal_toolbar.h"
#include "terminal_raster.h"
#include <string.h>

terminal_toolbar_button_t terminal_toolbar_hit(unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (width < 280 || height < TERMINAL_TOOLBAR_HEIGHT || x >= width || y >= height ||
        y < height - TERMINAL_TOOLBAR_HEIGHT || x >= width - 40) { return TOOLBAR_NONE; }
    return x < (width - 40) / 2 ? TOOLBAR_RESUME : TOOLBAR_CALIBRATE;
}

static bool label_pixel(const char *label, unsigned x, unsigned y, unsigned left, unsigned width)
{
    unsigned length = (unsigned)strlen(label), start = left + (width - length * 16) / 2;
    if (y < 4 || y >= 20 || x < start || x >= start + length * 16) { return false; }
    unsigned px = (x - start) / 2, py = (y - 4) / 2;
    return (terminal_glyph_row((uint8_t)label[px / 8], py) & (1u << (px % 8))) != 0;
}

bool terminal_toolbar_row(unsigned strip, unsigned width, bool paused, int link,
                          unsigned progress, uint16_t *pixels, size_t capacity)
{
    if (!pixels || width < 280 || width % 8 || strip >= TERMINAL_TOOLBAR_HEIGHT / 8 ||
        capacity < width * 8) { return false; }
    if (progress > 100) { progress = 100; }
    unsigned button_width = (width - 40) / 2;
    for (unsigned sy = 0; sy < 8; ++sy) {
        unsigned y = strip * 8 + sy;
        for (unsigned x = 0; x < width; ++x) {
            uint32_t color = 0x101820;
            if (x < width - 40) {
                unsigned index = x / button_width, bx = x % button_width;
                bool enabled = index != 0 || paused;
                if (bx >= 2 && bx < button_width - 2 && y >= 2 && y < 22) {
                    bool border = bx == 2 || bx == button_width - 3 || y == 2 || y == 21;
                    color = border ? 0x708090 : enabled ? 0x284058 : 0x202830;
                    const char *label = index == 0 ? (paused ? "WZNOW" : "LIVE") : "KALIBR.";
                    if (label_pixel(label, x, y, index * button_width, button_width)) {
                        color = enabled ? 0xffffff : 0x788088;
                    }
                    if (index == 1 && progress && y >= 20 && bx - 2 < (button_width - 4) * progress / 100) {
                        color = 0x40c070;
                    }
                }
            } else {
                /* A hollow gray circle is unknown, dark gray is down, blue is up. */
                int dx = (int)x - (int)(width - 20), dy = (int)y - 12;
                unsigned distance = (unsigned)(dx * dx + dy * dy);
                if (distance <= 36 && (link >= 0 || distance >= 16)) {
                    color = link > 0 ? 0x2080ff : link == 0 ? 0x384858 : 0x708090;
                }
            }
            pixels[sy * width + x] = terminal_rgb565(color);
        }
    }
    return true;
}
