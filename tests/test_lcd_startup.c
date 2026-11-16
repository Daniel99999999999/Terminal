/* Actual application + upstream ILI9341 driver, virtual GPIO/SPI and time.
 * The controller accepts SPI writes even when an early SLPOUT is invalid:
 * a successful host transaction cannot certify physical panel state. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lcd_test_sdk.h"
#include "lcd_panel.h"
#include "board_pins.h"
#define CHECK(v) do {if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
static uint32_t now_ms,low_at,released_at,sleep_out_at;
static bool low_seen,released,display_on,init_fault,dma_timeout;
static unsigned color_calls,param_calls;
static int semaphore;
static color_done_t callback;
static void *callback_ctx;
static uint16_t dma_pixels[LCD_STRIP_PIXELS];
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
 CHECK(host==SPI3_HOST&&c->cs_gpio_num==21&&c->dc_gpio_num==22&&c->trans_queue_depth==1);
 CHECK(c->pclk_hz==RETRO_TFT_SPI_HZ&&c->lcd_cmd_bits==8&&c->lcd_param_bits==8);
 callback=c->on_color_trans_done;callback_ctx=c->user_ctx;*io=(void *)1;return ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes){
 CHECK(io==(void *)1);++param_calls;
 if(cmd==LCD_CMD_SLPOUT){CHECK(released);sleep_out_at=now_ms;if(init_fault)return ESP_FAIL;}
 if(cmd==LCD_CMD_DISPON)display_on=true;
 if(cmd==LCD_CMD_MADCTL){CHECK(bytes==1&&data);}
 return ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes){
 CHECK(io==(void *)1&&cmd==LCD_CMD_RAMWR&&data==dma_pixels&&bytes==sizeof(dma_pixels));++color_calls;
 if(!dma_timeout){CHECK(callback);callback(io,NULL,callback_ctx);}return ESP_OK;
}
int main(int argc,char **argv){
 init_fault=argc>1&&!strcmp(argv[1],"init_error");
 dma_timeout=argc>1&&!strcmp(argv[1],"dma_timeout");
 esp_err_t result=lcd_panel_open();
 CHECK(released&&released_at-low_at>=1&&sleep_out_at>=released_at);
 printf("RESX release -> first SLPOUT: %lu ms (required >=120)\n",(unsigned long)(sleep_out_at-released_at));
 CHECK(sleep_out_at-released_at>=120);
 if(init_fault){CHECK(result==ESP_FAIL&&!display_on&&color_calls==0);puts("PASS: initialization error is propagated; display remains off");return 0;}
 CHECK(result==ESP_OK&&display_on&&lcd_panel_pixels()==dma_pixels);
 for(unsigned i=0;i<LCD_STRIP_PIXELS;++i)dma_pixels[i]=0x1234;
 CHECK(lcd_panel_write_row(0)==(dma_timeout?ESP_ERR_TIMEOUT:ESP_OK));
 CHECK(color_calls==1&&dma_pixels[0]==0x3412);
 if(dma_timeout){
  unsigned calls=param_calls;CHECK(lcd_panel_write_row(8)==ESP_ERR_INVALID_STATE);
  CHECK(color_calls==1&&param_calls==calls&&dma_pixels[0]==0x3412);
  puts("PASS: timeout retains buffer, blocks resubmission and second byte swap");
 }else{
  CHECK(lcd_panel_write_row(LCD_HEIGHT)==ESP_ERR_INVALID_STATE);
  puts("PASS: warm-reset guard with actual upstream reset/init; completed row DMA and bounds");
 }
 return 0;
}
