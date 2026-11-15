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
{ CHECK(host == SPI2_HOST && dma == SPI_DMA_DISABLED && c->max_transfer_sz == 4);
  CHECK(c->sclk_io_num == 14 && c->mosi_io_num == 13 && c->miso_io_num == 39);
  return ESP_OK; }
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *c, spi_device_handle_t *device)
{ CHECK(host == SPI2_HOST && c->mode == 0 && c->spics_io_num == 33 && c->queue_size == 1);
  CHECK(c->clock_speed_hz <= 2000000); *device = (void *)1; return ESP_OK; }
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t *t, uint32_t wait)
{
    CHECK(device == (void *)1 && !pending && wait == 0);
    /* Independent 24-clock frames with inline buffers; every command rearms
     * PENIRQ. ADC test values below include noisy first reads on both axes. */
    const unsigned commands[] = {0xb0,0xc0,0xd0,0xd0,0xd0,0xd0,0x90,0x90,0x90,0x90};
    CHECK(t->length == 24 && t->rxlength == 24);
    CHECK(t->flags == (SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA));
    CHECK(t->tx_data[0] == commands[calls % 10] && !t->tx_data[1] && !t->tx_data[2]);
    calls++; pending = t; return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t device, spi_transaction_t **t, uint32_t wait)
{
    CHECK(device == (void *)1 && pending && wait == 20);
    if (fault) { return ESP_ERR_TIMEOUT; }
    unsigned values[] = {1000,3500,0,1200,4000,1300,0,2800,2900,50};
    if (weak) { values[0] = 1; values[1] = 4095; }
    if (bad_bounds) { values[3] = values[4] = values[5] = 4095; }
    unsigned value = values[(calls - 1) % 10];
    pending->rx_data[0] = 0;
    pending->rx_data[1] = (uint8_t)((value << 3) >> 8);
    pending->rx_data[2] = (uint8_t)(value << 3);
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
    weak = 1;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed);
    CHECK(calls == before + 10 * !RETRO_TOUCH_REQUIRE_IRQ);
    weak = 0;
    before = calls;
    irq = 0;
    CHECK(xpt2046_read(&sample) == ESP_OK && sample.pressed && calls == before + 10);
    CHECK(sample.x == 1300 && sample.y == 2800 && sample.pressure == 1595);
    weak = 1; CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed); weak = 0;
    bad_bounds = 1; CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed); bad_bounds = 0;
    release_during_read = 1;
    CHECK(xpt2046_read(&sample) == ESP_OK && sample.pressed == !RETRO_TOUCH_REQUIRE_IRQ);
    release_during_read = 0;
    xpt2046_get_stats(&stats);
    CHECK(stats.polls == 5 && stats.irq_low == 4 && stats.valid == 1u + !RETRO_TOUCH_REQUIRE_IRQ);
    CHECK(stats.weak == 1u + !RETRO_TOUCH_REQUIRE_IRQ && stats.bounds == 1 && stats.end_high == 1 && stats.io_errors == 0);
    CHECK(stats.last_z1 == 1000 && stats.last_z2 == 3500 && !stats.last_irq_high);
    irq = 1;
    before = calls;
    CHECK(xpt2046_read(&sample) == ESP_OK && sample.pressed == !RETRO_TOUCH_REQUIRE_IRQ);
    CHECK(calls == before + 10 * !RETRO_TOUCH_REQUIRE_IRQ);
    xpt2046_get_stats(&stats); CHECK(stats.last_irq_high);
    weak = 1;
    CHECK(xpt2046_read(&sample) == ESP_OK && !sample.pressed);
    weak = 0;
    irq = 0; fault = 1;
    CHECK(xpt2046_read(&sample) == ESP_ERR_TIMEOUT);
    CHECK(pending != NULL);
    spi_transaction_t retained = *pending;
    before = calls;
    CHECK(xpt2046_read(&sample) == ESP_ERR_INVALID_STATE && calls == before);
    CHECK(memcmp(&retained, pending, sizeof(retained)) == 0);
    xpt2046_get_stats(&stats); CHECK(stats.io_errors == 1);
    printf("PASS: XPT2046 require_irq=%d; 24-clock inline frames/PENIRQ per conversion, HIGH IRQ with/without pressure, median/dummy samples, diagnostics, timeout descriptor lifetime\n", RETRO_TOUCH_REQUIRE_IRQ);
    return 0;
}
