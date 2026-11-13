#include "bt_hci_scan.h"

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool selected(uint16_t opcode)
{
    return opcode == BT_HCI_RESET || opcode == BT_HCI_READ_SCAN ||
           opcode == BT_HCI_WRITE_SCAN;
}

bool bt_hci_scan_parse(bool from_controller, const uint8_t *data, size_t len,
                       bt_hci_scan_event_t *event)
{
    if (!data || !event) {
        return false;
    }
    bt_hci_scan_event_t result = {0};
    if (!from_controller) {
        if (len < 4 || data[0] != 0x01 || len != (size_t)data[3] + 4) {
            return false;
        }
        result.opcode = le16(data + 1);
        if (!selected(result.opcode)) {
            return false;
        }
        result.kind = BT_HCI_TX;
        result.has_value = result.opcode == BT_HCI_WRITE_SCAN;
        if (data[3] != (result.has_value ? 1 : 0)) {
            return false;
        }
        if (result.has_value) {
            result.value = data[4];
        }
    } else {
        if (len < 7 || data[0] != 0x04 || len != (size_t)data[2] + 3) {
            return false;
        }
        if (data[1] == 0x0E) { /* Command Complete: credits, opcode, status, ... */
            result.kind = BT_HCI_COMPLETE;
            result.opcode = le16(data + 4);
            result.status = data[6];
            if (result.opcode == BT_HCI_READ_SCAN) {
                /* On error some controllers return only Status, others also
                 * retain an undefined Scan_Enable byte. Never treat it as data. */
                if ((len != 7 && len != 8) || (result.status == 0 && len != 8)) {
                    return false;
                }
                result.has_value = result.status == 0;
                if (result.has_value) {
                    result.value = data[7];
                }
            } else if (len != 7) {
                return false;
            }
        } else if (data[1] == 0x0F && len == 7) { /* Command Status */
            result.kind = BT_HCI_STATUS;
            result.opcode = le16(data + 5);
            result.status = data[3];
        } else {
            return false;
        }
        if (!selected(result.opcode)) {
            return false;
        }
    }
    *event = result;
    return true;
}

void bt_hci_scan_init(bt_hci_scan_state_t *state)
{
    *state = (bt_hci_scan_state_t){ .trace_complete = true };
}

void bt_hci_scan_lost(bt_hci_scan_state_t *state)
{
    state->trace_complete = false;
    state->confirmed = false;
    state->head = state->count = 0;
}

bool bt_hci_scan_apply(bt_hci_scan_state_t *state, const bt_hci_scan_event_t *event,
                       uint8_t *requested)
{
    if (event->opcode == BT_HCI_RESET) {
        state->head = state->count = 0;
        state->confirmed = false;
    } else if (event->opcode == BT_HCI_WRITE_SCAN) {
        if (event->kind == BT_HCI_TX) {
            state->confirmed = false;
            if (state->count == BT_HCI_SCAN_PENDING) {
                bt_hci_scan_lost(state);
            } else if (state->trace_complete) {
                state->pending[(state->head + state->count) % BT_HCI_SCAN_PENDING] = event->value;
                state->count++;
            }
        } else if (event->kind == BT_HCI_COMPLETE) {
            if (!state->count || !state->trace_complete) {
                bt_hci_scan_lost(state);
                return false;
            }
            *requested = state->pending[state->head];
            state->head = (state->head + 1) % BT_HCI_SCAN_PENDING;
            state->count--;
            state->confirmed = event->status == 0 && state->count == 0;
            if (event->status == 0) {
                state->value = *requested;
            }
            return true;
        } else if (event->status != 0) {
            /* These synchronous commands should complete with Command Complete.
             * An unexpected error makes the pairing of observations uncertain. */
            bt_hci_scan_lost(state);
        }
    } else if (event->opcode == BT_HCI_READ_SCAN && event->kind == BT_HCI_COMPLETE) {
        state->confirmed = state->trace_complete && state->count == 0 &&
                           event->status == 0 && event->has_value;
        if (state->confirmed) {
            state->value = event->value;
        }
    }
    return false;
}
