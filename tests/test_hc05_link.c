#include <stdio.h>
#include <stdint.h>
#include "hc05_link.h"

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

    /* Pin ownership is tested independently for every display profile. */
    puts("PASS: HC-05 unknown/STATE/debounce/session/AT/wrap policies");
    return 0;
}
