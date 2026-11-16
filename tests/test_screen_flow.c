
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include "../src/screen.c"
static int64_t fake_now, last_inject, next_inject=4000000;
static unsigned event_no, comparisons, writes;
static bool mock_pause, mock_overlay;
static int mock_scroll;
static uint32_t mock_revision=1;
static uint16_t strip[LCD_STRIP_PIXELS], frame[LCD_WIDTH*LCD_HEIGHT], expected[LCD_STRIP_PIXELS];
static jmp_buf finished;
static void (*task_fn)(void *);
static const char *commands[]={
 "\x1b[0m\x1b[2J\x1b[Hplain", "\r\nsecond", "\x1b[7mTEST\x1b[0m", "\nLF ONLY", "\r\nCRLF",
 "\x1b[12;25Hposition", "\x1b[s\x1b[3;12Hsaved\x1b[u-restored", "\x1b[?25h\x1b[8;5Hcursor",
 "\x1b[8;30Hmore", "\x1b[20;2Hbottom", "\x1b[?25l", "\x1b[1;1HABCDEFG\x1b[4D\x1b[2P",
 "\x1b[29;1Hline\r\nscroll\r\nscroll2", "\x1b[2;20r\x1b[20;1Hregion\r\nnext", "\x1b[?1049hALT PAGE",
 "\x1b[?1049l", "\x1b[3;3H\x1b[5mBLINK\x1b[0m", "\x1b[?25h\x1b[3;30HCURSOR",
 "\x1b[r\x1b[29;1H\r\nA\r\nB\r\nC\r\nD\r\nE\r\nF", "older", "history receives edits", "newer", "resume", "pause", "received while paused\r\n", "resume", "overlay", "received behind overlay", "exit overlay",
 "\x1b[0m\x1b[2J\x1b[HFINAL\r\nOK"
};
void test_log(const char *tag,const char *fmt,...) {(void)tag;(void)fmt;}
const char *esp_err_to_name(esp_err_t err){(void)err;return "mock";}
const esp_app_desc_t *esp_app_get_description(void){static const esp_app_desc_t d={"probe-v0.3.9"};return &d;}
void *heap_caps_calloc(size_t n,size_t size,unsigned caps){(void)caps;return calloc(n,size);}
void app_health_beat(health_channel_t c){(void)c;}
void app_health_fault(void){fprintf(stderr,"Unexpected health fault\n");exit(2);}
int64_t esp_timer_get_time(void){return fake_now;}
uint32_t esp_rom_crc32_le(uint32_t crc,const uint8_t *data,uint32_t n){while(n--)crc=crc*33+*data++;return crc;}
typedef struct {unsigned size,head,count;unsigned char data[64][160];} queue_t;
QueueHandle_t xQueueCreate(unsigned n,unsigned size){assert(n==64&&size<=160);queue_t *q=calloc(1,sizeof(*q));q->size=size;return q;}
BaseType_t xQueueSend(QueueHandle_t h,const void *p,TickType_t ticks){(void)ticks;queue_t *q=h;if(q->count==64)return pdFALSE;memcpy(q->data[(q->head+q->count++)%64],p,q->size);return pdTRUE;}
BaseType_t xQueueReceive(QueueHandle_t h,void *p,TickType_t ticks){(void)ticks;queue_t *q=h;if(!q->count)return pdFALSE;memcpy(p,q->data[q->head++%64],q->size);--q->count;return pdTRUE;}
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned prio,void *handle){(void)name;(void)stack;(void)arg;(void)prio;(void)handle;task_fn=fn;return pdPASS;}
esp_err_t lcd_panel_open(void){return ESP_OK;}
esp_err_t lcd_panel_show(void){return ESP_OK;}
uint16_t *lcd_panel_pixels(void){return strip;}
esp_err_t lcd_panel_write_row(unsigned y){assert(y+8<=LCD_HEIGHT);memcpy(frame+y*LCD_WIDTH,strip,sizeof(strip));fake_now+=2100;++writes;return ESP_OK;}
esp_err_t xpt2046_init(void){return ESP_OK;}
esp_err_t xpt2046_read(xpt2046_sample_t *s){*s=(xpt2046_sample_t){0};fake_now+=300;return ESP_OK;}
void xpt2046_get_stats(xpt2046_stats_t *s){memset(s,0,sizeof(*s));}
void touch_ui_init(bool cr){assert(cr);}
void touch_ui_update(const xpt2046_sample_t *s,uint32_t now){(void)s;(void)now;}
bool touch_ui_visible(void){return mock_overlay;}
bool touch_ui_paused(void){return mock_pause;}
int touch_ui_take_scroll(void){int n=mock_scroll;mock_scroll=0;return n;}
bool touch_ui_cr_newline(void){return true;}
unsigned touch_ui_cal_progress(void){return 0;}
uint32_t touch_ui_revision(void){return mock_revision;}
bool touch_ui_render_row(unsigned row,uint16_t *p,size_t n){(void)row;assert(n==LCD_STRIP_PIXELS);for(size_t i=0;i<n;++i)p[i]=0x1234;return true;}
static void compare_frame(void){
 const terminal_cursor_t *c=&s_terminal.page[s_terminal.active].cursor;
 bool phase=mock_pause||((fake_now/500000)&1)==0;
 for(unsigned row=0;row<ROWS;++row){
  assert(terminal_raster_row(terminal_history_row(&s_history,&s_terminal,row),COLS,c->x,!mock_pause&&s_terminal.cursor_visible&&c->y==row&&phase,phase,expected,LCD_STRIP_PIXELS));
  for(unsigned i=0;i<LCD_STRIP_PIXELS;++i)if(expected[i]!=frame[row*LCD_STRIP_PIXELS+i]){
   fprintf(stderr,"FAIL event=%u row=%u x=%u y=%u expected=%04x actual=%04x cursor=%u,%u\n",event_no,row,i%LCD_WIDTH,i/LCD_WIDTH,expected[i],frame[row*LCD_STRIP_PIXELS+i],c->x,c->y);exit(1);
  }
 }
 ++comparisons;
}
void vTaskDelay(TickType_t ticks){
 fake_now+=(int64_t)ticks*1000;
 if(fake_now>=next_inject){
  if(event_no<sizeof(commands)/sizeof(*commands)){
   const char *s=commands[event_no++];
   if(!strcmp(s,"older")){mock_pause=true;mock_scroll=3;}
   else if(!strcmp(s,"newer")){mock_scroll=-2;}
   else if(!strcmp(s,"pause"))mock_pause=true;
   else if(!strcmp(s,"resume"))mock_pause=false;
   else if(!strcmp(s,"overlay")){mock_overlay=true;++mock_revision;}
   else if(!strcmp(s,"exit overlay"))mock_overlay=false;
   else for(size_t pos=0,len=strlen(s);pos<len;){size_t n=1+(pos%7);if(n>len-pos)n=len-pos;screen_input((const uint8_t *)s+pos,n);pos+=n;}
   next_inject=fake_now+700000;last_inject=fake_now;
  }else if(fake_now-last_inject>1400000)longjmp(finished,1);
 }
 int64_t phase=fake_now%500000;
 if(fake_now>4500000&&fake_now-last_inject>250000&&phase>150000&&phase<400000&&!mock_overlay)compare_frame();
}
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t ticks){assert(clear==pdTRUE);vTaskDelay(ticks);return 0;}
int main(void){
 assert(screen_start()==ESP_OK);
 if(setjmp(finished)==0)task_fn(NULL);
 printf("PASS actual screen.c: %u pixel comparisons, %u writes, %u events; SGR/CUP/CR/LF/scroll/alternate/cursor/blink/frozen pause/history gestures/overlay; virtual LCD, no physical test\n",comparisons,writes,event_no);
 return 0;
}
