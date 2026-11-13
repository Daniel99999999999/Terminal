#include <stdio.h>
#include <string.h>
#include "terminal_raster.h"

#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); return 1; } } while (0)

int main(void)
{
    CHECK(terminal_rgb565(0xff0000) == 0xf800);
    CHECK(terminal_rgb565(0x00ff00) == 0x07e0);
    CHECK(terminal_rgb565(0x0000ff) == 0x001f);
    CHECK(terminal_rgb565(0xffffff) == 0xffff && terminal_rgb565(0) == 0);
    terminal_cell_t cells[2] = {{'A', 0xffffff, 0}, {' ', 0xffffff, 0}};
    terminal_cell_t saved[2]; memcpy(saved, cells, sizeof(cells));
    struct { uint16_t before, pixels[128], after; } out = {.before=0x1234,.after=0xabcd};
    CHECK(!terminal_raster_row(cells,2,0,false,true,out.pixels,127));
    CHECK(!terminal_raster_row(NULL,2,0,false,true,out.pixels,128));
    CHECK(!terminal_raster_row(cells,0,0,false,true,out.pixels,128));
    CHECK(!terminal_raster_row(cells,TERMINAL_MAX_COLS+1,0,false,true,out.pixels,128));
    CHECK(terminal_raster_row(cells,2,0,false,true,out.pixels,128));
    /* Golden public-domain A row 0 is 0x0c; bit 0 is the leftmost pixel. */
    CHECK(out.pixels[0]==0 && out.pixels[1]==0 && out.pixels[2]==0xffff && out.pixels[3]==0xffff);
    for (unsigned y=0;y<8;++y) for(unsigned x=8;x<16;++x) CHECK(out.pixels[y*16+x]==0);
    CHECK(out.before==0x1234 && out.after==0xabcd && memcmp(saved,cells,sizeof(cells))==0);
    cells[0]=(terminal_cell_t){' ',0xff0000 | ((uint32_t)TERM_INVERSE<<24),0x0000ff};
    CHECK(terminal_raster_row(cells,1,0,false,true,out.pixels,64));
    for(unsigned i=0;i<64;++i) CHECK(out.pixels[i]==0xf800);
    cells[0]=(terminal_cell_t){'A',0xffffff | ((uint32_t)TERM_BLINK<<24),0};
    CHECK(terminal_raster_row(cells,1,0,false,false,out.pixels,64));
    for(unsigned i=0;i<64;++i) CHECK(out.pixels[i]==0);
    cells[0]=(terminal_cell_t){' ',0xffffff | ((uint32_t)TERM_UNDERLINE<<24),0};
    CHECK(terminal_raster_row(cells,1,0,false,true,out.pixels,64));
    for(unsigned i=56;i<64;++i) CHECK(out.pixels[i]==0xffff);
    cells[0]=(terminal_cell_t){' ',0xffffff,0};
    CHECK(terminal_raster_row(cells,1,0,true,true,out.pixels,64));
    for(unsigned i=0;i<48;++i) CHECK(out.pixels[i]==0);
    for(unsigned i=48;i<64;++i) CHECK(out.pixels[i]==0xffff);
    CHECK(terminal_glyph_row(0x2588,3)==0xff && terminal_glyph_row('A',8)==0);
    CHECK(terminal_glyph_row(0x1f600,0)==terminal_glyph_row('?',0));
    bool box=false, polish=false;
    for(unsigned y=0;y<8;++y) { box |= terminal_glyph_row(0x253c,y)!=0; polish |= terminal_glyph_row(0x142,y)!=0; }
    CHECK(box && polish);
    puts("PASS: RGB565/glyph orientation/attributes/cursor/Unicode fallback/raster bounds");
    return 0;
}
