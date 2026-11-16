#pragma once
#include "terminal.h"

/* Single screen-task owner. Bounded ring plus an immutable live-page snapshot
 * while paused; UART parsing continues. No dependency on PSRAM or RTOS. */
typedef struct {
    terminal_cell_t *lines, *snapshot;
    uint16_t cols, rows, capacity;
    uint64_t total, anchor, top;
    uint32_t revision;
    bool paused, primary;
} terminal_history_t;

bool terminal_history_init(terminal_history_t *h, terminal_t *t,
                           terminal_cell_t *lines, uint16_t capacity,
                           terminal_cell_t *snapshot);
void terminal_history_pause(terminal_history_t *h, const terminal_t *t, bool pause);
/* Positive rows reveal older logs (finger down); negative rows reveal newer. */
void terminal_history_move(terminal_history_t *h, const terminal_t *t, int rows);
void terminal_history_clamp(terminal_history_t *h);
unsigned terminal_history_offset(const terminal_history_t *h);
const terminal_cell_t *terminal_history_row(const terminal_history_t *h,
                                           const terminal_t *t, unsigned row);
