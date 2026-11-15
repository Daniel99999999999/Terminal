#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpt2046.h"
#include "board_pins.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); exit(1); } } while (0)
static int irq = 1, calls, x_count, y_count, fault;
static spi_transaction_t *pending;

esp_err_t gpio_set_level(int pin, unsigned level)
{ CHECK(pin == TOUCH_CS_GPIO && level == 1); return ESP_OK; }
esp_err_t gpio_set_direction(int pin, int mode)
{ CHECK(pin == TOUCH_CS_GPIO && mode == GPIO_MODE_OUTPUT); return ESP_OK; }
esp_err_t gpio_config(const gpio_config_t *c)
{ CHECK(c->pin_bit_mask == (1ULL << TOUCH_IRQ_GPIO) && c->mode == GPIO_MODE_INPUT);
  CHECK(!c->pull_up_en && !c->pull_down_en); return ESP_OK; }
int gpio_get_level(int pin) { CHECK(pin == TOUCH_IRQ_GPIO); return irq; }
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *c, int dma)
{ CHECK(host == SPI2_HOST && dma == SPI_DMA_DISABLED);
  CHECK(c->sclk_io_num == 14 && c->mosi_io_num == 13 && c->miso_io_num == 39);
  return ESP_OK; }
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *c, spi_device_handle_t *device)
{ CHECK(host == SPI2_HOST && c->mode == 0 && c->spics_io_num == 33 && c->queue_size == 1);
  CHECK(c->clock_speed_hz <= 2000000); *device = (void *)1; return ESP_OK; }
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t *t, uint32_t wait)
{
    CHECK(device == (void *)1 && !pending && wait == 0);
    CHECK(t->length == 24 && t->rxlength == 24);
    CHECK(t->flags == (SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA));
    CHECK(!t->tx_data[1] && !t->tx_data[2]);
    calls++; pending = t; return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t device, spi_transaction_t **t, uint32_t wait)
{
    CHECK(device == (void *)1 && pending && wait == 20);
    if (fault) { return ESP_ERR_TIMEOUT; }
    unsigned value;
    switch (pending->tx_data[0]) {
    case 0xb0: value = 1000; break;
    case 0xc0: value = 3500; break;
    case 0xd0: { const unsigned values[] = {0, 1200, 4000, 1300}; value = values[x_count++ % 4]; break; }
    case 0x90: { const unsigned values[] = {0, 2800, 2900, 50}; value = values[y_count++ % 4]; break; }
    default: CHECK(0); value = 0;
    }
    pending->rx_data[1] = (uint8_t)((value << 3) >> 8);
    pending->rx_data[2] = (uint8_t)(value << 3);
    *t = pending; pending = NULL; return ESP_OK;
}

int main(void)
{
    xpt2046_sample_t sample;
    CHECK(xpt2046_read(NULL) == ESP_ERR_INVALID_ARG);
    CHECK(xpt2046_read(&sample) == ESP_ERR_INVALID_STATE);
    CHECK(xpt2046_init() == ESP_OK);
    CHECK(xpt2046_init() == ESP_ERR_INVALID_STATE);
    int before = calls;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed && calls == before);
    irq = 0; x_count = y_count = 0;
    CHECK(xpt2046_read(&sample) == ESP_OK && sample.pressed);
    CHECK(sample.x == 1300 && sample.y == 2800 && sample.pressure == 1595);
    irq = 1;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed && !sample.x && !sample.y);
    irq = 0; fault = 1;
    CHECK(xpt2046_read(&sample) == ESP_ERR_TIMEOUT);
    CHECK(pending != NULL);
    spi_transaction_t retained = *pending;
    before = calls;
    CHECK(xpt2046_read(&sample) == ESP_ERR_INVALID_STATE && calls == before);
    CHECK(memcmp(&retained, pending, sizeof(retained)) == 0);
    puts("PASS: XPT2046 protocol, IRQ idle/release, ADC/median/pressure, timeout descriptor lifetime");
    return 0;
}
