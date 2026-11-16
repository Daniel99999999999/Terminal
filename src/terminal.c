#include "terminal.h"
#include <string.h>

static terminal_page_t *page(terminal_t *t) { return &t->page[t->active]; }

const terminal_cell_t *terminal_cell(const terminal_t *t, uint16_t x, uint16_t y)
{
    if (!t || x >= t->cols || y >= t->rows) { return NULL; }
    const terminal_page_t *p = &t->page[t->active];
    return &p->cells[(size_t)p->line[y] * t->cols + x];
}

static terminal_cell_t *cell(terminal_t *t, unsigned x, unsigned y)
{
    terminal_page_t *p = page(t);
    return &p->cells[(size_t)p->line[y] * t->cols + x];
}

void terminal_cell_colors(const terminal_cell_t *c, uint32_t *fg, uint32_t *bg)
{
    bool inverse = ((c->fg >> 24) & TERM_INVERSE) != 0;
    *fg = inverse ? c->bg : (c->fg & 0xffffffu);
    *bg = inverse ? (c->fg & 0xffffffu) : c->bg;
}

static terminal_cursor_t default_cursor(void)
{
    terminal_cursor_t c = { .fg = TERMINAL_DEFAULT_FG,
                            .bg = TERMINAL_DEFAULT_BG, .wrap = true };
    return c;
}

static void erase(terminal_t *t, unsigned y, unsigned first, unsigned end)
{
    terminal_cursor_t *c = &page(t)->cursor;
    terminal_cell_t blank = { ' ', c->fg | ((uint32_t)c->flags << 24), c->bg };
    for (unsigned x = first; x < end; ++x) { *cell(t, x, y) = blank; }
}

static void reset_page(terminal_t *t)
{
    terminal_page_t *p = page(t);
    p->cursor = default_cursor();
    p->saved = p->cursor;
    p->top = 0;
    p->bottom = t->rows - 1;
    for (unsigned y = 0; y < t->rows; ++y) {
        p->line[y] = (uint16_t)y;
        erase(t, y, 0, t->cols);
    }
}

void terminal_reset(terminal_t *t)
{
    if (t->history_clear) { t->history_clear(t->history_ctx); }
    for (t->active = 0; t->active < 2; ++t->active) { reset_page(t); }
    t->active = 0;
    t->cursor_visible = true;
    t->insert = t->newline = false;
    t->cr_pending = false;
    t->utf_left = 0;
    for (unsigned x = 0; x < t->cols; ++x) { t->tabs[x] = x && x % 8 == 0; }
    ansi_parser_reset(&t->parser);
    ++t->revision;
}

/* Rotate row indices, not the entire PSRAM screen, even for scroll regions. */
static void scroll(terminal_t *t, unsigned top, unsigned bottom, unsigned n, bool down)
{
    terminal_page_t *p = page(t);
    unsigned height = bottom - top + 1;
    if (n > height) { n = height; }
    if (n == height) {
        if (!down && !t->active && !top && bottom == t->rows - 1 && t->history_line) {
            for (unsigned y = top; y <= bottom; ++y) { t->history_line(t->history_ctx, cell(t, 0, y)); }
        }
        for (unsigned y = top; y <= bottom; ++y) { erase(t, y, 0, t->cols); }
        return;
    }
    for (unsigned i = 0; i < n; ++i) {
        if (down) {
            uint16_t saved = p->line[bottom];
            memmove(&p->line[top + 1], &p->line[top], (height - 1) * sizeof(uint16_t));
            p->line[top] = saved;
            erase(t, top, 0, t->cols);
        } else {
            if (!t->active && !top && bottom == t->rows - 1 && t->history_line) {
                t->history_line(t->history_ctx, cell(t, 0, top));
            }
            uint16_t saved = p->line[top];
            memmove(&p->line[top], &p->line[top + 1], (height - 1) * sizeof(uint16_t));
            p->line[bottom] = saved;
            erase(t, bottom, 0, t->cols);
        }
    }
}

static void index_line(terminal_t *t, bool reverse)
{
    terminal_page_t *p = page(t);
    terminal_cursor_t *c = &p->cursor;
    c->pending_wrap = false;
    if (reverse) {
        if (c->y == p->top) { scroll(t, p->top, p->bottom, 1, true); }
        else if (c->y) { --c->y; }
    } else {
        if (c->y == p->bottom) { scroll(t, p->top, p->bottom, 1, false); }
        else if (c->y + 1 < t->rows) { ++c->y; }
    }
}

static uint32_t dec_graphic(uint32_t ch)
{
    static const uint16_t glyph[31] = {
        0x25c6,0x2592,0x2409,0x240c,0x240d,0x240a,0x00b0,0x00b1,
        0x2424,0x240b,0x2518,0x2510,0x250c,0x2514,0x253c,0x23ba,
        0x23bb,0x2500,0x23bc,0x23bd,0x251c,0x2524,0x2534,0x252c,
        0x2502,0x2264,0x2265,0x03c0,0x2260,0x00a3,0x00b7
    };
    return ch >= 0x60 && ch <= 0x7e ? glyph[ch - 0x60] : ch;
}

static void put(terminal_t *t, uint32_t ch)
{
    terminal_cursor_t *c = &page(t)->cursor;
    if (c->pending_wrap && c->wrap) {
        c->x = 0;
        index_line(t, false);
    }
    c->pending_wrap = false;
    if (t->insert && c->x + 1 < t->cols) {
        memmove(cell(t, c->x + 1, c->y), cell(t, c->x, c->y),
                (t->cols - c->x - 1) * sizeof(terminal_cell_t));
    }
    if ((c->selected ? c->g1 : c->g0) != 0) { ch = dec_graphic(ch); }
    *cell(t, c->x, c->y) = (terminal_cell_t){ ch,
        c->fg | ((uint32_t)c->flags << 24), c->bg };
    if (c->x + 1 == t->cols) { c->pending_wrap = c->wrap; }
    else { ++c->x; }
}

static void text_byte(terminal_t *t, uint8_t ch)
{
    if (t->utf_left) {
        if ((ch & 0xc0) == 0x80) {
            t->utf_value = (t->utf_value << 6) | (ch & 0x3f);
            if (--t->utf_left == 0) {
                uint32_t v = t->utf_value;
                put(t, v < t->utf_min || v > 0x10ffff ||
                    (v >= 0xd800 && v <= 0xdfff) ? 0xfffd : v);
            }
            return;
        }
        t->utf_left = 0;
        put(t, 0xfffd);
    }
    if (ch < 0x80) { put(t, ch); }
    else if (ch >= 0xc2 && ch <= 0xdf) {
        t->utf_left = 1; t->utf_value = ch & 0x1f; t->utf_min = 0x80;
    } else if (ch >= 0xe0 && ch <= 0xef) {
        t->utf_left = 2; t->utf_value = ch & 0x0f; t->utf_min = 0x800;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
        t->utf_left = 3; t->utf_value = ch & 7; t->utf_min = 0x10000;
    } else { put(t, 0xfffd); }
}

static unsigned param(const ansi_sequence_t *s, unsigned i, unsigned fallback)
{
    return i < s->count && s->param[i] ? s->param[i] : fallback;
}

static unsigned min(unsigned a, unsigned b) { return a < b ? a : b; }

static void position(terminal_t *t, unsigned row, unsigned col)
{
    terminal_page_t *p = page(t);
    p->cursor.x = (uint16_t)min(col, t->cols - 1);
    p->cursor.y = (uint16_t)(p->cursor.origin ?
        min(row + p->top, p->bottom) : min(row, t->rows - 1));
    p->cursor.pending_wrap = false;
}

static uint32_t color(unsigned n)
{
    static const uint32_t base[16] = {
        0x000000,0xaa0000,0x00aa00,0xaa5500,0x0000aa,0xaa00aa,0x00aaaa,0xaaaaaa,
        0x555555,0xff5555,0x55ff55,0xffff55,0x5555ff,0xff55ff,0x55ffff,0xffffff
    };
    static const unsigned level[6] = {0,95,135,175,215,255};
    if (n < 16) { return base[n]; }
    if (n < 232) {
        n -= 16;
        return (level[n / 36] << 16) | (level[(n / 6) % 6] << 8) | level[n % 6];
    }
    unsigned v = 8 + (n - 232) * 10;
    return (v << 16) | (v << 8) | v;
}

static void rendition(terminal_t *t, const ansi_sequence_t *s)
{
    terminal_cursor_t *c = &page(t)->cursor;
    for (unsigned i = 0; i < s->count; ++i) {
        unsigned n = s->param[i];
        if (n == 0) { c->fg = TERMINAL_DEFAULT_FG; c->bg = TERMINAL_DEFAULT_BG; c->flags = 0; }
        else if (n == 1) { c->flags |= TERM_BOLD; }
        else if (n == 4) { c->flags |= TERM_UNDERLINE; }
        else if (n == 5) { c->flags |= TERM_BLINK; }
        else if (n == 7) { c->flags |= TERM_INVERSE; }
        else if (n == 22) { c->flags &= (uint8_t)~TERM_BOLD; }
        else if (n == 24) { c->flags &= (uint8_t)~TERM_UNDERLINE; }
        else if (n == 25) { c->flags &= (uint8_t)~TERM_BLINK; }
        else if (n == 27) { c->flags &= (uint8_t)~TERM_INVERSE; }
        else if (n == 39) { c->fg = TERMINAL_DEFAULT_FG; }
        else if (n == 49) { c->bg = TERMINAL_DEFAULT_BG; }
        else if (n >= 30 && n <= 37) { c->fg = color(n - 30); }
        else if (n >= 40 && n <= 47) { c->bg = color(n - 40); }
        else if (n >= 90 && n <= 97) { c->fg = color(n - 90 + 8); }
        else if (n >= 100 && n <= 107) { c->bg = color(n - 100 + 8); }
        else if (n == 38 || n == 48) {
            uint32_t *dest = n == 38 ? &c->fg : &c->bg;
            if (i + 2 < s->count && s->param[i + 1] == 5) {
                if (s->param[i + 2] <= 255) { *dest = color(s->param[i + 2]); }
                i += 2;
            } else if (i + 4 < s->count && s->param[i + 1] == 2) {
                if (s->param[i + 2] <= 255 && s->param[i + 3] <= 255 && s->param[i + 4] <= 255) {
                    *dest = ((uint32_t)s->param[i + 2] << 16) |
                            ((uint32_t)s->param[i + 3] << 8) | s->param[i + 4];
                }
                i += 4;
            } else { break; } /* Incomplete/unknown color group: no partial SGR. */
        }
    }
}

static void mode(terminal_t *t, const ansi_sequence_t *s, bool set)
{
    for (unsigned i = 0; i < s->count; ++i) {
        terminal_page_t *p = page(t);
        unsigned n = s->param[i];
        if (!s->private_mark) {
            if (n == 4) { t->insert = set; }
            if (n == 20) { t->newline = set; }
        } else if (s->private_mark == '?') {
            if (n == 6) { p->cursor.origin = set; position(t, 0, 0); }
            if (n == 7) { p->cursor.wrap = set; p->cursor.pending_wrap = false; }
            if (n == 25) { t->cursor_visible = set; }
            if (n == 1048) {
                if (set) { p->saved = p->cursor; } else { p->cursor = p->saved; }
            }
            if (n == 1049) {
                if (set && !t->active) {
                    t->active = 1;
                    reset_page(t);
                } else if (!set && t->active) { t->active = 0; }
            }
        }
    }
}

static void csi(terminal_t *t, const ansi_sequence_t *s)
{
    terminal_page_t *p = page(t);
    terminal_cursor_t *c = &p->cursor;
    unsigned n = param(s, 0, 1);
    unsigned top = c->origin ? p->top : 0;
    unsigned bottom = c->origin ? p->bottom : t->rows - 1;
    if (s->intermediate) { return; }
    if (s->final == 'h' || s->final == 'l') { mode(t, s, s->final == 'h'); return; }
    if (s->private_mark) { return; }
    switch (s->final) {
    case 'A': c->y = (uint16_t)(c->y - min(n, c->y > top ? c->y - top : 0)); break;
    case 'B': case 'e': c->y = (uint16_t)min(c->y + n, bottom); break;
    case 'C': case 'a': c->x = (uint16_t)min(c->x + n, t->cols - 1); break;
    case 'D': c->x = (uint16_t)(c->x - min(n, c->x)); break;
    case 'E': c->y = (uint16_t)min(c->y + n, bottom); c->x = 0; break;
    case 'F': c->y = (uint16_t)(c->y - min(n, c->y > top ? c->y - top : 0)); c->x = 0; break;
    case 'G': case '`': c->x = (uint16_t)min(n - 1, t->cols - 1); break;
    case 'd': position(t, n - 1, c->x); break;
    case 'H': case 'f': position(t, n - 1, param(s, 1, 1) - 1); break;
    case 'J':
        n = s->param[0];
        if (n == 0) {
            erase(t, c->y, c->x, t->cols);
            for (unsigned y = c->y + 1; y < t->rows; ++y) { erase(t, y, 0, t->cols); }
        } else if (n == 1) {
            for (unsigned y = 0; y < c->y; ++y) { erase(t, y, 0, t->cols); }
            erase(t, c->y, 0, c->x + 1);
        } else if (n == 2) {
            for (unsigned y = 0; y < t->rows; ++y) { erase(t, y, 0, t->cols); }
        }
        break;
    case 'K':
        n = s->param[0];
        if (n == 0) { erase(t, c->y, c->x, t->cols); }
        else if (n == 1) { erase(t, c->y, 0, c->x + 1); }
        else if (n == 2) { erase(t, c->y, 0, t->cols); }
        break;
    case 'X': erase(t, c->y, c->x, min(c->x + n, t->cols)); break;
    case '@':
        n = min(n, t->cols - c->x);
        memmove(cell(t, c->x + n, c->y), cell(t, c->x, c->y),
                (t->cols - c->x - n) * sizeof(terminal_cell_t));
        erase(t, c->y, c->x, c->x + n);
        break;
    case 'P':
        n = min(n, t->cols - c->x);
        memmove(cell(t, c->x, c->y), cell(t, c->x + n, c->y),
                (t->cols - c->x - n) * sizeof(terminal_cell_t));
        erase(t, c->y, t->cols - n, t->cols);
        break;
    case 'L': case 'M':
        if (c->y >= p->top && c->y <= p->bottom) {
            scroll(t, c->y, p->bottom, n, s->final == 'L');
        }
        break;
    case 'S': case 'T': scroll(t, p->top, p->bottom, n, s->final == 'T'); break;
    case 'r': {
        unsigned first = n - 1, last = param(s, 1, t->rows) - 1;
        if (first < last && last < t->rows) {
            p->top = (uint16_t)first; p->bottom = (uint16_t)last;
            position(t, 0, 0);
        }
        break;
    }
    case 's': p->saved = *c; break;
    case 'u': *c = p->saved; break;
    case 'g':
        if (s->param[0] == 0) { t->tabs[c->x] = false; }
        else if (s->param[0] == 3) { memset(t->tabs, 0, sizeof(t->tabs)); }
        break;
    case 'm': rendition(t, s); return; /* SGR does not cancel delayed wrap. */
    default: return; /* Unknown complete commands are consumed as controls. */
    }
    c->pending_wrap = false;
}

static void event(void *ctx, ansi_event_t type, const ansi_sequence_t *s)
{
    terminal_t *t = ctx;
    terminal_page_t *p = page(t);
    terminal_cursor_t *c = &p->cursor;
    if (type == ANSI_TEXT) { text_byte(t, s->final); return; }
    if (t->utf_left) { t->utf_left = 0; put(t, 0xfffd); }
    if (type == ANSI_CSI) { ++t->control_sequences; csi(t, s); return; }
    if (type == ANSI_CONTROL) {
        switch (s->final) {
        case '\r':
            c->x = 0; c->pending_wrap = false;
            if (t->cr_newline) { index_line(t, false); t->cr_pending = true; }
            break;
        case '\n': case '\v': case '\f':
            if (s->final == '\n' && t->cr_newline && t->cr_pending) { break; }
            if (t->newline) { c->x = 0; }
            index_line(t, false); break;
        case '\b': if (c->x) { --c->x; } c->pending_wrap = false; break;
        case '\t':
            do { if (c->x + 1 == t->cols) { break; } ++c->x; }
            while (!t->tabs[c->x]);
            c->pending_wrap = false; break;
        case 0x0e: c->selected = 1; break;
        case 0x0f: c->selected = 0; break;
        default: break;
        }
        return;
    }
    ++t->control_sequences;
    if (s->intermediate == '(' || s->intermediate == ')') {
        if (s->final == '0' || s->final == 'B') {
            if (s->intermediate == '(') { c->g0 = s->final == '0'; }
            else { c->g1 = s->final == '0'; }
        }
        return;
    }
    if (s->intermediate) { return; }
    switch (s->final) {
    case '7': p->saved = *c; break;
    case '8': *c = p->saved; break;
    case 'D': index_line(t, false); break;
    case 'E': c->x = 0; index_line(t, false); break;
    case 'M': index_line(t, true); break;
    case 'H': t->tabs[c->x] = true; break;
    case 'c': terminal_reset(t); break;
    default: break;
    }
}

bool terminal_init(terminal_t *t, uint16_t cols, uint16_t rows,
                   terminal_cell_t *primary, terminal_cell_t *alternate)
{
    if (!t || !primary || !alternate || primary == alternate || !cols || !rows ||
        cols > TERMINAL_MAX_COLS || rows > TERMINAL_MAX_ROWS) { return false; }
    memset(t, 0, sizeof(*t));
    t->cols = cols; t->rows = rows;
    t->page[0].cells = primary; t->page[1].cells = alternate;
    ansi_parser_init(&t->parser, event, t);
    terminal_reset(t);
    return true;
}

void terminal_set_cr_newline(terminal_t *t, bool enable)
{
    if (t) { t->cr_newline = enable; t->cr_pending = false; }
}

void terminal_feed(terminal_t *t, const uint8_t *data, size_t length)
{
    if (!t || (!data && length)) { return; }
    if (!t->cr_newline) { ansi_parser_feed(&t->parser, data, length); }
    else {
        for (size_t i = 0; i < length; ++i) {
            /* Track adjacent input bytes, including ignored OSC/ESC bytes.
             * Interpret CR only when the parser emits a real C0 control. */
            if (data[i] != '\n') { t->cr_pending = false; }
            ansi_parser_feed(&t->parser, data + i, 1);
            if (data[i] == '\n') { t->cr_pending = false; }
        }
    }
    if (length) { ++t->revision; }
}

void terminal_stream_gap(terminal_t *t)
{
    ansi_parser_reset(&t->parser);
    t->utf_left = 0;
    t->cr_pending = false;
    terminal_cursor_t *c = &page(t)->cursor;
    c->fg = TERMINAL_DEFAULT_FG; c->bg = TERMINAL_DEFAULT_BG;
    c->flags = c->g0 = c->g1 = c->selected = 0;
    c->pending_wrap = false;
    ++t->revision;
}
