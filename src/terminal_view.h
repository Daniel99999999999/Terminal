#pragma once

#include "terminal.h"
#include "esp_err.h"

typedef struct {
    uint32_t revision, control_sequences, dropped_bytes, stream_gaps;
    uint16_t cols, rows, cursor_x, cursor_y;
    bool cursor_visible, alternate;
} terminal_view_info_t;

esp_err_t terminal_view_init(uint16_t cols, uint16_t rows);
/* Single producer: UART RX task. Copies data using a zero-wait queue. */
void terminal_view_feed(const uint8_t *data, size_t length);
void terminal_view_stream_gap(void);
/* Future renderer: copy one row and its metadata, or retry later if busy.
 * Never retain pointers to the mutable model and never do LCD I/O under lock. */
bool terminal_view_copy_row(uint16_t row, terminal_cell_t *dest, size_t capacity,
                            terminal_view_info_t *info);
void terminal_view_log_stats(void);
