#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_HCI_RESET       0x0C03
#define BT_HCI_READ_SCAN   0x0C19
#define BT_HCI_WRITE_SCAN  0x0C1A
#define BT_HCI_SCAN_PENDING 16

typedef enum { BT_HCI_TX, BT_HCI_COMPLETE, BT_HCI_STATUS } bt_hci_kind_t;
typedef struct {
    uint16_t opcode;
    bt_hci_kind_t kind;
    uint8_t value, status;
    bool has_value;
} bt_hci_scan_event_t;

/* Only sanitized scan/reset metadata leaves the H4 parser. No packet pointers. */
bool bt_hci_scan_parse(bool from_controller, const uint8_t *data, size_t len,
                       bt_hci_scan_event_t *event);

typedef struct {
    uint8_t pending[BT_HCI_SCAN_PENDING];
    unsigned head, count;
    bool trace_complete, confirmed;
    uint8_t value;
} bt_hci_scan_state_t;

void bt_hci_scan_init(bt_hci_scan_state_t *state);
void bt_hci_scan_lost(bt_hci_scan_state_t *state);
/* Returns true only for a WRITE completion matched to its FIFO request. */
bool bt_hci_scan_apply(bt_hci_scan_state_t *state, const bt_hci_scan_event_t *event,
                       uint8_t *requested);
