#include "terminal_history.h"
#include <string.h>

static uint64_t oldest(const terminal_history_t *h)
{
    uint64_t first = h->total > h->capacity ? h->total - h->capacity : 0;
    return !h->primary || first > h->anchor ? h->anchor : first;
}

void terminal_history_clamp(terminal_history_t *h)
{
    if (h->paused && h->top < oldest(h)) { h->top = oldest(h); ++h->revision; }
}

static void append(void *ctx, const terminal_cell_t *cells)
{
    terminal_history_t *h = ctx;
    memcpy(h->lines + (h->total % h->capacity) * h->cols, cells, h->cols * sizeof(*cells));
    ++h->total;
    terminal_history_clamp(h);
}

static void clear(void *ctx)
{
    terminal_history_t *h = ctx;
    h->total = h->anchor = h->top = 0;
    /* A paused page remains immutable even if the remote terminal resets. */
    ++h->revision;
}

bool terminal_history_init(terminal_history_t *h, terminal_t *t,
                           terminal_cell_t *lines, uint16_t capacity,
                           terminal_cell_t *snapshot)
{
    if (!h || !t || !lines || !capacity || !snapshot) { return false; }
    *h = (terminal_history_t){ .lines = lines, .snapshot = snapshot,
        .cols = t->cols, .rows = t->rows, .capacity = capacity };
    t->history_line = append; t->history_clear = clear; t->history_ctx = h;
    return true;
}

void terminal_history_pause(terminal_history_t *h, const terminal_t *t, bool pause)
{
    if (h->paused == pause) { return; }
    h->paused = pause;
    if (pause) {
        h->anchor = h->top = h->total;
        h->primary = t->active == 0;
        for (unsigned y = 0; y < h->rows; ++y) {
            memcpy(h->snapshot + y * h->cols, terminal_cell(t, 0, (uint16_t)y), h->cols * sizeof(*h->snapshot));
        }
    }
    ++h->revision;
}

void terminal_history_move(terminal_history_t *h, const terminal_t *t, int rows)
{
    if (!rows) { return; }
    terminal_history_pause(h, t, true);
    terminal_history_clamp(h);
    uint64_t before = h->top;
    if (rows > 0) {
        uint64_t distance = (unsigned)rows, limit = h->top - oldest(h);
        h->top -= distance < limit ? distance : limit;
    } else {
        uint64_t distance = (uint64_t)(-(int64_t)rows), limit = h->anchor - h->top;
        h->top += distance < limit ? distance : limit;
    }
    if (h->top != before) { ++h->revision; }
}

unsigned terminal_history_offset(const terminal_history_t *h)
{
    return h->paused ? (unsigned)(h->anchor - h->top) : 0;
}

const terminal_cell_t *terminal_history_row(const terminal_history_t *h,
                                           const terminal_t *t, unsigned row)
{
    if (row >= h->rows) { return NULL; }
    if (!h->paused) { return terminal_cell(t, 0, (uint16_t)row); }
    uint64_t line = h->top + row;
    if (line >= h->anchor) { return h->snapshot + (line - h->anchor) * h->cols; }
    return h->lines + (line % h->capacity) * h->cols;
}
