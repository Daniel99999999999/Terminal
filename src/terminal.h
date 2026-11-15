#pragma once

#include "ansi_parser.h"

#define TERMINAL_MAX_ROWS 64
#define TERMINAL_MAX_COLS 160
#define TERMINAL_DEFAULT_FG 0xffffffu
#define TERMINAL_DEFAULT_BG 0x000000u
enum { TERM_BOLD = 1, TERM_UNDERLINE = 2, TERM_INVERSE = 4, TERM_BLINK = 8 };

typedef struct {
    uint32_t codepoint;
    uint32_t fg; /* RGB888, low 24 bits; attributes in fg's high byte. */
    uint32_t bg;
} terminal_cell_t;

typedef struct {
    uint16_t x, y;
    uint32_t fg, bg;
    uint8_t flags;
    uint8_t g0, g1, selected;
    bool origin, wrap, pending_wrap;
} terminal_cursor_t;

typedef struct {
    terminal_cell_t *cells;
    uint16_t line[TERMINAL_MAX_ROWS];
    terminal_cursor_t cursor;
    terminal_cursor_t saved;
    uint16_t top, bottom;
} terminal_page_t;

/* One owner at a time; no I/O, heap allocation, locks or hardware access.
 * Both caller-owned buffers need rows*cols cells. Unicode scalar values use
 * one cell; full-width and combining-character layout is not implemented. */
typedef struct {
    ansi_parser_t parser;
    terminal_page_t page[2];
    uint16_t rows, cols;
    uint8_t active;
    bool cursor_visible, insert, newline;
    bool cr_newline, cr_pending; /* User display option; separate from VT LNM. */
    bool tabs[TERMINAL_MAX_COLS];
    uint32_t utf_value, utf_min;
    uint8_t utf_left;
    uint32_t revision, control_sequences;
} terminal_t;

bool terminal_init(terminal_t *t, uint16_t cols, uint16_t rows,
                   terminal_cell_t *primary, terminal_cell_t *alternate);
void terminal_reset(terminal_t *t);
/* Optional serial-text convention. CRLF still advances exactly one row.
 * Off preserves standard VT carriage-return/progress-line behavior. */
void terminal_set_cr_newline(terminal_t *t, bool enable);
void terminal_feed(terminal_t *t, const uint8_t *data, size_t length);
/* On a known transport gap, cancel partial UTF-8/ANSI and restore default attributes.
 * Retains visible cells; later incoming controls may rebuild the screen. */
void terminal_stream_gap(terminal_t *t);
const terminal_cell_t *terminal_cell(const terminal_t *t, uint16_t x, uint16_t y);
void terminal_cell_colors(const terminal_cell_t *cell, uint32_t *fg, uint32_t *bg);
