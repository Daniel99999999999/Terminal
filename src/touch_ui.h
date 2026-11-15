#pragma once
#include "xpt2046.h"
#include <stddef.h>
void touch_ui_init(void);
void touch_ui_update(const xpt2046_sample_t *sample, uint32_t now_ms);
bool touch_ui_visible(void);
uint32_t touch_ui_revision(void);
bool touch_ui_render_row(unsigned row, uint16_t *pixels, size_t capacity);
