#include "touch_calibration.h"
#include <math.h>

bool touch_calibration_valid(const touch_calibration_t *c)
{
    if (!c || c->width < 80 || c->width > 640 || c->height < 80 || c->height > 640) { return false; }
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(c->x[i]) || !isfinite(c->y[i]) || fabs(c->x[i]) > 10000 || fabs(c->y[i]) > 10000) { return false; }
    }
    return fabs((double)c->x[0] * c->y[1] - (double)c->x[1] * c->y[0]) > 1;
}

bool touch_calibration_map(const touch_calibration_t *c, uint16_t rx,
                           uint16_t ry, uint16_t *x, uint16_t *y)
{
    if (!x || !y || rx > 4095 || ry > 4095 || !touch_calibration_valid(c)) { return false; }
    double a = ((double)rx - 2048) / 2048, b = ((double)ry - 2048) / 2048;
    double px = c->x[0] * a + c->x[1] * b + c->x[2];
    double py = c->y[0] * a + c->y[1] * b + c->y[2];
    if (px < 0) { px = 0; } else if (px > c->width - 1) { px = c->width - 1; }
    if (py < 0) { py = 0; } else if (py > c->height - 1) { py = c->height - 1; }
    *x = (uint16_t)(px + 0.5); *y = (uint16_t)(py + 0.5);
    return true;
}

bool touch_calibration_fit(const touch_cal_point_t *p, size_t count,
                           uint16_t width, uint16_t height, touch_calibration_t *out)
{
    if (!p || !out || count < 3 || count > 8) { return false; }
    double m[3][5] = {{0}};
    for (size_t n = 0; n < count; ++n) {
        if (p[n].raw_x > 4095 || p[n].raw_y > 4095 || p[n].x >= width || p[n].y >= height) { return false; }
        double v[] = {((double)p[n].raw_x - 2048) / 2048, ((double)p[n].raw_y - 2048) / 2048, 1};
        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 3; ++j) { m[i][j] += v[i] * v[j]; }
            m[i][3] += v[i] * p[n].x; m[i][4] += v[i] * p[n].y;
        }
    }
    for (unsigned col = 0; col < 3; ++col) {
        unsigned pivot = col;
        for (unsigned row = col + 1; row < 3; ++row) {
            if (fabs(m[row][col]) > fabs(m[pivot][col])) { pivot = row; }
        }
        if (fabs(m[pivot][col]) < 1e-6) { return false; }
        for (unsigned j = 0; j < 5; ++j) { double t = m[col][j]; m[col][j] = m[pivot][j]; m[pivot][j] = t; }
        double divisor = m[col][col];
        for (unsigned j = col; j < 5; ++j) { m[col][j] /= divisor; }
        for (unsigned row = 0; row < 3; ++row) {
            if (row == col) { continue; }
            double factor = m[row][col];
            for (unsigned j = col; j < 5; ++j) { m[row][j] -= factor * m[col][j]; }
        }
    }
    touch_calibration_t candidate = { .width = width, .height = height };
    for (unsigned i = 0; i < 3; ++i) { candidate.x[i] = (float)m[i][3]; candidate.y[i] = (float)m[i][4]; }
    if (!touch_calibration_valid(&candidate)) { return false; }
    for (size_t n = 0; n < count; ++n) {
        uint16_t x, y;
        if (!touch_calibration_map(&candidate, p[n].raw_x, p[n].raw_y, &x, &y) ||
            fabs((double)x - p[n].x) > 8 || fabs((double)y - p[n].y) > 8) { return false; }
    }
    *out = candidate; /* Failed fits never replace a previous calibration. */
    return true;
}
