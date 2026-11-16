#include <stdio.h>
#include <string.h>
#include "terminal_history.h"
#define CHECK(x) do {if(!(x)){printf("FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static terminal_t t;
static terminal_history_t h;
static terminal_cell_t primary[18],alternate[18],ring[24],snapshot[18];
static void feed(const char *s){terminal_feed(&t,(const uint8_t *)s,strlen(s));}
static void row(unsigned y,char ch){char s[24];snprintf(s,sizeof(s),"\x1b[%u;1H%c",y+1,ch);feed(s);}
int main(void){
 CHECK(terminal_init(&t,6,3,primary,alternate));
 CHECK(terminal_history_init(&h,&t,ring,4,snapshot));
 row(0,'A');row(1,'B');row(2,'C');feed("\x1b[S");
 CHECK(h.total==1&&ring[0].codepoint=='A');
 CHECK(terminal_cell(&t,0,0)->codepoint=='B');
 terminal_history_move(&h,&t,1);
 CHECK(h.paused&&terminal_history_offset(&h)==1);
 CHECK(terminal_history_row(&h,&t,0)->codepoint=='A');
 CHECK(terminal_history_row(&h,&t,1)->codepoint=='B');
 row(0,'Z'); /* Incoming CUP edits do not modify the frozen live snapshot. */
 CHECK(terminal_history_row(&h,&t,1)->codepoint=='B');
 terminal_history_move(&h,&t,-20);CHECK(terminal_history_offset(&h)==0);
 CHECK(terminal_history_row(&h,&t,0)->codepoint=='B');
 terminal_history_pause(&h,&t,false);CHECK(terminal_history_row(&h,&t,0)->codepoint=='Z');
 feed("\033c");CHECK(h.total==0);
 for(unsigned i=0;i<7;++i){row(0,(char)('A'+i));feed("\x1b[S");}
 CHECK(h.total==7);
 terminal_history_move(&h,&t,100);CHECK(terminal_history_offset(&h)==4);
 CHECK(terminal_history_row(&h,&t,0)->codepoint=='D');
 CHECK(terminal_history_row(&h,&t,1)->codepoint=='E');
 /* Continued RX overwrites only the ring, bounds the old view and retains
  * the immutable live page; no array index may escape these two buffers. */
 for(unsigned i=0;i<9;++i){row(0,'N');feed("\x1b[S");}
 CHECK(terminal_history_offset(&h)==0);
 for(unsigned y=0;y<3;++y){CHECK(terminal_history_row(&h,&t,y)==snapshot+y*6);}
 terminal_history_pause(&h,&t,false);
 feed("\033c");row(0,'A');row(1,'B');row(2,'C');feed("\x1b[3S");
 CHECK(h.total==3&&ring[0].codepoint=='A'&&ring[6].codepoint=='B'&&ring[12].codepoint=='C');
 feed("\033c\x1b[2;3r\x1b[S\x1b[T");CHECK(h.total==0); /* Region/down excluded. */
 feed("\x1b[r\x1b[?1049hALT\x1b[S");CHECK(h.total==0);
 terminal_history_move(&h,&t,100);CHECK(terminal_history_offset(&h)==0); /* No primary/ALT mixing. */
 terminal_history_pause(&h,&t,false);feed("\x1b[?1049l\033c\x1b[31;44;7m\xc5\xbc\x1b[S");
 CHECK(h.total==1&&ring[0].codepoint==0x17c&&ring[0].fg==0x04aa0000&&ring[0].bg==0xaa);
 terminal_history_pause(&h,&t,true);terminal_cell_t frozen=snapshot[0];
 feed("\033c");CHECK(h.total==0&&h.paused&&!memcmp(&frozen,terminal_history_row(&h,&t,0),sizeof(frozen)));
 CHECK(!terminal_history_row(&h,&t,3));
 puts("PASS history: logical row rotation, colors/UTF8, ring wrap, paused RX/CUP, overflow clamp, full erase scroll, reset, ALT/regions excluded");
 return 0;
}
