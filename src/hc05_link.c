#include "hc05_link.h"

static void next_generation(hc05_link_t *l)
{
    if (++l->generation == 0) { l->generation = 1; }
}

void hc05_link_init(hc05_link_t *l, bool has_state)
{
    *l = (hc05_link_t){ .generation = 1, .has_state = has_state };
}

void hc05_link_sample(hc05_link_t *l, bool high, uint32_t now)
{
    if (!l->has_state) { return; }
    if (!high) {
        l->high_pending = false;
        if (l->connected) { l->connected = false; if (!l->at) { next_generation(l); } }
    } else if (!l->connected) {
        if (!l->high_pending) { l->high_pending = true; l->high_since = now; }
        if ((uint32_t)(now - l->high_since) >= 50) {
            l->connected = true; if (!l->at) { next_generation(l); }
        }
    }
}

void hc05_link_at(hc05_link_t *l, bool at)
{
    if (l->at != at) { l->at = at; next_generation(l); }
}

hc05_status_t hc05_link_status(const hc05_link_t *l)
{
    return l->at ? HC05_AT : !l->has_state ? HC05_UNKNOWN :
           l->connected ? HC05_CONNECTED : HC05_DISCONNECTED;
}

bool hc05_link_current(const hc05_link_t *l, uint32_t generation, bool at_packet)
{
    return generation == l->generation &&
           (at_packet ? l->at : !l->at && (!l->has_state || l->connected));
}
