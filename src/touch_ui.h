#pragma once
#include "xpt2046.h"
#include <stddef.h>
void touch_ui_init(bool cr_newline);
void touch_ui_update(const xpt2046_sample_t *sample, uint32_t now_ms);
bool touch_ui_visible(void);
bool touch_ui_paused(void);
bool touch_ui_cr_newline(void);
uint32_t touch_ui_revision(void);
bool touch_ui_render_row(unsigned row, uint16_t *pixels, size_t capacity);
