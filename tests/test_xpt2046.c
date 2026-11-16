#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpt2046.h"
#include "board_pins.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); exit(1); } } while (0)
static int irq=1,calls,fault,weak,bad_bounds,release_during_read,level_reads,notifications;
static spi_transaction_t *pending;
static void (*handler)(void *);
static void *handler_ctx;
static void edge(int level){irq=level;CHECK(handler);handler(handler_ctx);}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return (void *)2;}
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake){CHECK(task==(void *)2);++notifications;*wake=pdFALSE;}
esp_err_t gpio_install_isr_service(int flags){CHECK(flags==0);return ESP_OK;}
esp_err_t gpio_isr_handler_add(int pin,void (*fn)(void *),void *ctx){CHECK(pin==TOUCH_IRQ_GPIO);handler=fn;handler_ctx=ctx;return ESP_OK;}
esp_err_t gpio_set_level(int pin,unsigned level){CHECK(pin==TOUCH_CS_GPIO&&level==1);return ESP_OK;}
esp_err_t gpio_set_direction(int pin,int mode){CHECK(pin==TOUCH_CS_GPIO&&mode==GPIO_MODE_OUTPUT);return ESP_OK;}
esp_err_t gpio_config(const gpio_config_t *c){
 CHECK(c->pin_bit_mask==(1ULL<<TOUCH_IRQ_GPIO)&&c->mode==GPIO_MODE_INPUT);
 CHECK(!c->pull_up_en&&!c->pull_down_en&&c->intr_type==GPIO_INTR_ANYEDGE);return ESP_OK;
}
int gpio_get_level(int pin){CHECK(pin==TOUCH_IRQ_GPIO);++level_reads;return irq;}
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *c,int dma){
 CHECK(host==SPI2_HOST&&dma==SPI_DMA_DISABLED&&c->max_transfer_sz==4);
 CHECK(c->sclk_io_num==14&&c->mosi_io_num==13&&c->miso_io_num==39);return ESP_OK;
}
esp_err_t spi_bus_add_device(int host,const spi_device_interface_config_t *c,spi_device_handle_t *device){
 CHECK(host==SPI2_HOST&&c->mode==0&&c->spics_io_num==33&&c->queue_size==1);
 CHECK(c->clock_speed_hz<=2000000);*device=(void *)1;return ESP_OK;
}
esp_err_t spi_device_queue_trans(spi_device_handle_t device,spi_transaction_t *t,uint32_t wait){
 const unsigned commands[]={0xb0,0xc0,0xd0,0xd0,0xd0,0xd0,0x90,0x90,0x90,0x90};
 CHECK(device==(void *)1&&!pending&&wait==0&&t->length==24&&t->rxlength==24);
 CHECK(t->flags==(SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA));
 CHECK(t->tx_data[0]==commands[calls%10]&&!t->tx_data[1]&&!t->tx_data[2]);
 ++calls;pending=t;return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t device,spi_transaction_t **t,uint32_t wait){
 CHECK(device==(void *)1&&pending&&wait==20);if(fault){return ESP_ERR_TIMEOUT;}
 unsigned values[]={1000,3500,0,1200,4000,1300,0,2800,2900,50};
 if(weak){values[0]=1;values[1]=4095;}if(bad_bounds){values[3]=values[4]=values[5]=4095;}
 unsigned value=values[(calls-1)%10];
 pending->rx_data[0]=0;pending->rx_data[1]=(uint8_t)((value<<3)>>8);pending->rx_data[2]=(uint8_t)(value<<3);
 if(handler){handler(handler_ctx);} /* PENIRQ pulses during ADC must be ignored. */
 if(release_during_read){irq=1;}
 *t=pending;pending=NULL;return ESP_OK;
}
int main(void){
 xpt2046_sample_t sample;xpt2046_stats_t stats;
 CHECK(xpt2046_read(NULL)==ESP_ERR_INVALID_ARG&&xpt2046_read(&sample)==ESP_ERR_INVALID_STATE);
 CHECK(xpt2046_init()==ESP_OK&&calls==10&&handler);
 CHECK(xpt2046_init()==ESP_ERR_INVALID_STATE);
 CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.contact);
 int before=calls,gpio_before=level_reads;
 for(int i=0;i<100;++i){CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.pressed&&!sample.contact);}
 CHECK(calls==before&&level_reads==gpio_before);
 edge(0);CHECK(notifications==1);
 CHECK(xpt2046_read(&sample)==ESP_OK&&sample.pressed&&sample.contact&&calls==before+10);
 CHECK(sample.x==1300&&sample.y==2800&&sample.pressure==1595&&notifications==1);
 CHECK(xpt2046_read(&sample)==ESP_OK&&sample.pressed&&calls==before+20);
 weak=1;CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.pressed&&sample.contact);weak=0;
 bad_bounds=1;CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.pressed&&sample.contact);bad_bounds=0;
 release_during_read=1;CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.pressed&&!sample.contact);release_during_read=0;
 before=calls;gpio_before=level_reads;
 for(int i=0;i<10;++i){CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.contact);}
 CHECK(calls==before&&level_reads==gpio_before&&notifications==1);
 xpt2046_get_stats(&stats);
 CHECK(stats.polls==5&&stats.irq_low==5&&stats.valid==2&&stats.weak==1&&stats.bounds==1&&stats.end_high==1&&stats.irq_edges==1);
 edge(0);CHECK(xpt2046_read(&sample)==ESP_OK&&sample.pressed);
 edge(1);before=calls;CHECK(xpt2046_read(&sample)==ESP_OK&&!sample.contact&&calls==before);
 edge(0);fault=1;CHECK(xpt2046_read(&sample)==ESP_ERR_TIMEOUT&&pending);
 spi_transaction_t retained=*pending;before=calls;
 CHECK(xpt2046_read(&sample)==ESP_ERR_INVALID_STATE&&calls==before&&!memcmp(&retained,pending,sizeof(retained)));
 xpt2046_get_stats(&stats);CHECK(stats.io_errors==1&&stats.irq_edges==4&&notifications==4);
 puts("PASS XPT: IRQ wakes owner; no idle ADC/GPIO polling; held acquisition, self-edge suppression, weak/bounds/contact/release and retained timeout descriptor");
 return 0;
}
