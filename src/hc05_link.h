#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { HC05_UNKNOWN, HC05_DISCONNECTED, HC05_CONNECTED, HC05_AT } hc05_status_t;
typedef struct {
    uint32_t generation, high_since;
    bool has_state, connected, high_pending, at;
} hc05_link_t;

/* Pure policy; caller serializes access. Rising STATE is debounced 50 ms;
 * falling STATE blocks immediately. Unknown status still permits forwarding. */
void hc05_link_init(hc05_link_t *link, bool has_state);
void hc05_link_sample(hc05_link_t *link, bool high, uint32_t now_ms);
void hc05_link_at(hc05_link_t *link, bool at);
hc05_status_t hc05_link_status(const hc05_link_t *link);
bool hc05_link_current(const hc05_link_t *link, uint32_t generation, bool at_packet);
