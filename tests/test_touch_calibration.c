#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "touch_calibration.h"
#include "touch_ui.h"
#include "lcd_panel.h"
#include "nvs.h"
#include "esp_rom_crc.h"
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static unsigned char stored[128];
static size_t stored_size;
static unsigned writes, commits;
static uint32_t now_ms;

uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *data, uint32_t size)
{
    crc = ~crc;
    for (uint32_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) { crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u))); }
    }
    return ~crc;
}
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle)
{ CHECK(strcmp(name, "retro_touch") == 0); CHECK(mode == NVS_READONLY || mode == NVS_READWRITE); *handle = 1; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { CHECK(handle == 1); }
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *size)
{
    CHECK(handle == 1 && strcmp(key, "affine") == 0);
    if (!stored_size) { return 1; }
    CHECK(*size >= stored_size); memcpy(data, stored, stored_size); *size = stored_size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size)
{ CHECK(handle == 1 && strcmp(key, "affine") == 0 && size <= sizeof(stored)); memcpy(stored, data, size); stored_size = size; ++writes; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t handle) { CHECK(handle == 1); ++commits; return ESP_OK; }

static void release(void)
{
    xpt2046_sample_t sample = {0};
    for (unsigned i = 0; i < 5; ++i) { touch_ui_update(&sample, now_ms); now_ms += 20; }
}
static void hold(unsigned x, unsigned y)
{
    xpt2046_sample_t sample = { .pressed = true, .x = (uint16_t)(400 + x * 10), .y = (uint16_t)(350 + y * 12) };
    for (unsigned i = 0; i < 12; ++i) { touch_ui_update(&sample, now_ms); now_ms += 20; }
}
static void short_hold(unsigned x, unsigned y)
{
    xpt2046_sample_t sample = { .pressed = true, .x = (uint16_t)(400+x*10), .y = (uint16_t)(350+y*12) };
    for (unsigned i = 0; i < 4; ++i) { touch_ui_update(&sample,now_ms); now_ms += 20; }
}
static void five(void)
{
    const unsigned xs[] = {24, LCD_WIDTH-25, LCD_WIDTH-25, 24, LCD_WIDTH/2};
    const unsigned ys[] = {24, 24, LCD_HEIGHT-25, LCD_HEIGHT-25, LCD_HEIGHT/2};
    for (unsigned i = 0; i < 5; ++i) { release(); hold(xs[i], ys[i]); }
}

static void gesture_sample(unsigned x, unsigned y, bool pressed)
{
    xpt2046_sample_t sample = { .pressed = pressed,
        .x = (uint16_t)(400 + x * 10), .y = (uint16_t)(350 + y * 12) };
    touch_ui_update(&sample, now_ms); now_ms += 20;
}

static void noisy_calibration(unsigned period_ms, bool two_missing)
{
    stored_size = 0;
    unsigned before = writes;
    touch_ui_init(true);
    const unsigned xs[] = {24,295,295,24,160,106}, ys[] = {24,24,215,215,120,160};
    for (unsigned point = 0; point < 6; ++point) {
        release();
        for (unsigned n = 0; n < 40; ++n) {
            bool missing = two_missing ? (n % 5 == 1 || n % 5 == 2) : n % 3 == 1;
            xpt2046_sample_t sample = { .pressed = !missing,
                .x = (uint16_t)(400 + xs[point]*10 + (n%2 ? 20 : 0)),
                .y = (uint16_t)(350 + ys[point]*12) };
            touch_ui_update(&sample, now_ms); now_ms += period_ms;
        }
        CHECK(writes == before + (point == 5 ? 1u : 0u));
    }
    CHECK(commits == writes);
    release(); hold(LCD_WIDTH/2, LCD_HEIGHT-16); CHECK(!touch_ui_visible());
    touch_ui_init(true); CHECK(!touch_ui_visible());
}

int main(void)
{
    touch_cal_point_t points[] = {{0,0,24,24},{0,0,295,24},{0,0,295,215},{0,0,24,215},{0,0,160,120}};
    for (unsigned n = 0; n < 5; ++n) {
        /* Swapped raw axes and reversed horizontal direction. */
        points[n].raw_x = (uint16_t)(350 + points[n].y * 12);
        points[n].raw_y = (uint16_t)(3800 - points[n].x * 10);
    }
    touch_calibration_t cal = {0}; uint16_t x,y;
    CHECK(touch_calibration_fit(points, 5, 320, 240, &cal));
    CHECK(touch_calibration_map(&cal, 350 + 170 * 12, 3800 - 80 * 10, &x, &y));
    CHECK(x == 80 && y == 170);
    CHECK(touch_calibration_map(&cal, 4095, 0, &x, &y) && x == 319 && y == 239);
    touch_calibration_t old = cal;
    points[4].x = 250;
    CHECK(!touch_calibration_fit(points, 5, 320, 240, &cal));
    CHECK(memcmp(&cal, &old, sizeof(cal)) == 0);
    for (unsigned n = 0; n < 5; ++n) { points[n].raw_x = points[n].raw_y = 1000; }
    CHECK(!touch_calibration_fit(points, 5, 320, 240, &cal));
    CHECK(!touch_calibration_fit(points, 2, 320, 240, &cal));
    CHECK(!touch_calibration_map(&cal, 4096, 0, &x, &y));
    cal.x[0] = NAN; CHECK(!touch_calibration_valid(&cal));
    cal = old;
    /* Rounding near an edge must never return width or height. */
    cal.x[0] = 160; cal.x[1] = 0; cal.x[2] = 159.8f;
    cal.y[0] = 0; cal.y[1] = 120; cal.y[2] = 119.8f;
    CHECK(touch_calibration_map(&cal, 4095, 4095, &x, &y) && x == 319 && y == 239);

    touch_ui_init(true); CHECK(touch_ui_visible());
    /* Fully released short presses must not combine. */
    release(); short_hold(24,24); release(); short_hold(24,24); release();
    /* One missing sample in a stable press must not discard all progress.
     * Neither side has eight consecutive samples, so the old filter fails. */
    short_hold(24,24);
    xpt2046_sample_t missing = {0}; touch_ui_update(&missing,now_ms); now_ms += 20;
    short_hold(24,24);
    hold(295,24); /* Holding cannot capture two targets. */
    for (unsigned i = 1; i < 5; ++i) {
        const unsigned xs[] = {24,295,295,24,160}, ys[] = {24,24,215,215,120};
        release(); hold(xs[i],ys[i]);
    }
    CHECK(writes == 0 && commits == 0);
    release(); hold(250,60); /* Independent verification rejects a wrong tap. */
    CHECK(writes == 0 && touch_ui_visible());
    five(); CHECK(writes == 0);
    release(); hold(LCD_WIDTH/3,LCD_HEIGHT*2/3);
    CHECK(writes == 1 && commits == 1 && touch_ui_visible());
    uint16_t pixels[LCD_STRIP_PIXELS + 2];
    pixels[0] = pixels[LCD_STRIP_PIXELS+1] = 0xa55a;
    for (unsigned row = 0; row < LCD_HEIGHT/8; ++row) { CHECK(touch_ui_render_row(row,pixels+1,LCD_STRIP_PIXELS)); }
    CHECK(pixels[0] == 0xa55a && pixels[LCD_STRIP_PIXELS+1] == 0xa55a);
    CHECK(!touch_ui_render_row(0,pixels,1));
    release(); hold(LCD_WIDTH/2,LCD_HEIGHT-16); CHECK(!touch_ui_visible());
    touch_ui_init(true); CHECK(!touch_ui_visible()); /* Persistent calibration restored. */
    CHECK(!touch_ui_paused() && touch_ui_cr_newline());
    release(); hold(20,LCD_HEIGHT-3); CHECK(!touch_ui_paused());
    /* A lost ADC sample during one contact must not toggle PAUZA twice. */
    gesture_sample(0,0,false); hold(20,LCD_HEIGHT-3); CHECK(!touch_ui_paused());
    release(); CHECK(touch_ui_paused()); /* One action, committed on release. */
    hold(20,LCD_HEIGHT-3); release(); CHECK(!touch_ui_paused());
    hold(LCD_WIDTH/2,LCD_HEIGHT-3); CHECK(touch_ui_cr_newline());
    release(); CHECK(!touch_ui_cr_newline());
    hold(LCD_WIDTH/2,LCD_HEIGHT-3); release(); CHECK(touch_ui_cr_newline());
    /* Vertical drags can start anywhere, including all footer controls. */
    gesture_sample(100,100,true); gesture_sample(100,132,true);
    CHECK(touch_ui_paused() && touch_ui_take_scroll()==4 && touch_ui_take_scroll()==0);
    gesture_sample(100,108,true); CHECK(touch_ui_take_scroll()==-3);
    release(); CHECK(touch_ui_paused());
    hold(20,LCD_HEIGHT-3); release(); CHECK(!touch_ui_paused());
    const unsigned starts[]={20,LCD_WIDTH/2,LCD_WIDTH-20};
    for(unsigned i=0;i<3;++i){
        gesture_sample(starts[i],LCD_HEIGHT-3,true);
        gesture_sample(starts[i],LCD_HEIGHT-43,true);
        CHECK(touch_ui_take_scroll()==-5 && touch_ui_paused());
        release(); CHECK(!touch_ui_visible() && touch_ui_cr_newline() && touch_ui_cal_progress()==0);
        hold(20,LCD_HEIGHT-3); release(); CHECK(!touch_ui_paused());
    }
    /* An invalid ADC reading with IRQ LOW keeps one contact, even >80 ms. */
    hold(LCD_WIDTH/2,LCD_HEIGHT-3);
    xpt2046_sample_t contact_only={.contact=true};
    for(unsigned i=0;i<10;++i){touch_ui_update(&contact_only,now_ms);now_ms+=20;}
    CHECK(touch_ui_cr_newline());release();CHECK(!touch_ui_cr_newline());
    hold(LCD_WIDTH/2,LCD_HEIGHT-3);release();CHECK(touch_ui_cr_newline());
    gesture_sample(20,LCD_HEIGHT-3,true);gesture_sample(70,LCD_HEIGHT-3,true);
    release();CHECK(!touch_ui_paused() && touch_ui_take_scroll()==0); /* Horizontal cancels tap. */
    /* Entering CAL from a held press elsewhere cannot arm it. */
    release(); hold(20,100); hold(LCD_WIDTH-20,LCD_HEIGHT-3);
    CHECK(touch_ui_cal_progress() == 0 && !touch_ui_visible());
    release(); hold(LCD_WIDTH-20,LCD_HEIGHT-3); CHECK(touch_ui_cal_progress() > 0);
    hold(20,100); CHECK(touch_ui_cal_progress() == 0 && !touch_ui_visible());
    hold(LCD_WIDTH-20,LCD_HEIGHT-3); CHECK(touch_ui_cal_progress() == 0);
    /* Elapsed time without observations is not evidence of a continuous hold. */
    release(); hold(LCD_WIDTH-20,LCD_HEIGHT-3);
    now_ms += 2100; hold(LCD_WIDTH-20,LCD_HEIGHT-3);
    CHECK(!touch_ui_visible() && touch_ui_cal_progress() < 50);
    /* Two separate 1.2 second holds must not combine into CAL. */
    release();
    for (unsigned i=0;i<60;++i) { gesture_sample(LCD_WIDTH-20,LCD_HEIGHT-3,true); }
    release();
    for (unsigned i=0;i<60;++i) { gesture_sample(LCD_WIDTH-20,LCD_HEIGHT-3,true); }
    CHECK(!touch_ui_visible());
    /* Real two-second hold: periodic missing samples and modest inward jitter
     * after arming must preserve the timer, with visible progress. */
    release(); gesture_sample(LCD_WIDTH-20,LCD_HEIGHT-3,true);
    for (unsigned i=0;i<105 && !touch_ui_visible();++i) {
        if (i % 3 == 1) { gesture_sample(0,0,false); }
        else if (i % 4 == 0) { gesture_sample(LCD_WIDTH*2/3-5,LCD_HEIGHT-20,true); }
        else { gesture_sample(LCD_WIDTH-20,LCD_HEIGHT-3,true); }
        if (i == 49) { CHECK(!touch_ui_visible() && touch_ui_cal_progress() >= 40 && touch_ui_cal_progress() <= 50); }
    }
    CHECK(touch_ui_visible() && touch_ui_cal_progress() == 0);
    CHECK(writes == 1 && commits == 1); /* Opening CAL never erases/writes NVS. */
    stored[20] ^= 1; /* Keep the magic/header intact; exercise the CRC check. */
    touch_ui_init(true); CHECK(touch_ui_visible()); /* Invalid saved data is ignored. */
    stored[20] ^= 1;
    uint32_t wrong_rotation = (RETRO_TFT_ROTATION + 2) & 3;
    memcpy(stored + 8, &wrong_rotation, sizeof(wrong_rotation));
    uint32_t crc = esp_rom_crc32_le(0, stored, (uint32_t)stored_size - 4);
    memcpy(stored + stored_size - 4, &crc, sizeof(crc));
    touch_ui_init(true); CHECK(touch_ui_visible()); /* Old orientation cannot be reused. */

    /* Reproduce the measured ~40 Hz cadence: one rejected poll in three
     * caused the old >=50 ms rule to restart before reaching eight samples. */
    noisy_calibration(25, false);
    noisy_calibration(30, false);
    noisy_calibration(25, true); /* Two missing polls: 75 ms between good ones. */

    /* Real releases, a long polling gap and an accidental remote touch must
     * discard the old sum. Seven subsequent good samples still cannot capture. */
    stored_size = 0; unsigned before = writes;
    touch_ui_init(true); release();
    short_hold(24,24); release(); short_hold(24,24); release();
    short_hold(24,24); now_ms += 80; short_hold(24,24);
    gesture_sample(220,100,true);
    for (unsigned i = 0; i < 7; ++i) { gesture_sample(24,24,true); }
    five(); release(); hold(LCD_WIDTH/3,LCD_HEIGHT*2/3);
    CHECK(writes == before + 1 && commits == writes);
    puts("PASS: affine/verification/NVS guards; calibration at 25/30 ms with missing polls; release/gap/moved-contact sum reset; debounced single-action taps; CAL missing samples/jitter/progress, real release/drag/gap cancellation; render bounds");
    return 0;
}
