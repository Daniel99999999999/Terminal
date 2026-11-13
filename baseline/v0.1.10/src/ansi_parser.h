#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ANSI_MAX_PARAMS 16
#define ANSI_MAX_SEQUENCE 128

typedef enum { ANSI_TEXT, ANSI_CONTROL, ANSI_ESCAPE, ANSI_CSI } ansi_event_t;
typedef struct {
    uint16_t param[ANSI_MAX_PARAMS];
    uint8_t count;
    uint8_t final;
    uint8_t private_mark;
    uint8_t intermediate;
} ansi_sequence_t;
typedef void (*ansi_emit_fn)(void *ctx, ansi_event_t event,
                             const ansi_sequence_t *sequence);

/* Bounded streaming parser. Default text is UTF-8, so raw C1 bytes are not
 * interpreted as controls. ANSI sequences use their usual 7-bit ESC forms.
 * OSC/DCS/APC/PM strings are consumed without storing their contents. */
typedef struct {
    ansi_sequence_t sequence;
    uint16_t length;
    uint8_t state;
    uint8_t string_kind;
    bool parameter_seen;
    ansi_emit_fn emit;
    void *ctx;
} ansi_parser_t;

void ansi_parser_init(ansi_parser_t *parser, ansi_emit_fn emit, void *ctx);
void ansi_parser_reset(ansi_parser_t *parser);
void ansi_parser_feed(ansi_parser_t *parser, const uint8_t *data, size_t length);
