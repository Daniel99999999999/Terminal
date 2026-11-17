#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TERMINAL_TOOLBAR_HEIGHT 24
typedef enum { TOOLBAR_NONE, TOOLBAR_RESUME, TOOLBAR_CALIBRATE } terminal_toolbar_button_t;
/* The rightmost 40 pixels belong to the BT indicator. */
terminal_toolbar_button_t terminal_toolbar_hit(unsigned x, unsigned y, unsigned width, unsigned height);
/* Render one 8-pixel strip in native RGB565. Link: -1 unknown, 0 down, 1 up. */
bool terminal_toolbar_row(unsigned strip, unsigned width, bool paused, int link,
                          unsigned progress, uint16_t *pixels, size_t capacity);
