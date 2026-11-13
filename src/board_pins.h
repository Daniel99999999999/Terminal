#pragma once

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
#define TFT_DC_GPIO 15
#define TFT_CS_GPIO 5
#define TFT_RD_GPIO 0
#define TFT_RST_GPIO 4
