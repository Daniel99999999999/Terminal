/* Host-only reference probe: inspect cells and render a reproducible image.
 * Not linked into the firmware. Input is fed one byte at a time. */
#include <stdio.h>
#include "terminal.h"
#include "terminal_raster.h"

static terminal_cell_t primary[40 * 29], alternate[40 * 29];
static terminal_t terminal;

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) { return 2; }
    FILE *input = fopen(argv[1], "rb");
    if (!input || !terminal_init(&terminal, 40, 29, primary, alternate)) { return 2; }
    terminal_set_cr_newline(&terminal, true);
    int ch;
    while ((ch = fgetc(input)) != EOF) {
        uint8_t byte = (uint8_t)ch;
        terminal_feed(&terminal, &byte, 1);
    }
    if (ferror(input)) { fclose(input); return 2; }
    fclose(input);
    const terminal_cursor_t *cursor = &terminal.page[terminal.active].cursor;
    printf("{\"cursor\":[%u,%u],\"controls\":%lu,\"active\":%u,\"visible\":%s,\"cells\":[",
           cursor->x, cursor->y, (unsigned long)terminal.control_sequences,
           terminal.active, terminal.cursor_visible ? "true" : "false");
    for (unsigned y = 0; y < 29; ++y) {
        printf("%s[", y ? "," : "");
        for (unsigned x = 0; x < 40; ++x) {
            const terminal_cell_t *c = terminal_cell(&terminal, (uint16_t)x, (uint16_t)y);
            printf("%s[%lu,%lu,%lu]", x ? "," : "", (unsigned long)c->codepoint,
                   (unsigned long)c->fg, (unsigned long)c->bg);
        }
        printf("]");
    }
    puts("]}");
    if (argc == 3) {
        FILE *image = fopen(argv[2], "wb");
        if (!image) { return 2; }
        fprintf(image, "P6\n320 240\n255\n");
        uint16_t pixels[320 * 8];
        for (unsigned row = 0; row < 30; ++row) {
            terminal_cell_t cells[40];
            const char *footer = "HC05 115200 8N1 | CR=NL";
            for (unsigned x = 0; x < 40; ++x) {
                if (row < 29) { cells[x] = *terminal_cell(&terminal, (uint16_t)x, (uint16_t)row); }
                else {
                    cells[x] = (terminal_cell_t){ .codepoint = *footer ? (unsigned char)*footer++ : ' ',
                        .fg = 0xaaaaaa, .bg = 0x101010 };
                }
            }
            if (!terminal_raster_row(cells, 40, cursor->x,
                row == cursor->y && terminal.cursor_visible, true, pixels, 320 * 8)) {
                fclose(image); return 2;
            }
            for (unsigned i = 0; i < 320 * 8; ++i) {
                uint16_t p = pixels[i];
                fputc((((p >> 11) & 31) * 255 + 15) / 31, image);
                fputc((((p >> 5) & 63) * 255 + 31) / 63, image);
                fputc(((p & 31) * 255 + 15) / 31, image);
            }
        }
        if (ferror(image)) { fclose(image); return 2; }
        fclose(image);
    }
    return 0;
}
