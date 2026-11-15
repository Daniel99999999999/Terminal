#pragma once

/* Select ONE physical module here, or override with -DRETRO_TFT_PROFILE=...
 * NONE keeps all display/touch GPIOs untouched. Never wire both pin plans. */
#define RETRO_TFT_NONE           0
#define RETRO_TFT_ILI9341_I80    1
#define RETRO_TFT_ILI9341_SPI    2
#ifndef RETRO_TFT_PROFILE
#define RETRO_TFT_PROFILE RETRO_TFT_ILI9341_SPI
#endif

#ifndef RETRO_TFT_ROTATION
#define RETRO_TFT_ROTATION 1
#endif
#ifndef RETRO_TFT_BGR
#define RETRO_TFT_BGR 1
#endif
#ifndef RETRO_TFT_INVERT
#define RETRO_TFT_INVERT 0
#endif
#ifndef RETRO_TFT_COLOR_TEST
#define RETRO_TFT_COLOR_TEST 0 /* Optional diagnostic, after the welcome screen. */
#endif
#ifndef RETRO_TFT_I80_HZ
#define RETRO_TFT_I80_HZ 1000000
#endif
#ifndef RETRO_TFT_SPI_HZ
#define RETRO_TFT_SPI_HZ 20000000
#endif
#ifndef RETRO_TOUCH_ENABLE
#define RETRO_TOUCH_ENABLE 0 /* Enable explicitly after wiring XPT2046. */
#endif
#ifndef RETRO_TOUCH_SPI_HZ
#define RETRO_TOUCH_SPI_HZ 1000000
#endif
/* GPIO4 drives an external backlight driver's EN, never the LED load itself.
 * Leave OFF until that driver, polarity and module LED supply are verified. */
#ifndef RETRO_TFT_BACKLIGHT_CONTROL
#define RETRO_TFT_BACKLIGHT_CONTROL 0
#endif
#ifndef RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL
#define RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL 1
#endif

#if RETRO_TFT_PROFILE < RETRO_TFT_NONE || RETRO_TFT_PROFILE > RETRO_TFT_ILI9341_SPI
#error "Unknown RETRO_TFT_PROFILE"
#endif
#if RETRO_TFT_ROTATION < 0 || RETRO_TFT_ROTATION > 3
#error "RETRO_TFT_ROTATION must be 0..3"
#endif
#if (RETRO_TOUCH_ENABLE || RETRO_TFT_BACKLIGHT_CONTROL) && RETRO_TFT_PROFILE != RETRO_TFT_ILI9341_SPI
#error "XPT2046/backlight GPIO control require the SPI pin plan"
#endif
#if RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL != 0 && RETRO_TFT_BACKLIGHT_ACTIVE_LEVEL != 1
#error "Backlight active level must be 0 or 1"
#endif
#if RETRO_TFT_SPI_HZ < 100000 || RETRO_TFT_SPI_HZ > 40000000 || RETRO_TOUCH_SPI_HZ < 100000 || RETRO_TOUCH_SPI_HZ > 2000000
#error "SPI clock outside supported profile range"
#endif
#if RETRO_TFT_I80_HZ < 100000 || RETRO_TFT_I80_HZ > 10000000
#error "I80 clock outside supported profile range"
#endif
