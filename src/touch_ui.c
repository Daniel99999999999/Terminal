#include "touch_ui.h"
#include "sdkconfig.h"
#include "display_profile.h"
#if RETRO_TOUCH_ENABLE
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "lcd_panel.h"
#include "terminal_raster.h"
#include "touch_calibration.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "nvs.h"
#include "nvs_flash.h"

enum { CALIBRATE, VERIFY, TEST, TERMINAL };
enum { TAP_NONE = -1, TAP_PAUSE, TAP_RESUME, TAP_CR };
typedef struct {
    uint32_t magic, version, rotation;
    touch_calibration_t map;
    uint32_t crc;
} saved_t;
static touch_calibration_t s_map;
static touch_cal_point_t s_points[5];
static unsigned s_page, s_point, s_samples, s_releases, s_progress;
static uint32_t s_revision = 1, s_sum_x, s_sum_y, s_since, s_hold_since, s_last_valid;
static uint32_t s_contact_at;
static unsigned s_hold_progress;
static uint16_t s_min_x, s_max_x, s_min_y, s_max_y, s_x, s_y;
static bool s_wait_release = true, s_down, s_holding, s_nvs, s_saved, s_retry;
static bool s_paused, s_cr_newline;
static unsigned s_gap_resets, s_motion_resets, s_peak_samples;
static uint32_t s_report_at, s_reset_gap;
static int s_scroll_rows, s_scroll_y, s_start_y, s_start_x, s_tap = TAP_NONE;
static bool s_dragging;

static int tap_action(uint16_t x, uint16_t y)
{
    /* The text is 8 px high, but a finger needs a larger touch target.
     * Recorded footer taps mapped to y=222..231, above the old y>=232 zone. */
    bool footer = y >= LCD_HEIGHT - 24;
    if (footer && x >= LCD_WIDTH / 3 && x < LCD_WIDTH * 2 / 3) { return TAP_CR; }
    if (s_paused) { return footer && x < LCD_WIDTH / 3 ? TAP_RESUME : TAP_NONE; }
    return TAP_PAUSE; /* A plain terminal tap freezes; only WZNOW resumes. */
}

static void finish_gesture(void)
{
    if (s_page == TERMINAL && !s_dragging) {
        if (s_tap == TAP_PAUSE || s_tap == TAP_RESUME) {
            s_paused = s_tap == TAP_PAUSE; ++s_revision;
            ESP_LOGI("TOUCH", "Tap release -> %s", s_paused ? "PAUSE" : "LIVE");
        }
        if (s_tap == TAP_CR) {
            s_cr_newline = !s_cr_newline; ++s_revision;
            ESP_LOGI("TOUCH", "CR tap release -> %s", s_cr_newline ? "NL" : "CR");
        }
    }
    s_tap = TAP_NONE; s_dragging = false;
}

/* A missing ADC sample is not immediately a new physical press. Bound the
 * grace period so distinct taps and a real release still terminate gestures. */
#define CONTACT_GRACE_MS 80u

static void clear_measurement(void)
{
    s_samples = s_sum_x = s_sum_y = s_since = s_last_valid = 0;
    s_min_x = s_max_x = s_min_y = s_max_y = 0;
}

static void measurement_report(uint32_t now, bool accepted)
{
    if (!accepted && now - s_report_at < 1000) { return; }
    ESP_LOGI("TOUCH_CAL", "point=%u verify=%u accepted=%u samples=%u peak=%u gap_resets=%u motion_resets=%u last_gap_ms=%lu",
        s_point + 1, s_page == VERIFY ? 1u : 0u, accepted ? 1u : 0u,
        s_samples, s_peak_samples, s_gap_resets, s_motion_resets, (unsigned long)s_reset_gap);
    s_report_at = now;
}

static void reset_measurement(uint32_t now, bool moved)
{
    if (moved) { ++s_motion_resets; }
    else { ++s_gap_resets; s_reset_gap = now - s_last_valid; }
    measurement_report(now, false);
    clear_measurement();
}

static void stop_hold(void)
{
    if (s_holding) { ESP_LOGI("TOUCH", "CAL cancelled screen=%u,%u", s_x, s_y); }
    if (s_holding || s_hold_progress) { ++s_revision; }
    s_holding = false; s_hold_progress = 0;
}

static void target(unsigned *x, unsigned *y)
{
    const unsigned xs[] = {24, LCD_WIDTH - 25, LCD_WIDTH - 25, 24, LCD_WIDTH / 2};
    const unsigned ys[] = {24, 24, LCD_HEIGHT - 25, LCD_HEIGHT - 25, LCD_HEIGHT / 2};
    *x = s_page == VERIFY ? LCD_WIDTH / 3 : xs[s_point < 5 ? s_point : 4];
    *y = s_page == VERIFY ? LCD_HEIGHT * 2 / 3 : ys[s_point < 5 ? s_point : 4];
}

static bool load(void)
{
    nvs_handle_t handle;
    if (!s_nvs || nvs_open("retro_touch", NVS_READONLY, &handle) != ESP_OK) { return false; }
    saved_t value = {0}; size_t bytes = sizeof(value);
    esp_err_t err = nvs_get_blob(handle, "affine", &value, &bytes);
    nvs_close(handle);
    if (err != ESP_OK || bytes != sizeof(value) || value.magic != 0x58505443 || value.version != 1 ||
        value.rotation != RETRO_TFT_ROTATION || value.map.width != LCD_WIDTH || value.map.height != LCD_HEIGHT ||
        value.crc != esp_rom_crc32_le(0, (const uint8_t *)&value, offsetof(saved_t, crc)) ||
        !touch_calibration_valid(&value.map)) { return false; }
    s_map = value.map;
    return true;
}

static bool save(void)
{
#ifndef CONFIG_UART_ISR_IN_IRAM
    /* Manual touch builds without an IRAM UART ISR retain RAM-only settings.
     * The dedicated touch profile enables the SDK ISR for flash writes. */
    return false;
#else
    if (!s_nvs) { return false; }
    saved_t value = { .magic = 0x58505443, .version = 1, .rotation = RETRO_TFT_ROTATION, .map = s_map };
    value.crc = esp_rom_crc32_le(0, (const uint8_t *)&value, offsetof(saved_t, crc));
    nvs_handle_t handle;
    esp_err_t err = nvs_open("retro_touch", NVS_READWRITE, &handle);
    if (err != ESP_OK) { return false; }
    err = nvs_set_blob(handle, "affine", &value, sizeof(value));
    if (err == ESP_OK) { err = nvs_commit(handle); }
    nvs_close(handle);
    return err == ESP_OK;
#endif
}

static void restart(void)
{
    s_tap = TAP_NONE; s_dragging = false; s_scroll_rows = 0;
    s_page = CALIBRATE; s_point = s_releases = s_progress = 0;
    clear_measurement();
    s_gap_resets = s_motion_resets = s_peak_samples = 0;
    s_report_at = s_reset_gap = 0;
    s_wait_release = true; s_holding = false; s_hold_progress = 0; ++s_revision;
    s_paused = false;
}

void touch_ui_init(bool cr_newline)
{
    s_tap = TAP_NONE; s_dragging = false; s_scroll_rows = 0;
    s_paused = false; s_cr_newline = cr_newline;
    s_down = s_holding = false; s_contact_at = s_hold_progress = 0;
    s_wait_release = true; s_releases = 0;
    /* Preserve other namespaces. Never erase the existing NVS on init errors. */
    esp_err_t err = nvs_flash_init();
    s_nvs = err == ESP_OK;
    if (!s_nvs) { ESP_LOGW("TOUCH", "Settings unavailable (%s); calibration will last until reset", esp_err_to_name(err)); }
    if (load()) {
        s_page = TERMINAL; s_saved = true;
        ESP_LOGI("TOUCH", "Calibration loaded for rotation=%d %dx%d", RETRO_TFT_ROTATION, LCD_WIDTH, LCD_HEIGHT);
    } else {
        restart();
        ESP_LOGI("TOUCH", "Calibration: touch five targets, release between targets; then verification");
    }
}

static void captured(uint16_t rx, uint16_t ry)
{
    unsigned x, y; target(&x, &y);
    if (s_page == CALIBRATE) {
        s_points[s_point] = (touch_cal_point_t){rx, ry, (uint16_t)x, (uint16_t)y};
        ESP_LOGI("TOUCH", "Calibration point=%u raw=%u,%u screen=%u,%u", s_point + 1, rx, ry, x, y);
        if (++s_point == 5) {
            if (touch_calibration_fit(s_points, 5, LCD_WIDTH, LCD_HEIGHT, &s_map)) {
                s_page = VERIFY;
            } else { s_retry = true; restart(); }
        }
    } else {
        uint16_t px, py;
        if (!touch_calibration_map(&s_map, rx, ry, &px, &py) ||
            fabs((double)px - x) > 12 || fabs((double)py - y) > 12) {
            ESP_LOGW("TOUCH", "Independent verification failed; retry calibration");
            s_retry = true; restart();
        } else {
            s_saved = save(); s_page = TEST; s_retry = false; s_x = px; s_y = py;
            ESP_LOGI("TOUCH", "Calibration verified; %s", s_saved ? "saved" : "RAM only until reset");
        }
    }
    ++s_revision;
}

void touch_ui_update(const xpt2046_sample_t *sample, uint32_t now)
{
    if (!sample) { return; }
    if (!s_wait_release && (s_page == TEST || s_page == TERMINAL)) {
        if (s_down && now - s_contact_at >= CONTACT_GRACE_MS) {
            /* Contact is not proof of position during a long ADC/task gap.
             * Cancel CAL even when the next observation is already valid. */
            stop_hold();
            if (!sample->contact && !sample->pressed) { finish_gesture(); s_down = false; }
        }
        if (!sample->pressed && s_down) { return; } /* Preserve contact identity. */
    }
    if (sample->pressed) { s_contact_at = now; }
    if (s_wait_release) {
        if (sample->pressed || sample->contact) { s_releases = 0; s_down = true; }
        else if (++s_releases >= 3 && (!s_down || now - s_contact_at >= CONTACT_GRACE_MS)) {
            s_wait_release = false; s_releases = 0; clear_measurement(); s_down = false; ++s_revision;
        }
        return;
    }
    if (s_page == CALIBRATE || s_page == VERIFY) {
        /* Use elapsed time, not a count of rejected polls. At the observed
         * 25 ms poll interval one missing sample means 50 ms between valid
         * samples. Match the bounded contact grace used by gestures/release. */
        if (s_samples && now - s_last_valid >= CONTACT_GRACE_MS) {
            reset_measurement(now, false);
        }
        if (sample->pressed) {
            if (s_samples && (sample->x + 120 < s_max_x || sample->x > s_min_x + 120 ||
                sample->y + 120 < s_max_y || sample->y > s_min_y + 120)) {
                reset_measurement(now, true);
            }
            if (!s_samples) {
                s_since = now;
                s_min_x = s_max_x = sample->x; s_min_y = s_max_y = sample->y;
            }
            if (sample->x < s_min_x) { s_min_x = sample->x; }
            if (sample->x > s_max_x) { s_max_x = sample->x; }
            if (sample->y < s_min_y) { s_min_y = sample->y; }
            if (sample->y > s_max_y) { s_max_y = sample->y; }
            s_sum_x += sample->x; s_sum_y += sample->y; ++s_samples;
            s_last_valid = now;
            if (s_samples > s_peak_samples) { s_peak_samples = s_samples; }
            if (s_samples >= 8 && now - s_since >= 100) {
                measurement_report(now, true);
                captured((uint16_t)(s_sum_x / s_samples), (uint16_t)(s_sum_y / s_samples));
                s_wait_release = true; s_releases = 0; clear_measurement();
                s_gap_resets = s_motion_resets = s_peak_samples = 0;
                s_reset_gap = 0;
            }
        }
        unsigned progress = s_samples ? s_samples * 100 / 8 : 0;
        unsigned elapsed = now - s_since;
        if (progress > elapsed) { progress = elapsed; }
        if (progress > 100) { progress = 100; }
        progress = progress / 10 * 10;
        if (s_progress != progress) { s_progress = progress; ++s_revision; }
    } else if (sample->pressed) {
        uint16_t x, y;
        if (touch_calibration_map(&s_map, sample->x, sample->y, &x, &y)) {
            if (x != s_x || y != s_y) { s_x = x; s_y = y; ++s_revision; }
            if (!s_down) {
                ESP_LOGI("TOUCH", "UI press raw=%u,%u screen=%u,%u tap=%d", sample->x, sample->y, x, y,
                         s_page == TERMINAL ? tap_action(x, y) : TAP_NONE);
            }
            if (s_page == TEST && !s_down && y >= LCD_HEIGHT - 32 &&
                x >= LCD_WIDTH / 2 - 48 && x <= LCD_WIDTH / 2 + 48) {
                s_page = TERMINAL; ++s_revision;
            } else if (s_page == TERMINAL) {
                if (!s_down) {
                    s_start_x = x; s_start_y = s_scroll_y = y; s_dragging = false;
                    s_tap = tap_action(x, y);
                }
                int dy = (int)y - s_start_y, dx = (int)x - s_start_x;
                if (dy >= 24 || dy <= -24) { s_dragging = true; s_tap = TAP_NONE; }
                if (dx >= 24 || dx <= -24) { s_tap = TAP_NONE; }
                if (s_dragging) {
                    stop_hold();
                    int rows = ((int)y - s_scroll_y) / 8;
                    s_scroll_y += rows * 8;
                    s_scroll_rows += rows;
                    if (s_scroll_rows > 1024) { s_scroll_rows = 1024; }
                    if (s_scroll_rows < -1024) { s_scroll_rows = -1024; }
                    s_paused = true;
                    s_down = true;
                    return;
                }
                bool start_cal = !s_down && x >= LCD_WIDTH * 2 / 3 && y >= LCD_HEIGHT - 8;
                /* Hysteresis only after arming on the actual CAL button. */
                bool stay_cal = s_holding && x + 8 >= LCD_WIDTH * 2 / 3 && y >= LCD_HEIGHT - 24;
                if (start_cal || stay_cal) {
                    if (!s_holding) {
                        s_hold_since = now; s_holding = true;
                        ESP_LOGI("TOUCH", "CAL hold started screen=%u,%u", x, y);
                    }
                    uint32_t elapsed = now - s_hold_since;
                    unsigned progress = elapsed >= 2000 ? 100 : (elapsed / 200) * 10;
                    if (progress != s_hold_progress) { s_hold_progress = progress; ++s_revision; }
                    if (elapsed >= 2000) {
                        ESP_LOGI("TOUCH", "CAL hold accepted; starting calibration");
                        s_retry = false; restart();
                    }
                } else {
                    stop_hold();
                    /* Footer taps commit only on release: a drag may start
                     * anywhere, including PAUZA/CR/CAL, without firing a tap. */
                }
            } else { stop_hold(); }
        } else { stop_hold(); }
    } else { stop_hold(); }
    s_down = sample->pressed;
}

bool touch_ui_visible(void) { return s_page != TERMINAL; }
bool touch_ui_paused(void) { return s_paused; }
bool touch_ui_cr_newline(void) { return s_cr_newline; }
unsigned touch_ui_cal_progress(void) { return s_hold_progress; }
uint32_t touch_ui_revision(void) { return s_revision; }
int touch_ui_take_scroll(void) { int rows = s_scroll_rows; s_scroll_rows = 0; return rows; }

bool touch_ui_render_row(unsigned row, uint16_t *pixels, size_t capacity)
{
    if (!pixels || row >= LCD_HEIGHT / 8 || capacity < LCD_STRIP_PIXELS) { return false; }
    terminal_cell_t cells[LCD_WIDTH / 8];
    for (unsigned x = 0; x < LCD_WIDTH / 8; ++x) { cells[x] = (terminal_cell_t){ .codepoint = ' ', .fg = 0xffffff }; }
    const char *text = ""; char line[48];
    if (row == 1) { text = s_page == TEST ? "TEST DOTYKU" : "KALIBRACJA DOTYKU"; }
    else if (row == 4) { text = s_page == TEST ? "DOTKNIJ EKRANU" : s_wait_release ? "PUSC EKRAN" : "PRZYTRZYMAJ KRZYZYK"; }
    else if (row == 6) {
        if (s_page == TEST) { snprintf(line, sizeof(line), "X=%u  Y=%u", s_x, s_y); }
        else if (s_page == VERIFY) { snprintf(line, sizeof(line), "SPRAWDZENIE DOKLADNOSCI"); }
        else { snprintf(line, sizeof(line), "PUNKT %u / 5", s_point + 1); }
        text = line;
    } else if (row == 8 && s_page != TEST && !s_wait_release && s_progress) {
        snprintf(line, sizeof(line), "POMIAR %u%% - TRZYMAJ", s_progress); text = line;
    } else if (row == LCD_HEIGHT / 8 - 5) {
        text = s_page == TEST ? (s_saved ? "KALIBRACJA ZAPISANA" : "ZAPIS TYLKO DO RESETU") : s_retry ? "POMIAR DO POWTORZENIA" : "LOGI SA ODBIERANE W TLE";
    } else if (row == LCD_HEIGHT / 8 - 2 && s_page == TEST) { text = "[ GOTOWE ]"; }
    size_t length = strlen(text); if (length > LCD_WIDTH / 8) { length = LCD_WIDTH / 8; }
    for (size_t i = 0; i < length; ++i) { cells[(LCD_WIDTH / 8 - length) / 2 + i].codepoint = (uint8_t)text[i]; }
    if (!terminal_raster_row(cells, LCD_WIDTH / 8, 0, false, true, pixels, capacity)) { return false; }
    unsigned tx, ty;
    if (s_page == TEST) { tx = s_x; ty = s_y; } else { target(&tx, &ty); }
    unsigned radius = s_page == TEST ? 4 : 10;
    for (unsigned y = row * 8; y < row * 8 + 8; ++y) {
        for (unsigned x = 0; x < LCD_WIDTH; ++x) {
            unsigned dx = x > tx ? x - tx : tx - x, dy = y > ty ? y - ty : ty - y;
            if ((dx <= radius && y == ty) || (dy <= radius && x == tx)) {
                pixels[(y - row * 8) * LCD_WIDTH + x] = s_page == TEST ? 0x07e0 : 0xf800;
            }
        }
    }
    return true;
}
#else
void touch_ui_init(bool cr_newline) { (void)cr_newline; }
void touch_ui_update(const xpt2046_sample_t *sample, uint32_t now) { (void)sample; (void)now; }
bool touch_ui_visible(void) { return false; }
bool touch_ui_paused(void) { return false; }
int touch_ui_take_scroll(void) { return 0; }
bool touch_ui_cr_newline(void) { return false; }
unsigned touch_ui_cal_progress(void) { return 0; }
uint32_t touch_ui_revision(void) { return 0; }
bool touch_ui_render_row(unsigned row, uint16_t *pixels, size_t size) { (void)row; (void)pixels; (void)size; return false; }
#endif
