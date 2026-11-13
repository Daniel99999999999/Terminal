/* Host regression tests: exercise the production state machine, no RF mocks. */
#include <stdio.h>
#include "bt_tx_state.h"

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        return 1; \
    } \
} while (0)

static bt_tx_state_t connected(void)
{
    bt_tx_state_t state = {0};
    bt_tx_server_started(&state);
    (void)bt_tx_open(&state, 17);
    return state;
}

static int normal_write(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 3) == BT_TX_STARTED);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    bt_tx_written(&s, 17, true, 3, false);
    CHECK(bt_tx_result(&s) == BT_TX_DONE);
    bt_tx_finish(&s);
    CHECK(!s.pending);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    return 0;
}

static int stale_dequeued_packet(void)
{
    bt_tx_state_t s = connected();
    bt_session_t dequeued = s.session;
    CHECK(bt_tx_close(&s, 17));
    CHECK(bt_tx_open(&s, 18));
    CHECK(bt_tx_begin(&s, dequeued, 128) == BT_TX_STALE);
    CHECK(!s.pending);
    return 0;
}

static int reused_handle(void)
{
    bt_tx_state_t s = connected();
    bt_session_t old = s.session;
    CHECK(bt_tx_close(&s, 17));
    CHECK(bt_tx_open(&s, 17));
    CHECK(!bt_tx_is_current(&s, old));
    CHECK(bt_tx_begin(&s, old, 1) == BT_TX_STALE);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    return 0;
}

static int anonymous_error(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 128) == BT_TX_STARTED);
    bt_tx_written(&s, 0, false, -1, false);
    CHECK(bt_tx_result(&s) == BT_TX_FAULT);
    CHECK(!bt_tx_open(&s, 18));
    return 0;
}

static int congestion_is_not_completion(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    bt_tx_congestion(&s, 17, true);
    bt_tx_congestion(&s, 17, false);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    bt_tx_written(&s, 17, true, 1, false);
    CHECK(bt_tx_result(&s) == BT_TX_DONE);
    return 0;
}

static int wait_for_congestion_clear(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    bt_tx_written(&s, 17, true, 1, true);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    bt_tx_finish(&s);
    CHECK(s.pending);
    bt_tx_congestion(&s, 17, false);
    CHECK(bt_tx_result(&s) == BT_TX_DONE);
    return 0;
}

static int idle_congestion_blocks_write(void)
{
    bt_tx_state_t s = connected();
    bt_tx_congestion(&s, 17, true);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_BLOCKED);
    bt_tx_congestion(&s, 17, false);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    return 0;
}

static int close_after_reservation(void)
{
    bt_tx_state_t s = connected();
    bt_session_t reserved = s.session;
    CHECK(bt_tx_begin(&s, reserved, 3) == BT_TX_STARTED);
    CHECK(bt_tx_close(&s, 17));
    CHECK(bt_tx_result(&s) == BT_TX_FAULT);
    CHECK(!bt_tx_open(&s, 18));
    bt_tx_written(&s, 0, false, -1, false);
    bt_tx_congestion(&s, 17, false);
    CHECK(bt_tx_result(&s) == BT_TX_FAULT);
    /* Caller confirms UNINIT -> INIT -> START before this reset. */
    bt_tx_server_started(&s);
    CHECK(bt_tx_open(&s, 17));
    CHECK(bt_tx_begin(&s, reserved, 3) == BT_TX_STALE);
    CHECK(bt_tx_begin(&s, s.session, 3) == BT_TX_STARTED);
    bt_tx_written(&s, 17, true, 3, false);
    CHECK(bt_tx_result(&s) == BT_TX_DONE);
    return 0;
}

static int unrelated_events(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    bt_tx_written(&s, 99, false, -1, false);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    bt_tx_written(&s, 17, true, 1, true);
    bt_tx_congestion(&s, 99, false);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    CHECK(!bt_tx_close(&s, 99));
    return 0;
}

static int timeout_or_submit_failure(void)
{
    bt_tx_state_t s = connected();
    bt_session_t old = s.session;
    CHECK(bt_tx_begin(&s, old, 1) == BT_TX_STARTED);
    bt_tx_fail(&s); /* Called by the task on its finite timeout/API failure. */
    CHECK(bt_tx_result(&s) == BT_TX_FAULT);
    CHECK(bt_tx_begin(&s, old, 1) == BT_TX_STALE);
    uint64_t generation = s.session.generation;
    bt_tx_fail(&s);
    CHECK(s.session.generation == generation); /* No repeated UART flushes. */
    bt_tx_server_started(&s);
    CHECK(bt_tx_open(&s, 18));
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    return 0;
}

static int partial_write_is_failure(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 128) == BT_TX_STARTED);
    bt_tx_written(&s, 17, true, 127, false);
    CHECK(bt_tx_result(&s) == BT_TX_FAULT);
    return 0;
}

static int only_one_outstanding_write(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_BLOCKED);
    return 0;
}

static int idle_reconnect_needs_no_restart(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_close(&s, 17));
    CHECK(!s.fault);
    CHECK(bt_tx_open(&s, 18));
    return 0;
}

static int error_without_pending_is_ignored(void)
{
    bt_tx_state_t s = connected();
    bt_tx_written(&s, 0, false, -1, false);
    CHECK(!s.fault);
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    return 0;
}

static int successful_zero_handle_is_not_completion(void)
{
    bt_tx_state_t s = connected();
    CHECK(bt_tx_begin(&s, s.session, 1) == BT_TX_STARTED);
    bt_tx_written(&s, 0, true, 1, false);
    CHECK(bt_tx_result(&s) == BT_TX_WAIT);
    return 0;
}

static int reject_second_client(void)
{
    bt_tx_state_t s = connected();
    bt_session_t first = s.session;
    CHECK(!bt_tx_open(&s, 18));
    CHECK(bt_tx_is_current(&s, first));
    return 0;
}

int main(void)
{
    int (*tests[])(void) = {
        normal_write, stale_dequeued_packet, reused_handle, anonymous_error,
        congestion_is_not_completion, wait_for_congestion_clear,
        idle_congestion_blocks_write, close_after_reservation, unrelated_events,
        timeout_or_submit_failure, partial_write_is_failure,
        only_one_outstanding_write, idle_reconnect_needs_no_restart,
        error_without_pending_is_ignored, successful_zero_handle_is_not_completion,
        reject_second_client,
    };
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i]()) {
            return 1;
        }
    }
    puts("PASS: 16 Bluetooth TX/session regression scenarios");
    return 0;
}
