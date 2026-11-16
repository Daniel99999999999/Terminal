#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define IRAM_ATTR
#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(6,0,1)
#define __containerof(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NO_MEM 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
const char *esp_err_to_name(esp_err_t err);
#define ESP_RETURN_ON_ERROR(call,...) do {esp_err_t e_=(call);if(e_!=ESP_OK)return e_;}while(0)
#define ESP_GOTO_ON_ERROR(call,label,...) do {ret=(call);if(ret!=ESP_OK)goto label;}while(0)
#define ESP_GOTO_ON_FALSE(cond,code,label,...) do {if(!(cond)){ret=(code);goto label;}}while(0)
void lcd_test_log(const char *tag,const char *fmt,...);
#define ESP_LOGI(...) lcd_test_log(__VA_ARGS__)
#define ESP_LOGD(...) lcd_test_log(__VA_ARGS__)
#define ESP_LOGW(...) lcd_test_log(__VA_ARGS__)
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(TickType_t ticks);
int64_t esp_timer_get_time(void);
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem,BaseType_t *wake);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem,TickType_t timeout);
#define GPIO_MODE_OUTPUT 2
typedef struct {uint64_t pin_bit_mask;int mode;} gpio_config_t;
esp_err_t gpio_set_level(int pin,unsigned level);
esp_err_t gpio_set_direction(int pin,int direction);
esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_reset_pin(int pin);
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
void *heap_caps_malloc(size_t bytes,unsigned caps);
#define SPI3_HOST 2
#define SPI_DMA_CH_AUTO 3
typedef struct {int sclk_io_num,mosi_io_num,miso_io_num,quadwp_io_num,quadhd_io_num;size_t max_transfer_sz;} spi_bus_config_t;
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *config,int dma);
typedef struct esp_lcd_panel_io_t esp_lcd_panel_io_t;
typedef esp_lcd_panel_io_t *esp_lcd_panel_io_handle_t;
typedef struct {int unused;} esp_lcd_panel_io_event_data_t;
typedef bool (*color_done_t)(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t *,void *);
typedef struct {int cs_gpio_num,dc_gpio_num,spi_mode,pclk_hz,trans_queue_depth,lcd_cmd_bits,lcd_param_bits,cs_ena_pretrans,cs_ena_posttrans;color_done_t on_color_trans_done;void *user_ctx;} esp_lcd_panel_io_spi_config_t;
esp_err_t esp_lcd_new_panel_io_spi(int host,const esp_lcd_panel_io_spi_config_t *config,esp_lcd_panel_io_handle_t *io);
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes);
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io,int cmd,const void *data,size_t bytes);
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t io,int cmd,void *data,size_t bytes);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io);
struct esp_lcd_panel_io_t {
 esp_err_t (*rx_param)(esp_lcd_panel_io_t *,int,void *,size_t);
 esp_err_t (*tx_param)(esp_lcd_panel_io_t *,int,const void *,size_t);
 esp_err_t (*tx_color)(esp_lcd_panel_io_t *,int,const void *,size_t);
};
#define LCD_RGB_ELEMENT_ORDER_RGB 0
#define LCD_RGB_ELEMENT_ORDER_BGR 1
typedef struct {int reset_gpio_num,bits_per_pixel,rgb_ele_order;struct {bool reset_active_high;} flags;const void *vendor_config;} esp_lcd_panel_dev_config_t;
typedef struct esp_lcd_panel_t esp_lcd_panel_t;
typedef esp_lcd_panel_t *esp_lcd_panel_handle_t;
struct esp_lcd_panel_t {
 esp_err_t (*del)(esp_lcd_panel_t *);
 esp_err_t (*reset)(esp_lcd_panel_t *);
 esp_err_t (*init)(esp_lcd_panel_t *);
 esp_err_t (*draw_bitmap)(esp_lcd_panel_t *,int,int,int,int,const void *);
 esp_err_t (*invert_color)(esp_lcd_panel_t *,bool);
 esp_err_t (*mirror)(esp_lcd_panel_t *,bool,bool);
 esp_err_t (*swap_xy)(esp_lcd_panel_t *,bool);
 esp_err_t (*set_gap)(esp_lcd_panel_t *,int,int);
 esp_err_t (*disp_on_off)(esp_lcd_panel_t *,bool);
 esp_err_t (*disp_sleep)(esp_lcd_panel_t *,bool);
};
static inline esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t p){return p->reset(p);}
static inline esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t p){return p->init(p);}
static inline esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p,int x,int y,int xe,int ye,const void *d){return p->draw_bitmap(p,x,y,xe,ye,d);}
static inline esp_err_t esp_lcd_panel_swap_xy(esp_lcd_panel_handle_t p,bool b){return p->swap_xy(p,b);}
static inline esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t p,bool x,bool y){return p->mirror(p,x,y);}
static inline esp_err_t esp_lcd_panel_invert_color(esp_lcd_panel_handle_t p,bool b){return p->invert_color(p,b);}
static inline esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t p,bool b){return p->disp_on_off(p,b);}
#define LCD_CMD_SWRESET 0x01
#define LCD_CMD_SLPIN 0x10
#define LCD_CMD_SLPOUT 0x11
#define LCD_CMD_INVOFF 0x20
#define LCD_CMD_INVON 0x21
#define LCD_CMD_DISPOFF 0x28
#define LCD_CMD_DISPON 0x29
#define LCD_CMD_CASET 0x2a
#define LCD_CMD_RASET 0x2b
#define LCD_CMD_RAMWR 0x2c
#define LCD_CMD_MADCTL 0x36
#define LCD_CMD_COLMOD 0x3a
#define LCD_CMD_MX_BIT 0x40
#define LCD_CMD_MY_BIT 0x80
#define LCD_CMD_MV_BIT 0x20
#define LCD_CMD_BGR_BIT 0x08
