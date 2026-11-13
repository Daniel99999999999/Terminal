#include <stdio.h>
#include <string.h>
#include "terminal.h"

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); return 1; } } while (0)
#define CAPACITY (80 * 24)
typedef struct {
    uint32_t before;
    terminal_cell_t a[CAPACITY];
    uint32_t between;
    terminal_cell_t b[CAPACITY];
    uint32_t after;
    terminal_t t;
} fixture_t;
static fixture_t f, g;

static void init(fixture_t *v, unsigned cols, unsigned rows)
{
    memset(v, 0xcc, sizeof(*v));
    if (!terminal_init(&v->t, (uint16_t)cols, (uint16_t)rows, v->a, v->b)) {
        fprintf(stderr, "fixture initialization failed\n");
    }
}
static void send_to(fixture_t *v, const char *s)
{
    terminal_feed(&v->t, (const uint8_t *)s, strlen(s));
}
static void send(const char *s) { send_to(&f, s); }
static const terminal_cell_t *at(unsigned x, unsigned y) { return terminal_cell(&f.t, (uint16_t)x, (uint16_t)y); }
static terminal_cursor_t *cursor(void) { return &f.t.page[f.t.active].cursor; }
static int row(unsigned y, const char *s)
{
    for (unsigned x = 0; s[x]; ++x) { CHECK(at(x, y)->codepoint == (uint8_t)s[x]); }
    return 0;
}
static int valid(fixture_t *v)
{
    CHECK(v->before == 0xcccccccc && v->between == 0xcccccccc && v->after == 0xcccccccc);
    CHECK(v->t.active < 2);
    for (unsigned p = 0; p < 2; ++p) {
        terminal_page_t *pg = &v->t.page[p];
        CHECK(pg->cursor.x < v->t.cols && pg->cursor.y < v->t.rows);
        CHECK(pg->top <= pg->bottom && pg->bottom < v->t.rows);
        for (unsigned y = 0; y < v->t.rows; ++y) {
            CHECK(pg->line[y] < v->t.rows);
            for (unsigned j = 0; j < y; ++j) { CHECK(pg->line[y] != pg->line[j]); }
        }
        for (size_t i = (size_t)v->t.rows * v->t.cols; i < CAPACITY; ++i) {
            CHECK(pg->cells[i].codepoint == 0xcccccccc);
            CHECK(pg->cells[i].fg == 0xcccccccc && pg->cells[i].bg == 0xcccccccc);
        }
    }
    return 0;
}

static int initialization(void)
{
    CHECK(!terminal_init(NULL, 12, 5, f.a, f.b));
    CHECK(!terminal_init(&f.t, 0, 5, f.a, f.b));
    CHECK(!terminal_init(&f.t, 12, 0, f.a, f.b));
    CHECK(!terminal_init(&f.t, 12, 65, f.a, f.b));
    CHECK(!terminal_init(&f.t, 161, 5, f.a, f.b));
    CHECK(!terminal_init(&f.t, 12, 5, f.a, f.a));
    init(&f, 80, 24);
    for (unsigned y = 0; y < 24; ++y) for (unsigned x = 0; x < 80; ++x) {
        CHECK(at(x,y)->codepoint == ' ');
        CHECK(at(x,y)->fg == 0xffffff && at(x,y)->bg == 0);
    }
    CHECK(terminal_cell(&f.t, 80, 0) == NULL);
    return valid(&f);
}
static int plain(void)
{
    init(&f,12,5); send("Hello\r\nworld");
    CHECK(row(0,"Hello") == 0 && row(1,"world") == 0);
    CHECK(cursor()->x == 5 && cursor()->y == 1);
    CHECK(f.t.control_sequences == 0 && at(0,1)->fg == 0xffffff && at(0,1)->bg == 0);
    return valid(&f);
}
static int basic_controls(void)
{
    init(&f,12,5); send("AB\tX\bY\rZ");
    CHECK(row(0,"ZB      Y") == 0);
    CHECK(cursor()->x == 1 && cursor()->y == 0);
    send("\x1b[20h\nX"); CHECK(at(0,1)->codepoint == 'X');
    send("\x1b[20l\nY"); CHECK(at(1,2)->codepoint == 'Y');
    return valid(&f);
}
static int sgr(void)
{
    init(&f,12,5); send("A\x1b[31;44;1;4mB\x1b[0mC");
    CHECK(at(0,0)->fg == 0xffffff && at(0,0)->bg == 0);
    CHECK((at(1,0)->fg & 0xffffff) == 0xaa0000 && at(1,0)->bg == 0x0000aa);
    CHECK((at(1,0)->fg >> 24) == (TERM_BOLD | TERM_UNDERLINE));
    CHECK(at(2,0)->fg == 0xffffff && at(2,0)->bg == 0);
    send("\x1b[97;100;5mD\x1b[25;39;49mE");
    CHECK((at(3,0)->fg & 0xffffff) == 0xffffff && at(3,0)->bg == 0x555555);
    CHECK(at(4,0)->fg == 0xffffff && at(4,0)->bg == 0);
    return valid(&f);
}
static int extended_colors(void)
{
    init(&f,12,5); send("\x1b[38;5;196;48;5;244mA\x1b[38;2;1;2;3;48;2;4;5;6mB");
    CHECK(at(0,0)->fg == 0xff0000 && at(0,0)->bg == 0x808080);
    CHECK(at(1,0)->fg == 0x010203 && at(1,0)->bg == 0x040506);
    send("\x1b[38;2;999;0;0mC\x1b[39;49mD");
    CHECK(at(2,0)->fg == 0x010203);
    CHECK(at(3,0)->fg == 0xffffff && at(3,0)->bg == 0);
    return valid(&f);
}
static int inverse(void)
{
    init(&f,12,5); send("\x1b[7mA\x1b[27mB");
    uint32_t fg,bg;
    terminal_cell_colors(at(0,0), &fg,&bg); CHECK(fg == 0 && bg == 0xffffff);
    terminal_cell_colors(at(1,0), &fg,&bg); CHECK(fg == 0xffffff && bg == 0);
    return valid(&f);
}
static int cursor_motion(void)
{
    init(&f,12,5); send("\x1b[3;4HX\x1b[2AZ\x1b[999B\x1b[999CY");
    CHECK(at(3,2)->codepoint == 'X' && at(4,0)->codepoint == 'Z');
    CHECK(at(11,4)->codepoint == 'Y');
    send("\x1b[999999999999999999999999A\x1b[999999999999999999999999DZ");
    CHECK(at(0,0)->codepoint == 'Z');
    send("\x1b[0;0HQ"); CHECK(at(0,0)->codepoint == 'Q');
    return valid(&f);
}
static int erase_display(void)
{
    init(&f,12,5); send("ABC\r\nDEF\x1b[1;2H\x1b[J");
    CHECK(row(0,"A           ") == 0 && row(1,"            ") == 0);
    send("\x1b[2;3HX\x1b[2J");
    CHECK(cursor()->x == 3 && cursor()->y == 1);
    for (unsigned y=0;y<5;++y) CHECK(row(y,"            ") == 0);
    send("\x1b[1;1HABC\x1b[2;1HDEF\x1b[2;2H\x1b[1J");
    CHECK(row(0,"            ") == 0 && row(1,"  F") == 0);
    return valid(&f);
}
static int erase_line(void)
{
    init(&f,12,5); send("ABCDEF\x1b[1;3H\x1b[K");
    CHECK(row(0,"AB          ") == 0);
    send("\rABCDEF\x1b[1;3H\x1b[1K"); CHECK(row(0,"   DEF") == 0);
    send("\x1b[2K"); CHECK(row(0,"            ") == 0);
    send("\rABCDEF\x1b[1;3H\x1b[2X"); CHECK(row(0,"AB  EF") == 0);
    CHECK(cursor()->x == 2);
    return valid(&f);
}
static int wrap(void)
{
    init(&f,4,3); send("ABCD"); CHECK(cursor()->y == 0 && cursor()->x == 3);
    send("\x1b[31mE"); CHECK(row(0,"ABCD") == 0 && at(0,1)->codepoint == 'E');
    send("\x1b[?7l\x1b[1;4H12"); CHECK(row(0,"ABC2") == 0 && cursor()->y == 0);
    return valid(&f);
}
static void fill_rows(void)
{
    send("\x1b[1;1HA\x1b[2;1HB\x1b[3;1HC\x1b[4;1HD\x1b[5;1HE");
}
static int scroll_region(void)
{
    init(&f,12,5); fill_rows(); send("\x1b[2;4r\x1b[4;1H\n");
    CHECK(at(0,0)->codepoint == 'A' && at(0,1)->codepoint == 'C');
    CHECK(at(0,2)->codepoint == 'D' && at(0,3)->codepoint == ' ' && at(0,4)->codepoint == 'E');
    send("\x1b[2;1H\x1bM");
    CHECK(at(0,1)->codepoint == ' ' && at(0,2)->codepoint == 'C');
    send("\x1b[?6h\x1b[1;1HX"); CHECK(at(0,1)->codepoint == 'X');
    send("\x1b[999;1HY"); CHECK(at(0,3)->codepoint == 'Y');
    return valid(&f);
}
static int character_edits(void)
{
    init(&f,12,5); send("ABCDEF\x1b[1;3H\x1b[2@"); CHECK(row(0,"AB  CDEF") == 0);
    send("\x1b[2P"); CHECK(row(0,"ABCDEF  ") == 0);
    send("\x1b[4hZ\x1b[4l"); CHECK(row(0,"ABZCDEF") == 0);
    send("\x1b[999@"); CHECK(row(0,"ABZ         ") == 0);
    return valid(&f);
}
static int line_edits(void)
{
    init(&f,12,5); fill_rows(); send("\x1b[2;4r\x1b[3;1H\x1b[L");
    CHECK(at(0,1)->codepoint == 'B' && at(0,2)->codepoint == ' ' && at(0,3)->codepoint == 'C');
    CHECK(at(0,4)->codepoint == 'E');
    send("\x1b[M"); CHECK(at(0,2)->codepoint == 'C' && at(0,3)->codepoint == ' ');
    send("\x1b[999S"); CHECK(at(0,0)->codepoint == 'A' && at(0,4)->codepoint == 'E');
    return valid(&f);
}
static int save_restore(void)
{
    init(&f,12,5); send("\x1b[31mA\x1b" "7\x1b[3;5H\x1b[32mX\x1b" "8B");
    CHECK(row(0,"AB") == 0 && at(4,2)->codepoint == 'X');
    CHECK(at(1,0)->fg == 0xaa0000);
    send("\x1b[s\x1b[5;12HZ\x1b[uC"); CHECK(row(0,"ABC") == 0);
    return valid(&f);
}
static int alternate_screen(void)
{
    init(&f,12,5); send("\x1b[31mA\x1b[?1049hALT");
    CHECK(f.t.active == 1 && row(0,"ALT") == 0 && at(0,0)->fg == 0xffffff);
    send("\x1b[?1049hX"); CHECK(row(0,"ALTX") == 0);
    send("\x1b[?1049lB"); CHECK(f.t.active == 0 && row(0,"AB") == 0 && at(1,0)->fg == 0xaa0000);
    send("\x1b[?25l"); CHECK(!f.t.cursor_visible);
    send("\x1b[?25h"); CHECK(f.t.cursor_visible);
    return valid(&f);
}
static int line_graphics(void)
{
    init(&f,12,5); send("\x1b(0lqk\x1b(Bq\x1b)0\x0ex\x0fq");
    CHECK(at(0,0)->codepoint == 0x250c && at(1,0)->codepoint == 0x2500);
    CHECK(at(2,0)->codepoint == 0x2510 && at(3,0)->codepoint == 'q');
    CHECK(at(4,0)->codepoint == 0x2502 && at(5,0)->codepoint == 'q');
    return valid(&f);
}
static int utf8(void)
{
    init(&f,12,5); send("z\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87");
    const uint32_t expected[] = {'z',0x17c,0xf3,0x142,0x107};
    for(unsigned x=0;x<5;++x) CHECK(at(x,0)->codepoint == expected[x]);
    send("\xed\xa0\x80X"); CHECK(at(5,0)->codepoint == 0xfffd && at(6,0)->codepoint == 'X');
    send("\xc3\x1b[0mY"); CHECK(at(7,0)->codepoint == 0xfffd && at(8,0)->codepoint == 'Y');
    return valid(&f);
}
static int control_strings(void)
{
    init(&f,40,5); send("A\x1b]0;ignored title\aB\x1b]52;c;ignored\x1b\\C\x1bPignored\x1b\\D\x1b_ignored\x1b\\E");
    CHECK(row(0,"ABCDE") == 0 && cursor()->x == 5);
    send("\x1b]unfinished\x18X"); CHECK(at(5,0)->codepoint == 'X');
    return valid(&f);
}
static int malformed(void)
{
    init(&f,40,5);
    send("\x1b[38:2::255:0:0mA\x1b[1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16;17mB");
    CHECK(row(0,"AB") == 0 && at(0,0)->fg == 0xffffff && at(1,0)->fg == 0xffffff);
    send("\x1b["); for(unsigned i=0;i<200;++i)send("1"); send("mC");
    CHECK(at(2,0)->codepoint == 'C' && at(2,0)->fg == 0xffffff);
    send("\x1b[31\x18" "D\x1b[32\x1a" "E"); CHECK(row(0,"ABCDE") == 0);
    return valid(&f);
}
static int stream_gap(void)
{
    init(&f,12,5); send("\x1b[31mA\x1b[32"); terminal_stream_gap(&f.t); send("mB");
    CHECK(row(0,"AmB") == 0 && at(1,0)->fg == 0xffffff && at(2,0)->fg == 0xffffff);
    send("\xc3"); terminal_stream_gap(&f.t); send("X"); CHECK(at(3,0)->codepoint == 'X');
    send("\x1b" "c"); CHECK(row(0,"            ") == 0 && cursor()->x == 0 && cursor()->y == 0);
    return valid(&f);
}
static int fragmentation(void)
{
    const char *s="Start\r\n\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87\x1b[31;44mRED\x1b[0m\x1b[3;4HX\x1b]0;title\x1b\\\x1b[?1049hOTHER\x1b[?1049l\x1b(0q\x1b(B\x1b[?25l";
    size_t length=strlen(s);
    init(&f,12,5); send(s);
    for(size_t split=0;split<=length;++split){
        init(&g,12,5);
        terminal_feed(&g.t,(const uint8_t*)s,split);
        terminal_feed(&g.t,(const uint8_t*)s+split,length-split);
        for(unsigned y=0;y<5;++y) for(unsigned x=0;x<12;++x){
            const terminal_cell_t *a=at(x,y),*b=terminal_cell(&g.t,(uint16_t)x,(uint16_t)y);
            CHECK(a->codepoint==b->codepoint && a->fg==b->fg && a->bg==b->bg);
        }
        CHECK(g.t.page[g.t.active].cursor.x==cursor()->x && g.t.page[g.t.active].cursor.y==cursor()->y);
        CHECK(g.t.active==f.t.active && g.t.cursor_visible==f.t.cursor_visible);
        CHECK(g.t.control_sequences==f.t.control_sequences && valid(&g)==0);
    }
    init(&g,12,5);
    for(size_t i=0;i<length;++i) terminal_feed(&g.t,(const uint8_t*)s+i,1);
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<12;++x){
        const terminal_cell_t *a=at(x,y),*b=terminal_cell(&g.t,(uint16_t)x,(uint16_t)y);
        CHECK(a->codepoint==b->codepoint && a->fg==b->fg && a->bg==b->bg);
    }
    return valid(&g);
}
static int adversarial(void)
{
    init(&f,8,4); uint32_t state=0x92ac74u;
    for(unsigned i=0;i<20000;++i){
        state=state*1664525u+1013904223u;
        uint8_t b=(uint8_t)(state>>24); terminal_feed(&f.t,&b,1);
        if(i%32==0) send("\x18\x1b[65535;65535H\x1b[65535P\x1b[65535@\x1b[65535A\x1b[65535D");
        if(i%127==0) send("\x1b[?1049h\x1b[2;3r\x1b[?6h\x1b[3;4H\x1b[s\x1b[1;2r\x1b[u\x1b[A\x1b[?1049l");
        CHECK(valid(&f)==0);
    }
    init(&f,1,1); send("abc\r\n\x1bM\x1b[999P\x1b[999@\x1b[999S\x1b[999T");
    return valid(&f);
}

int main(void)
{
    int (*tests[])(void)={initialization,plain,basic_controls,sgr,extended_colors,inverse,
        cursor_motion,erase_display,erase_line,wrap,scroll_region,character_edits,line_edits,
        save_restore,alternate_screen,line_graphics,utf8,control_strings,malformed,stream_gap,
        fragmentation,adversarial};
    for(unsigned i=0;i<sizeof(tests)/sizeof(tests[0]);++i) if(tests[i]()) return 1;
    printf("terminal: %u scenarios PASS (all split points, UTF-8, ANSI/VT100, colors, guards, adversarial stream)\n",
           (unsigned)(sizeof(tests)/sizeof(tests[0])));
    return 0;
}
