#include <stdio.h>
#include <stdint.h>
#include "board_pins.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); return 1; } } while (0)

int main(void)
{
    const unsigned pins[] = {1, 2, 3, TARGET_UART_TX_GPIO, TARGET_UART_RX_GPIO,
        HC05_UART_TX_GPIO, HC05_UART_RX_GPIO, HC05_STATE_GPIO, HC05_KEY_GPIO,
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_I80
        TFT_D0_GPIO,TFT_D1_GPIO,TFT_D2_GPIO,TFT_D3_GPIO,TFT_D4_GPIO,TFT_D5_GPIO,
        TFT_D6_GPIO,TFT_D7_GPIO,TFT_WR_GPIO,TFT_RD_GPIO,TFT_DC_GPIO,TFT_CS_GPIO,TFT_RST_GPIO,
#elif RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
        TFT_SPI_SCLK_GPIO,TFT_SPI_MOSI_GPIO,TFT_SPI_MISO_GPIO,TFT_DC_GPIO,TFT_CS_GPIO,TFT_RST_GPIO,
        TFT_BACKLIGHT_EN_GPIO,TOUCH_SPI_SCLK_GPIO,TOUCH_SPI_MOSI_GPIO,TOUCH_CS_GPIO,
        TOUCH_SPI_MISO_GPIO,TOUCH_IRQ_GPIO,
#endif
    };
    uint64_t used = 0;
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        unsigned p = pins[i];
        CHECK(p < 40 && p != 16 && p != 17 && !(p >= 6 && p <= 11));
        CHECK(!(used & (1ULL << p))); used |= 1ULL << p;
    }
#if RETRO_TFT_PROFILE == RETRO_TFT_ILI9341_SPI
    const unsigned outputs[] = {TFT_SPI_SCLK_GPIO,TFT_SPI_MOSI_GPIO,TFT_DC_GPIO,
        TFT_CS_GPIO,TFT_RST_GPIO,TFT_BACKLIGHT_EN_GPIO,TOUCH_SPI_SCLK_GPIO,
        TOUCH_SPI_MOSI_GPIO,TOUCH_CS_GPIO};
    for (unsigned i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
        CHECK(outputs[i] <= 33);
        CHECK(outputs[i] != 0 && outputs[i] != 2 && outputs[i] != 5 &&
              outputs[i] != 12 && outputs[i] != 15); /* No new straps. */
    }
    CHECK(TOUCH_IRQ_GPIO >= 34 && TOUCH_SPI_MISO_GPIO >= 34);
    CHECK(TFT_SPI_MISO_GPIO != TOUCH_SPI_MISO_GPIO);
#else
    CHECK(!RETRO_TOUCH_ENABLE && !RETRO_TFT_BACKLIGHT_CONTROL);
#endif
    printf("PASS: profile %d GPIO ownership, PSRAM/UART/LED and pin capabilities\n", RETRO_TFT_PROFILE);
    return 0;
}
