#pragma once
#include "display_profile.h"

/* ESP32-side plan only. LCD voltages/HC245 routing must be verified separately.
 * GPIO16/17 are PSRAM; GPIO6..11 flash; UART0 and GPIO2 remain reserved. */
#define TARGET_UART_TX_GPIO 25
#define TARGET_UART_RX_GPIO 26
#define HC05_UART_TX_GPIO   32
#define HC05_UART_RX_GPIO   34
#define HC05_STATE_GPIO     35
#define HC05_KEY_GPIO       12 /* Conditional strap: disabled by default. */
#define TFT_D0_GPIO 13
#define TFT_D1_GPIO 14
#define TFT_D2_GPIO 18
#define TFT_D3_GPIO 19
#define TFT_D4_GPIO 21
#define TFT_D5_GPIO 22
#define TFT_D6_GPIO 23
#define TFT_D7_GPIO 27
#define TFT_WR_GPIO 33
#define TFT_RD_GPIO 0

/* SPI TFT uses SPI3, XPT2046 uses SPI2: independent MISO and chip selects.
 * These replace (rather than extend) the I80 wiring. GPIO36/39 need external
 * pull-ups where required; they have no internal pulls. */
#define TFT_SPI_SCLK_GPIO 18
#define TFT_SPI_MOSI_GPIO 23
#define TFT_SPI_MISO_GPIO 19
#define TFT_BACKLIGHT_EN_GPIO 4 /* External driver EN only, optional. */
#define TOUCH_SPI_SCLK_GPIO 14
#define TOUCH_SPI_MOSI_GPIO 13
#define TOUCH_SPI_MISO_GPIO 39
#define TOUCH_CS_GPIO 33
#define TOUCH_IRQ_GPIO 36

#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
#define TFT_DC_GPIO 22
#define TFT_CS_GPIO 21
#define TFT_RST_GPIO 27
#else
#define TFT_DC_GPIO 15
#define TFT_CS_GPIO 5
#define TFT_RST_GPIO 4
#endif
