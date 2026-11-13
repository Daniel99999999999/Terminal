#include "ansi_parser.h"
#include <string.h>

enum { GROUND, ESCAPE, ESC_INTERMEDIATE, CSI, CSI_IGNORE, STRING, STRING_ESC };

void ansi_parser_reset(ansi_parser_t *p)
{
    p->state = GROUND;
    p->length = 0;
    p->parameter_seen = false;
    memset(&p->sequence, 0, sizeof(p->sequence));
}

void ansi_parser_init(ansi_parser_t *p, ansi_emit_fn emit, void *ctx)
{
    memset(p, 0, sizeof(*p));
    p->emit = emit;
    p->ctx = ctx;
}

static void emit(ansi_parser_t *p, ansi_event_t event, uint8_t final)
{
    p->sequence.final = final;
    if (p->emit) {
        p->emit(p->ctx, event, &p->sequence);
    }
}

static void byte(ansi_parser_t *p, uint8_t ch)
{
    if (ch == 0x18 || ch == 0x1a) { /* CAN / SUB cancel incomplete sequences. */
        emit(p, ANSI_CONTROL, ch);
        ansi_parser_reset(p);
        return;
    }
    if (p->state == STRING || p->state == STRING_ESC) {
        if ((ch == 7 && p->string_kind == ']') ||
            (ch == '\\' && p->state == STRING_ESC)) {
            ansi_parser_reset(p);
        } else {
            p->state = ch == 0x1b ? STRING_ESC : STRING;
        }
        return;
    }
    if (ch == 0x1b) {
        emit(p, ANSI_CONTROL, ch); /* Also terminates an incomplete UTF-8 unit. */
        ansi_parser_reset(p);
        p->state = ESCAPE;
        return;
    }
    if (ch < 0x20) {
        emit(p, ANSI_CONTROL, ch);
        return;
    }
    if (ch == 0x7f) { emit(p, ANSI_CONTROL, ch); return; }
    if (p->state != GROUND && ++p->length > ANSI_MAX_SEQUENCE) {
        if (p->state == CSI || p->state == CSI_IGNORE) {
            p->state = CSI_IGNORE;
        } else {
            ansi_parser_reset(p);
            return;
        }
    }
    switch (p->state) {
    case GROUND:
        emit(p, ANSI_TEXT, ch);
        break;
    case ESCAPE:
        if (ch == '[') {
            p->state = CSI;
            p->sequence.count = 1;
        } else if (ch == ']' || ch == 'P' || ch == 'X' || ch == '^' || ch == '_') {
            p->state = STRING;
            p->string_kind = ch;
        } else if (ch >= 0x20 && ch <= 0x2f) {
            p->sequence.intermediate = ch;
            p->state = ESC_INTERMEDIATE;
        } else {
            emit(p, ANSI_ESCAPE, ch);
            ansi_parser_reset(p);
        }
        break;
    case ESC_INTERMEDIATE:
        if (ch >= 0x30 && ch <= 0x7e) {
            emit(p, ANSI_ESCAPE, ch);
            ansi_parser_reset(p);
        } else {
            /* Multiple intermediates are valid but unsupported. */
            p->sequence.intermediate = 0xff;
        }
        break;
    case CSI:
        if (ch >= '0' && ch <= '9' && !p->sequence.intermediate) {
            uint16_t *v = &p->sequence.param[p->sequence.count - 1];
            unsigned n = (unsigned)*v * 10 + ch - '0';
            *v = (uint16_t)(n > UINT16_MAX ? UINT16_MAX : n);
            p->parameter_seen = true;
        } else if (ch == ';' && !p->sequence.intermediate) {
            p->parameter_seen = true;
            if (p->sequence.count == ANSI_MAX_PARAMS) { p->state = CSI_IGNORE; }
            else { ++p->sequence.count; }
        } else if (ch >= 0x3c && ch <= 0x3f && !p->parameter_seen &&
                   !p->sequence.private_mark && !p->sequence.intermediate) {
            p->sequence.private_mark = ch;
        } else if (ch >= 0x20 && ch <= 0x2f && !p->sequence.intermediate) {
            p->sequence.intermediate = ch;
        } else if (ch >= 0x40 && ch <= 0x7e) {
            emit(p, ANSI_CSI, ch);
            ansi_parser_reset(p);
        } else {
            p->state = CSI_IGNORE; /* Includes unsupported colon subparameters. */
        }
        break;
    case CSI_IGNORE:
        if (ch >= 0x40 && ch <= 0x7e) { ansi_parser_reset(p); }
        break;
    default:
        ansi_parser_reset(p);
        break;
    }
}

void ansi_parser_feed(ansi_parser_t *p, const uint8_t *data, size_t length)
{
    if (!p || (!data && length)) { return; }
    for (size_t i = 0; i < length; ++i) { byte(p, data[i]); }
}
