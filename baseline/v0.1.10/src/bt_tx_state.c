#include "bt_tx_state.h"

void bt_tx_server_started(bt_tx_state_t *state)
{
    /* Only after initial START, or UNINIT -> INIT -> START recovery barrier. */
    uint64_t generation = state->session.generation + 1;
    *state = (bt_tx_state_t){ .session.generation = generation, .ready = true };
}

bool bt_tx_open(bt_tx_state_t *state, uint32_t handle)
{
    if (!state->ready || state->fault || state->session.handle || !handle) {
        return false;
    }
    state->session.generation++;
    state->session.handle = handle;
    state->congested = false;
    return true;
}

bool bt_tx_close(bt_tx_state_t *state, uint32_t handle)
{
    if (!handle || handle != state->session.handle) {
        return false;
    }
    state->session.handle = 0;
    state->session.generation++;
    /* A late WRITE with handle=0 cannot be attributed to a later session.
     * Drain the SPP profile through UNINIT before accepting another client. */
    if (state->pending) {
        bt_tx_fail(state);
    }
    state->congested = false;
    return true;
}

bool bt_tx_is_current(const bt_tx_state_t *state, bt_session_t session)
{
    return state->ready && !state->fault && session.handle != 0 &&
           session.handle == state->session.handle &&
           session.generation == state->session.generation;
}

bt_tx_begin_t bt_tx_begin(bt_tx_state_t *state, bt_session_t session, uint16_t len)
{
    if (!bt_tx_is_current(state, session) || !len) {
        return BT_TX_STALE;
    }
    if (state->pending || state->congested) {
        return BT_TX_BLOCKED;
    }
    state->pending = true;
    state->write_done = false;
    state->pending_handle = session.handle;
    state->pending_len = len;
    return BT_TX_STARTED;
}

void bt_tx_written(bt_tx_state_t *state, uint32_t handle, bool success,
                   int len, bool congested)
{
    if (!state->pending || state->fault ||
        (handle != state->pending_handle && !(handle == 0 && !success))) {
        return;
    }
    if (!success || len != state->pending_len) {
        bt_tx_fail(state);
        return;
    }
    state->write_done = true;
    state->congested = congested;
}

void bt_tx_congestion(bt_tx_state_t *state, uint32_t handle, bool congested)
{
    if (!state->fault && handle && handle == state->session.handle) {
        state->congested = congested;
    }
}

bt_tx_result_t bt_tx_result(const bt_tx_state_t *state)
{
    if (state->fault) {
        return BT_TX_FAULT;
    }
    return state->pending && state->write_done && !state->congested
               ? BT_TX_DONE : BT_TX_WAIT;
}

void bt_tx_finish(bt_tx_state_t *state)
{
    if (bt_tx_result(state) == BT_TX_DONE) {
        state->pending = false;
        state->write_done = false;
        state->pending_handle = 0;
    }
}

void bt_tx_fail(bt_tx_state_t *state)
{
    if (!state->fault) {
        state->session.generation++;
    }
    state->fault = true;
    state->ready = false;
    state->session.handle = 0;
}
