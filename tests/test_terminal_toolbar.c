#include <stdio.h>
#include <string.h>
#include "terminal_toolbar.h"
#include "terminal_raster.h"
#define CHECK(x) do { if (!(x)) { printf("FAIL line%d: %s\n",__LINE__,#x); return 1; } } while (0)
static uint16_t pixels[320 * 8 + 2], frame[320 * 24];
int main(void)
{
    CHECK(terminal_toolbar_hit(0,216,320,240)==TOOLBAR_RESUME);
    CHECK(terminal_toolbar_hit(139,239,320,240)==TOOLBAR_RESUME);
    CHECK(terminal_toolbar_hit(140,216,320,240)==TOOLBAR_CALIBRATE);
    CHECK(terminal_toolbar_hit(279,239,320,240)==TOOLBAR_CALIBRATE);
    CHECK(terminal_toolbar_hit(280,239,320,240)==TOOLBAR_NONE);
    CHECK(terminal_toolbar_hit(20,215,320,240)==TOOLBAR_NONE);
    CHECK(terminal_toolbar_hit(20,240,320,240)==TOOLBAR_NONE);
    CHECK(terminal_toolbar_hit(320,230,320,240)==TOOLBAR_NONE);
    for (int link=-1;link<=1;++link) {
        pixels[0]=pixels[320*8+1]=0xa55a;
        for (unsigned strip=0;strip<3;++strip) {
            CHECK(terminal_toolbar_row(strip,320,true,link,0,pixels+1,320*8));
            CHECK(pixels[0]==0xa55a && pixels[320*8+1]==0xa55a);
            memcpy(frame+strip*320*8,pixels+1,320*8*sizeof(uint16_t));
        }
        CHECK(frame[12*320+300]==terminal_rgb565(link>0?0x2080ff:link==0?0x384858:0x101820));
        CHECK(frame[12*320+305]==terminal_rgb565(link>0?0x2080ff:link==0?0x384858:0x708090));
        /* Check the doubled W glyph against the normal terminal raster's bit order. */
        for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
            bool ink=(terminal_glyph_row('W',y)&(1u<<x))!=0;
            CHECK(frame[(4+y*2)*320+30+x*2]==terminal_rgb565(ink?0xffffff:0x284058));
        }
    }
    CHECK(terminal_toolbar_row(2,320,false,-1,100,pixels,320*8));
    CHECK(pixels[4*320+150]==terminal_rgb565(0x40c070));
    CHECK(!terminal_toolbar_row(3,320,true,1,0,pixels,320*8));
    CHECK(!terminal_toolbar_row(0,320,true,1,0,pixels,320*8-1));
    CHECK(!terminal_toolbar_row(0,240,true,1,0,pixels,320*8));
    CHECK(!terminal_toolbar_row(0,320,true,1,0,NULL,320*8));
    puts("PASS toolbar: hit bounds, 3 RGB565 strips, no overrun, unknown/down/blue link, readable doubled glyph, CAL progress");
    return 0;
}
