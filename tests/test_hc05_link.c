#include <stdio.h>
#include <stdint.h>
#include "hc05_link.h"
#include "board_pins.h"

#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); return 1; } } while (0)

int main(void)
{
    hc05_link_t l;
    hc05_link_init(&l, false);
    CHECK(hc05_link_status(&l) == HC05_UNKNOWN);
    CHECK(hc05_link_current(&l, l.generation, false));
    CHECK(!hc05_link_current(&l, l.generation, true));
    hc05_link_sample(&l, true, 500);
    CHECK(hc05_link_status(&l) == HC05_UNKNOWN); /* No STATE, never fake connected. */

    hc05_link_init(&l, true);
    CHECK(hc05_link_status(&l) == HC05_DISCONNECTED);
    CHECK(!hc05_link_current(&l, l.generation, false));
    hc05_link_sample(&l, true, 100);
    hc05_link_sample(&l, true, 149);
    CHECK(!l.connected);
    uint32_t old = l.generation;
    hc05_link_sample(&l, true, 150);
    CHECK(l.connected && l.generation != old);
    CHECK(hc05_link_current(&l, l.generation, false));
    CHECK(!hc05_link_current(&l, old, false));

    old = l.generation;
    hc05_link_sample(&l, false, 151);
    CHECK(!l.connected && l.generation != old); /* Immediate falling edge. */
    CHECK(!hc05_link_current(&l, old, false));
    hc05_link_sample(&l, true, 200);
    hc05_link_sample(&l, false, 240);
    hc05_link_sample(&l, true, 245);
    hc05_link_sample(&l, true, 294);
    CHECK(!l.connected);
    hc05_link_sample(&l, true, 295);
    CHECK(l.connected);

    hc05_link_init(&l, true);
    hc05_link_sample(&l, true, UINT32_MAX - 30);
    hc05_link_sample(&l, true, 18);
    CHECK(!l.connected);
    hc05_link_sample(&l, true, 19);
    CHECK(l.connected); /* Millisecond clock wrap. */
    l.generation = UINT32_MAX;
    hc05_link_sample(&l, false, 20);
    CHECK(l.generation == 1);

    hc05_link_init(&l, false);
    old = l.generation;
    hc05_link_at(&l, true);
    CHECK(hc05_link_status(&l) == HC05_AT && l.generation != old);
    CHECK(!hc05_link_current(&l, old, false));
    CHECK(!hc05_link_current(&l, l.generation, false));
    CHECK(hc05_link_current(&l, l.generation, true));
    old = l.generation;
    hc05_link_at(&l, true);
    CHECK(l.generation == old); /* Repeated mode request is idempotent. */
    hc05_link_at(&l, false);
    CHECK(!hc05_link_current(&l, old, true));
    CHECK(hc05_link_current(&l, l.generation, false));

    hc05_link_init(&l, true);
    hc05_link_sample(&l, true, 0);
    hc05_link_sample(&l, true, 50);
    hc05_link_at(&l, true);
    old = l.generation;
    hc05_link_sample(&l, false, 51);
    CHECK(l.generation == old && !l.connected);
    CHECK(hc05_link_current(&l, old, true)); /* BT disconnect must not cancel AT replies. */
    hc05_link_at(&l, false);
    CHECK(hc05_link_status(&l) == HC05_DISCONNECTED);
    CHECK(!hc05_link_current(&l, l.generation, false));

    const unsigned outputs[] = {TFT_D0_GPIO,TFT_D1_GPIO,TFT_D2_GPIO,TFT_D3_GPIO,
        TFT_D4_GPIO,TFT_D5_GPIO,TFT_D6_GPIO,TFT_D7_GPIO,TFT_WR_GPIO,TFT_DC_GPIO,
        TFT_CS_GPIO,TFT_RD_GPIO,TFT_RST_GPIO,HC05_UART_TX_GPIO,HC05_KEY_GPIO,
        TARGET_UART_TX_GPIO,TARGET_UART_RX_GPIO,1,2,3};
    uint64_t used = 0;
    for (unsigned i = 0; i < sizeof(outputs)/sizeof(outputs[0]); ++i) {
        unsigned p = outputs[i];
        CHECK(p <= 33 && p != 16 && p != 17 && !(p >= 6 && p <= 11));
        CHECK(!(used & (1ULL << p))); used |= 1ULL << p;
    }
    CHECK(HC05_UART_RX_GPIO >= 34 && HC05_STATE_GPIO >= 34 && HC05_UART_RX_GPIO != HC05_STATE_GPIO);
    puts("PASS: HC-05 unknown/STATE/debounce/session/AT/wrap policies and GPIO reservations");
    return 0;
}
