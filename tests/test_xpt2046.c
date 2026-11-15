#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpt2046.h"
#include "board_pins.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#define CHECK(v) do { if (!(v)) { printf("FAIL line %d: %s\n", __LINE__, #v); exit(1); } } while (0)
static int irq = 1, calls, fault, weak, bad_bounds, release_during_read;
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
{ CHECK(host == SPI2_HOST && dma == SPI_DMA_DISABLED && c->max_transfer_sz == 32);
  CHECK(c->sclk_io_num == 14 && c->mosi_io_num == 13 && c->miso_io_num == 39);
  return ESP_OK; }
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *c, spi_device_handle_t *device)
{ CHECK(host == SPI2_HOST && c->mode == 0 && c->spics_io_num == 33 && c->queue_size == 1);
  CHECK(c->clock_speed_hz <= 2000000); *device = (void *)1; return ESP_OK; }
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t *t, uint32_t wait)
{
    CHECK(device == (void *)1 && !pending && wait == 0);
    /* One CS-low transaction, no per-conversion powerdown. Only last command
     * rearms PENIRQ, including for weak/released/out-of-range samples. */
    const unsigned commands[] = {0xb1,0xc1,0xd1,0xd1,0xd1,0xd1,0x91,0x91,0x91,0x90};
    CHECK(t->length == 240 && t->rxlength == 240 && !t->flags);
    CHECK(t->tx_buffer && t->rx_buffer);
    CHECK(((uintptr_t)t->tx_buffer % 4) == 0 && ((uintptr_t)t->rx_buffer % 4) == 0);
    const uint8_t *tx = t->tx_buffer;
    for (unsigned i = 0; i < 10; ++i) { CHECK(tx[3*i] == commands[i] && !tx[3*i+1] && !tx[3*i+2]); }
    calls++; pending = t; return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t device, spi_transaction_t **t, uint32_t wait)
{
    CHECK(device == (void *)1 && pending && wait == 20);
    if (fault) { return ESP_ERR_TIMEOUT; }
    unsigned values[] = {1000,3500,0,1200,4000,1300,0,2800,2900,50};
    if (weak) { values[0] = 1; values[1] = 4095; }
    if (bad_bounds) { values[3] = values[4] = values[5] = 4095; }
    uint8_t *rx = pending->rx_buffer;
    for (unsigned i = 0; i < 10; ++i) {
        rx[3*i] = 0; rx[3*i+1] = (uint8_t)((values[i] << 3) >> 8); rx[3*i+2] = (uint8_t)(values[i] << 3);
    }
    if (release_during_read) { irq = 1; }
    *t = pending; pending = NULL; return ESP_OK;
}

int main(void)
{
    xpt2046_sample_t sample; xpt2046_stats_t stats;
    CHECK(xpt2046_read(NULL) == ESP_ERR_INVALID_ARG);
    CHECK(xpt2046_read(&sample) == ESP_ERR_INVALID_STATE);
    CHECK(xpt2046_init() == ESP_OK);
    CHECK(xpt2046_init() == ESP_ERR_INVALID_STATE);
    int before = calls;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed && calls == before);
    irq = 0;
    CHECK(xpt2046_read(&sample) == ESP_OK && sample.pressed && calls == before + 1);
    CHECK(sample.x == 1300 && sample.y == 2800 && sample.pressure == 1595);
    weak = 1; CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed); weak = 0;
    bad_bounds = 1; CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed); bad_bounds = 0;
    release_during_read = 1; CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed); release_during_read = 0;
    xpt2046_get_stats(&stats);
    CHECK(stats.polls == 5 && stats.irq_low == 4 && stats.valid == 1);
    CHECK(stats.weak == 1 && stats.bounds == 1 && stats.end_high == 1 && stats.io_errors == 0);
    irq = 1;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed && !sample.x && !sample.y);
    irq = 0; fault = 1;
    CHECK(xpt2046_read(&sample) == ESP_ERR_TIMEOUT);
    CHECK(pending != NULL);
    spi_transaction_t retained = *pending;
    uint8_t tx[32], rx[32]; memcpy(tx,pending->tx_buffer,32); memcpy(rx,pending->rx_buffer,32);
    before = calls;
    CHECK(xpt2046_read(&sample) == ESP_ERR_INVALID_STATE && calls == before);
    CHECK(memcmp(&retained, pending, sizeof(retained)) == 0);
    CHECK(memcmp(tx,pending->tx_buffer,32) == 0 && memcmp(rx,pending->rx_buffer,32) == 0);
    xpt2046_get_stats(&stats); CHECK(stats.io_errors == 1);
    puts("PASS: XPT2046 single CS burst/ADC power/PENIRQ rearm, median/dummy samples, pressure/bounds/release diagnostics, timeout buffer lifetime");
    return 0;
}
