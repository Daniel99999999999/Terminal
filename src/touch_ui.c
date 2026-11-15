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
typedef struct {
    uint32_t magic, version, rotation;
    touch_calibration_t map;
    uint32_t crc;
} saved_t;
static touch_calibration_t s_map;
static touch_cal_point_t s_points[5];
static unsigned s_page, s_point, s_samples, s_releases, s_misses, s_progress;
static uint32_t s_revision = 1, s_sum_x, s_sum_y, s_since, s_hold_since, s_last_valid;
static uint32_t s_contact_at;
static unsigned s_hold_progress;
static uint16_t s_min_x, s_max_x, s_min_y, s_max_y, s_x, s_y;
static bool s_wait_release = true, s_down, s_holding, s_nvs, s_saved, s_retry;
static bool s_paused, s_cr_newline;

/* A missing ADC sample is not immediately a new physical press. Bound the
 * grace period so distinct taps and a real release still terminate gestures. */
#define CONTACT_GRACE_MS 80u

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
    s_page = CALIBRATE; s_point = s_samples = s_releases = 0;
    s_misses = s_progress = 0;
    s_wait_release = true; s_holding = false; s_hold_progress = 0; ++s_revision;
    s_paused = false;
}

void touch_ui_init(bool cr_newline)
{
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
    if (s_page == TEST || s_page == TERMINAL) {
        if (s_down && now - s_contact_at >= CONTACT_GRACE_MS) {
            s_down = false; stop_hold();
        }
        if (sample->pressed) { s_contact_at = now; }
        else if (s_down) { return; } /* Keep the gesture through a short gap. */
    }
    if (s_wait_release) {
        if (sample->pressed) { s_releases = 0; }
        else if (++s_releases >= 3) { s_wait_release = false; s_releases = 0; s_samples = 0; ++s_revision; }
        s_down = sample->pressed;
        return;
    }
    if (s_page == CALIBRATE || s_page == VERIFY) {
        if (!sample->pressed) {
            if (s_samples && (++s_misses >= 3 || now - s_last_valid >= 50)) { s_samples = s_misses = 0; }
        }
        else {
            if (s_samples && now - s_last_valid >= 50) { s_samples = 0; }
            if (!s_samples || sample->x + 120 < s_max_x || sample->x > s_min_x + 120 ||
                sample->y + 120 < s_max_y || sample->y > s_min_y + 120) {
                s_samples = 0; s_sum_x = s_sum_y = 0; s_since = now;
                s_min_x = s_max_x = sample->x; s_min_y = s_max_y = sample->y;
            }
            if (sample->x < s_min_x) { s_min_x = sample->x; }
            if (sample->x > s_max_x) { s_max_x = sample->x; }
            if (sample->y < s_min_y) { s_min_y = sample->y; }
            if (sample->y > s_max_y) { s_max_y = sample->y; }
            s_sum_x += sample->x; s_sum_y += sample->y; ++s_samples;
            s_last_valid = now; s_misses = 0;
            if (s_samples >= 8 && now - s_since >= 100) {
                captured((uint16_t)(s_sum_x / s_samples), (uint16_t)(s_sum_y / s_samples));
                s_wait_release = true; s_releases = 0; s_samples = 0;
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
                ESP_LOGI("TOUCH", "UI press raw=%u,%u screen=%u,%u", sample->x, sample->y, x, y);
            }
            if (s_page == TEST && !s_down && y >= LCD_HEIGHT - 32 &&
                x >= LCD_WIDTH / 2 - 48 && x <= LCD_WIDTH / 2 + 48) {
                s_page = TERMINAL; ++s_revision;
            } else if (s_page == TERMINAL) {
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
                    if (!s_down && y >= LCD_HEIGHT - 8 && x < LCD_WIDTH * 2 / 3) {
                        if (x < LCD_WIDTH / 3) { s_paused = !s_paused; }
                        else { s_cr_newline = !s_cr_newline; }
                        ++s_revision;
                    }
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
bool touch_ui_cr_newline(void) { return false; }
unsigned touch_ui_cal_progress(void) { return 0; }
uint32_t touch_ui_revision(void) { return 0; }
bool touch_ui_render_row(unsigned row, uint16_t *pixels, size_t size) { (void)row; (void)pixels; (void)size; return false; }
#endif
