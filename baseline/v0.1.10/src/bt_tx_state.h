#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Pure state machine. The caller serializes every access (including snapshots).
 * No Bluetooth/RTOS call may be made while holding that caller's spinlock. */
typedef struct {
    uint64_t generation;
    uint32_t handle;
} bt_session_t;

typedef struct {
    bt_session_t session;
    uint32_t pending_handle;
    uint16_t pending_len;
    bool ready;
    bool fault;
    bool pending;
    bool write_done;
    bool congested;
} bt_tx_state_t;

typedef enum {
    BT_TX_STALE,
    BT_TX_BLOCKED,
    BT_TX_STARTED,
} bt_tx_begin_t;

typedef enum {
    BT_TX_WAIT,
    BT_TX_DONE,
    BT_TX_FAULT,
} bt_tx_result_t;

void bt_tx_server_started(bt_tx_state_t *state);
bool bt_tx_open(bt_tx_state_t *state, uint32_t handle);
bool bt_tx_close(bt_tx_state_t *state, uint32_t handle);
bool bt_tx_is_current(const bt_tx_state_t *state, bt_session_t session);
bt_tx_begin_t bt_tx_begin(bt_tx_state_t *state, bt_session_t session, uint16_t len);
void bt_tx_written(bt_tx_state_t *state, uint32_t handle, bool success,
                   int len, bool congested);
void bt_tx_congestion(bt_tx_state_t *state, uint32_t handle, bool congested);
bt_tx_result_t bt_tx_result(const bt_tx_state_t *state);
void bt_tx_finish(bt_tx_state_t *state);
void bt_tx_fail(bt_tx_state_t *state);
