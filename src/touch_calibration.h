#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { uint16_t raw_x, raw_y, x, y; } touch_cal_point_t;
typedef struct { uint16_t width, height; float x[3], y[3]; } touch_calibration_t;
/* Fit an affine map, including swapped/reversed axes, in display coordinates. */
bool touch_calibration_fit(const touch_cal_point_t *points, size_t count,
                           uint16_t width, uint16_t height, touch_calibration_t *out);
bool touch_calibration_valid(const touch_calibration_t *cal);
bool touch_calibration_map(const touch_calibration_t *cal, uint16_t raw_x,
                           uint16_t raw_y, uint16_t *x, uint16_t *y);
