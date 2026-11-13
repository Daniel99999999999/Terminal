#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bt_hci_scan.h"

static const uint8_t write3[] = {1, 0x1A, 0x0C, 1, 3};
static const uint8_t complete[] = {4, 0x0E, 4, 1, 0x1A, 0x0C, 0};

static bt_hci_scan_event_t parse(bool rx, const uint8_t *data, size_t len)
{
    bt_hci_scan_event_t event;
    assert(bt_hci_scan_parse(rx, data, len, &event));
    return event;
}

static void write_command(void)
{
    uint8_t copy[sizeof(write3)];
    memcpy(copy, write3, sizeof(copy));
    bt_hci_scan_event_t event = parse(false, copy, sizeof(copy));
    assert(event.kind == BT_HCI_TX && event.opcode == BT_HCI_WRITE_SCAN);
    assert(event.has_value && event.value == 3);
    assert(memcmp(copy, write3, sizeof(copy)) == 0);
}

static void write_response(void)
{
    bt_hci_scan_event_t event = parse(true, complete, sizeof(complete));
    assert(event.kind == BT_HCI_COMPLETE && event.opcode == BT_HCI_WRITE_SCAN);
    assert(event.status == 0 && !event.has_value);
}

static void short_packets(void)
{
    bt_hci_scan_event_t event = {.opcode = 0xFFFF};
    for (size_t n = 0; n < sizeof(write3); ++n) {
        assert(!bt_hci_scan_parse(false, write3, n, &event));
    }
    for (size_t n = 0; n < sizeof(complete); ++n) {
        assert(!bt_hci_scan_parse(true, complete, n, &event));
    }
    assert(event.opcode == 0xFFFF);
    assert(!bt_hci_scan_parse(true, NULL, 7, &event));
    assert(!bt_hci_scan_parse(true, complete, sizeof(complete), NULL));
}

static void malformed_lengths(void)
{
    bt_hci_scan_event_t event;
    const uint8_t command[] = {1, 0x1A, 0x0C, 0};
    const uint8_t overlong[] = {1, 0x1A, 0x0C, 1, 3, 0};
    const uint8_t missing_read_value[] = {4, 0x0E, 4, 1, 0x19, 0x0C, 0};
    assert(!bt_hci_scan_parse(false, command, sizeof(command), &event));
    assert(!bt_hci_scan_parse(false, overlong, sizeof(overlong), &event));
    assert(!bt_hci_scan_parse(true, missing_read_value, sizeof(missing_read_value), &event));
}

static void unrelated_packets(void)
{
    bt_hci_scan_event_t event;
    const uint8_t key_reply[] = {1, 0x0B, 4, 0};
    const uint8_t key_event[] = {4, 0x18, 4, 1, 0x1A, 0x0C, 0};
    const uint8_t acl[] = {2, 0x0E, 4, 1, 0x1A, 0x0C, 0};
    assert(!bt_hci_scan_parse(false, key_reply, sizeof(key_reply), &event));
    assert(!bt_hci_scan_parse(true, key_event, sizeof(key_event), &event));
    assert(!bt_hci_scan_parse(true, acl, sizeof(acl), &event));
    assert(!bt_hci_scan_parse(true, write3, sizeof(write3), &event));
    assert(!bt_hci_scan_parse(false, complete, sizeof(complete), &event));
}

static void read_result(void)
{
    const uint8_t read[] = {1, 0x19, 0x0C, 0};
    const uint8_t result[] = {4, 0x0E, 5, 1, 0x19, 0x0C, 0, 3};
    bt_hci_scan_event_t event = parse(false, read, sizeof(read));
    assert(event.opcode == BT_HCI_READ_SCAN && !event.has_value);
    event = parse(true, result, sizeof(result));
    assert(event.has_value && event.value == 3 && event.status == 0);
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    uint8_t value;
    assert(!bt_hci_scan_apply(&state, &event, &value));
    assert(state.confirmed && state.value == 3);
}

static void failed_read(void)
{
    const uint8_t failed[] = {4, 0x0E, 4, 1, 0x19, 0x0C, 0x0C};
    bt_hci_scan_event_t event = parse(true, failed, sizeof(failed));
    assert(event.status == 0x0C && !event.has_value);
    const uint8_t failed_padded[] = {4, 0x0E, 5, 1, 0x19, 0x0C, 0x0C, 3};
    event = parse(true, failed_padded, sizeof(failed_padded));
    assert(event.status == 0x0C && !event.has_value);
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    state.confirmed = true;
    uint8_t value;
    assert(!bt_hci_scan_apply(&state, &event, &value));
    assert(!state.confirmed);
}

static void fifo_confirmation(void)
{
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    bt_hci_scan_event_t tx = parse(false, write3, sizeof(write3));
    bt_hci_scan_event_t rx = parse(true, complete, sizeof(complete));
    uint8_t value = 0xFF;
    for (unsigned repeat = 0; repeat < 40; ++repeat) {
        tx.value = 1;
        assert(!bt_hci_scan_apply(&state, &tx, &value));
        tx.value = 3;
        assert(!bt_hci_scan_apply(&state, &tx, &value));
        assert(!state.confirmed);
        assert(bt_hci_scan_apply(&state, &rx, &value) && value == 1);
        assert(!state.confirmed && state.count == 1);
        assert(bt_hci_scan_apply(&state, &rx, &value) && value == 3);
        assert(state.confirmed && state.value == 3 && state.count == 0);
    }
}

static void failed_write(void)
{
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    bt_hci_scan_event_t tx = parse(false, write3, sizeof(write3));
    bt_hci_scan_event_t rx = parse(true, complete, sizeof(complete));
    uint8_t value;
    bt_hci_scan_apply(&state, &tx, &value);
    rx.status = 0x0C;
    assert(bt_hci_scan_apply(&state, &rx, &value) && value == 3);
    assert(!state.confirmed && state.count == 0);
}

static void missing_request(void)
{
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    bt_hci_scan_event_t rx = parse(true, complete, sizeof(complete));
    uint8_t value;
    assert(!bt_hci_scan_apply(&state, &rx, &value));
    assert(!state.trace_complete && !state.confirmed);
}

static void queue_loss_and_overflow(void)
{
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    bt_hci_scan_event_t tx = parse(false, write3, sizeof(write3));
    bt_hci_scan_event_t rx = parse(true, complete, sizeof(complete));
    uint8_t value;
    for (unsigned i = 0; i <= BT_HCI_SCAN_PENDING; ++i) {
        bt_hci_scan_apply(&state, &tx, &value);
    }
    assert(!state.trace_complete && !state.confirmed);
    bt_hci_scan_init(&state);
    bt_hci_scan_apply(&state, &tx, &value);
    bt_hci_scan_lost(&state);
    assert(!bt_hci_scan_apply(&state, &rx, &value));
    assert(!state.confirmed && !state.trace_complete);
}

static void reset_invalidates(void)
{
    const uint8_t reset[] = {1, 3, 0x0C, 0};
    bt_hci_scan_event_t event = parse(false, reset, sizeof(reset));
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    state.confirmed = true;
    uint8_t value;
    bt_hci_scan_apply(&state, &event, &value);
    assert(!state.confirmed && state.count == 0);
    bt_hci_scan_lost(&state);
    bt_hci_scan_apply(&state, &event, &value);
    assert(!state.trace_complete); /* Lost trace is sticky until manual reboot. */
}

static void command_status(void)
{
    const uint8_t packet[] = {4, 0x0F, 4, 0x0C, 1, 0x1A, 0x0C};
    bt_hci_scan_event_t event = parse(true, packet, sizeof(packet));
    assert(event.kind == BT_HCI_STATUS && event.status == 0x0C);
    bt_hci_scan_state_t state;
    bt_hci_scan_init(&state);
    uint8_t value;
    bt_hci_scan_apply(&state, &event, &value);
    assert(!state.trace_complete && !state.confirmed);
}

int main(void)
{
    write_command(); write_response(); short_packets(); malformed_lengths();
    unrelated_packets(); read_result(); failed_read(); fifo_confirmation();
    failed_write(); missing_request(); queue_loss_and_overflow();
    reset_invalidates(); command_status();
    puts("PASS: 13 HCI parser/FIFO/confirmation scenarios");
    return 0;
}
