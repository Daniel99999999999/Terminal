/* Real app + unchanged upstream driver; virtual transport/time, not an LCD. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lcd_test_sdk.h"
#include "lcd_panel.h"
#include "board_pins.h"
#define CHECK(v) do {if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
static uint32_t now_ms,low_at,released_at,sleep_out_at,on_at;
static bool low_seen,released,display_on,init_fault,dma_timeout,color_fault,read_fault,recreate_fault;
static unsigned color_calls,param_calls,reads,created,deleted,clock_hz;
static int semaphore;
static esp_lcd_panel_io_t backend;
static color_done_t callback;
static void *callback_ctx;
static uint16_t dma_pixels[LCD_STRIP_PIXELS];
const char *esp_err_to_name(esp_err_t err){return err==ESP_OK?"OK":"ERROR";}
void lcd_test_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
void vTaskDelay(TickType_t ticks){now_ms+=ticks;}
int64_t esp_timer_get_time(void){return (int64_t)now_ms*1000;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){semaphore=0;return &semaphore;}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem,BaseType_t *wake){CHECK(sem==&semaphore);semaphore=1;*wake=pdFALSE;return pdTRUE;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem,TickType_t timeout){CHECK(sem==&semaphore&&timeout==250);if(!semaphore)return pdFALSE;semaphore=0;return pdTRUE;}
esp_err_t gpio_set_level(int pin,unsigned level){
 CHECK(pin==TFT_CS_GPIO||pin==TFT_RST_GPIO);
 if(pin==TFT_RST_GPIO){if(!level){low_at=now_ms;low_seen=true;}else if(low_seen){released=true;released_at=now_ms;}}
 return ESP_OK;
}
esp_err_t gpio_set_direction(int pin,int mode){CHECK((pin==TFT_CS_GPIO||pin==TFT_RST_GPIO)&&mode==GPIO_MODE_OUTPUT);return ESP_OK;}
esp_err_t gpio_config(const gpio_config_t *c){CHECK(c->mode==GPIO_MODE_OUTPUT&&c->pin_bit_mask==(1ULL<<TFT_RST_GPIO));return ESP_OK;}
esp_err_t gpio_reset_pin(int pin){CHECK(pin==TFT_RST_GPIO);return ESP_OK;}
void *heap_caps_malloc(size_t bytes,unsigned caps){CHECK(bytes==sizeof(dma_pixels)&&caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));return dma_pixels;}
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *c,int dma){
 CHECK(host==SPI3_HOST&&dma==SPI_DMA_CH_AUTO&&c->max_transfer_sz==sizeof(dma_pixels));
 CHECK(c->sclk_io_num==18&&c->mosi_io_num==23&&c->miso_io_num==19);return ESP_OK;
}
esp_err_t esp_lcd_new_panel_io_spi(int host,const esp_lcd_panel_io_spi_config_t *c,esp_lcd_panel_io_handle_t *io){
 CHECK(host==SPI3_HOST&&c->cs_gpio_num==4&&c->dc_gpio_num==22&&c->trans_queue_depth==1);
 CHECK((c->pclk_hz==1000000||c->pclk_hz==RETRO_TFT_SPI_HZ)&&c->spi_mode==0);
 CHECK(c->lcd_cmd_bits==8&&c->lcd_param_bits==8&&c->cs_ena_pretrans==2&&c->cs_ena_posttrans==2);
 if(recreate_fault&&created){return ESP_FAIL;}
 ++created;clock_hz=(unsigned)c->pclk_hz;
 callback=c->on_color_trans_done;callback_ctx=c->user_ctx;*io=&backend;return ESP_OK;
}
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io){CHECK(io==&backend);++deleted;return ESP_OK;}
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes){
 if(io!=&backend){return io->tx_param(io,cmd,data,bytes);}
 ++param_calls;
 if(cmd==LCD_CMD_SLPOUT){CHECK(released);sleep_out_at=now_ms;if(init_fault)return ESP_FAIL;}
 else if(sleep_out_at){CHECK(now_ms-sleep_out_at>=150);}
 if(cmd==LCD_CMD_DISPON){CHECK(color_calls==LCD_HEIGHT/8);display_on=true;on_at=now_ms;}
 if(cmd==LCD_CMD_DISPOFF){display_on=false;}
 if(cmd==LCD_CMD_MADCTL){CHECK(bytes==1&&data);}
 return ESP_OK;
}
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t io,int cmd,void *data,size_t bytes){
 if(io!=&backend){return io->rx_param(io,cmd,data,bytes);}
 static const unsigned commands[]={0xd3,0x09,0x0a,0x0b,0x0c},sizes[]={4,5,2,2,2};
 CHECK(clock_hz==1000000&&cmd==(int)commands[reads%5]&&bytes==sizes[reads%5]);
 if(display_on){CHECK(now_ms-on_at>=150);}else{CHECK(color_calls==0);}
 memset(data,0,bytes);++reads;return read_fault?ESP_FAIL:ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes){
 if(io!=&backend){return io->tx_color(io,cmd,data,bytes);}
 CHECK(cmd==LCD_CMD_RAMWR&&data==dma_pixels&&bytes==sizeof(dma_pixels));
 CHECK(clock_hz==RETRO_TFT_SPI_HZ&&now_ms-sleep_out_at>=150);++color_calls;
 if(color_fault){return ESP_FAIL;}
 if(!dma_timeout){CHECK(callback);callback(io,NULL,callback_ctx);}return ESP_OK;
}
int main(int argc,char **argv){
 const char *mode=argc>1?argv[1]:"normal";
 init_fault=!strcmp(mode,"init_error");dma_timeout=!strcmp(mode,"dma_timeout");
 color_fault=!strcmp(mode,"color_error");read_fault=!strcmp(mode,"read_error");recreate_fault=!strcmp(mode,"recreate_error");
 esp_err_t result=lcd_panel_open();
 CHECK(released&&released_at-low_at>=1&&sleep_out_at>=released_at);
 CHECK(sleep_out_at-released_at>=120);
 if(init_fault||recreate_fault){CHECK(result==ESP_FAIL&&!display_on&&color_calls==0);puts("PASS LCD: init/transport error propagated; display remains off");return 0;}
 CHECK(result==ESP_OK&&!display_on&&lcd_panel_pixels()==dma_pixels&&reads==5);
 CHECK(lcd_panel_show()==ESP_ERR_INVALID_STATE);
 CHECK(lcd_panel_write_row(1)==ESP_ERR_INVALID_STATE);
 for(unsigned row=0;row<LCD_HEIGHT/8;++row){
  for(unsigned i=0;i<LCD_STRIP_PIXELS;++i){dma_pixels[i]=0x1234;}
  CHECK(lcd_panel_write_row(row*8)==(dma_timeout?ESP_ERR_TIMEOUT:color_fault?ESP_FAIL:ESP_OK));
  CHECK(dma_pixels[0]==0x3412);
  if(dma_timeout||color_fault){
   unsigned calls=param_calls;CHECK(lcd_panel_write_row(8)==ESP_ERR_INVALID_STATE);
   CHECK(color_calls==1&&param_calls==calls&&dma_pixels[0]==0x3412);
   CHECK(lcd_panel_show()==ESP_ERR_INVALID_STATE&&!display_on);
   puts("PASS LCD: DMA timeout/queue error latched; no reuse, second byte swap or DISPON");return 0;
  }
  if(row+1<LCD_HEIGHT/8){CHECK(lcd_panel_show()==ESP_ERR_INVALID_STATE);}
 }
 CHECK(lcd_panel_show()==ESP_OK&&display_on&&now_ms-on_at>=150);
 CHECK(reads==10&&created==4&&deleted==3&&clock_hz==RETRO_TFT_SPI_HZ);
 CHECK(lcd_panel_show()==ESP_OK&&reads==10);
 CHECK(lcd_panel_write_row(LCD_HEIGHT)==ESP_ERR_INVALID_STATE);
 puts("PASS LCD: reset/wake/display guards, full GRAM before DISPON, 1 MHz raw reads, 10 MHz writes, CS margins, informational read errors");
 return 0;
}
