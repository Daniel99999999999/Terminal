#include "xpt2046.h"
#include <stddef.h>
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if RETRO_TOUCH_ENABLE
static spi_device_handle_t s_device;
static bool s_ready, s_failed;
static xpt2046_stats_t s_stats;
static TaskHandle_t s_owner;
static uint32_t s_irq_changed, s_irq_edges, s_sampling;
static bool s_contact;

/* Not an IRAM interrupt: SDK defers it during flash/NVS operations. SPI is
 * always performed by the screen task, never by this ISR. */
static void irq_changed(void *ctx)
{
    (void)ctx;
    /* XPT2046 may pulse PENIRQ during its own conversions. Release is checked
     * after the last frame; these edges must not create a busy sampling loop. */
    if (__atomic_load_n(&s_sampling, __ATOMIC_RELAXED)) { return; }
    __atomic_store_n(&s_irq_changed, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&s_irq_edges, 1, __ATOMIC_RELAXED);
    BaseType_t wake = pdFALSE;
    vTaskNotifyGiveFromISR(s_owner, &wake);
    if (wake) { portYIELD_FROM_ISR(); }
}
/* Never put this descriptor on the stack: a timed-out transaction may still
 * be owned by the SPI ISR. It is not modified again after a timeout. */
static spi_transaction_t s_transaction;
/* Restore the 24-clock conversion frames that produced physical coordinates
 * in v0.3.1. Every frame ends with ADC power-down/PENIRQ enabled (PD0=0).
 * Inline TX/RX storage belongs to the retained static descriptor. */
static uint16_t s_adc[10];

static esp_err_t read_samples(void)
{
    static const uint8_t commands[] = {
        0xb0, 0xc0, 0xd0, 0xd0, 0xd0, 0xd0, 0x90, 0x90, 0x90, 0x90,
    };
    if (s_failed) { return ESP_ERR_INVALID_STATE; }
    for (unsigned i = 0; i < sizeof(commands); ++i) {
        s_transaction = (spi_transaction_t){
            .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
            .length = 24, .rxlength = 24, .tx_data = {commands[i], 0, 0, 0},
        };
        esp_err_t err = spi_device_queue_trans(s_device, &s_transaction, 0);
        if (err != ESP_OK) { s_failed = true; ++s_stats.io_errors; return err; }
        spi_transaction_t *result = NULL;
        err = spi_device_get_trans_result(s_device, &result, pdMS_TO_TICKS(20));
        if (err != ESP_OK || result != &s_transaction) {
            s_failed = true;
            ++s_stats.io_errors;
            return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
        }
        s_adc[i] = ((((uint16_t)result->rx_data[1] << 8) | result->rx_data[2]) >> 3) & 0x0fff;
    }
    return ESP_OK;
}

static uint16_t median(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { b = c; }
    return a > b ? a : b;
}

#endif

esp_err_t xpt2046_init(void)
{
#if RETRO_TOUCH_ENABLE
    if (s_ready || s_failed) { return ESP_ERR_INVALID_STATE; }
    esp_err_t err = gpio_set_level(TOUCH_CS_GPIO, 1);
    if (err == ESP_OK) { err = gpio_set_direction(TOUCH_CS_GPIO, GPIO_MODE_OUTPUT); }
    if (err != ESP_OK) { return err; }
    const gpio_config_t irq = {
        .pin_bit_mask = 1ULL << TOUCH_IRQ_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    err = gpio_config(&irq);
    if (err != ESP_OK) { return err; }
    const spi_bus_config_t bus = {
        .sclk_io_num = TOUCH_SPI_SCLK_GPIO, .mosi_io_num = TOUCH_SPI_MOSI_GPIO,
        .miso_io_num = TOUCH_SPI_MISO_GPIO, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 4,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) { return err; }
    const spi_device_interface_config_t device = {
        .clock_speed_hz = RETRO_TOUCH_SPI_HZ, .mode = 0,
        .spics_io_num = TOUCH_CS_GPIO, .queue_size = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &device, &s_device);
    if (err != ESP_OK) { return err; }
    err = read_samples(); /* One bootstrap to arm PENIRQ, not idle polling. */
    if (err != ESP_OK) { return err; }
    s_owner = xTaskGetCurrentTaskHandle();
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { return err; }
    err = gpio_isr_handler_add(TOUCH_IRQ_GPIO, irq_changed, NULL);
    /* Handle a panel already held down when the interrupt was installed. */
    __atomic_store_n(&s_irq_changed, 1, __ATOMIC_RELAXED);
    s_ready = err == ESP_OK;
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t xpt2046_read(xpt2046_sample_t *sample)
{
    if (!sample) { return ESP_ERR_INVALID_ARG; }
    *sample = (xpt2046_sample_t){0};
#if RETRO_TOUCH_ENABLE
    if (!s_ready || s_failed) { return ESP_ERR_INVALID_STATE; }
    if (__atomic_exchange_n(&s_irq_changed, 0, __ATOMIC_RELAXED)) {
        s_stats.last_irq_high = gpio_get_level(TOUCH_IRQ_GPIO) != 0;
        s_contact = !s_stats.last_irq_high;
    }
    if (!s_contact) { return ESP_OK; }
    sample->contact = true;
    ++s_stats.polls; ++s_stats.irq_low;
    __atomic_store_n(&s_sampling, 1, __ATOMIC_RELAXED);
    esp_err_t err = read_samples();
    __atomic_store_n(&s_sampling, 0, __ATOMIC_RELAXED);
    if (err != ESP_OK) { return err; }
    if (gpio_get_level(TOUCH_IRQ_GPIO)) {
        ++s_stats.end_high;
        s_contact = false; s_stats.last_irq_high = true; sample->contact = false;
        return ESP_OK;
    }
    uint16_t z1 = s_adc[0], z2 = s_adc[1];
    sample->pressure = z1 + 4095 - z2;
    sample->x = median(s_adc[3], s_adc[4], s_adc[5]);
    sample->y = median(s_adc[7], s_adc[8], s_adc[9]);
    s_stats.last_x = sample->x; s_stats.last_y = sample->y; s_stats.last_pressure = sample->pressure;
    s_stats.last_z1 = z1; s_stats.last_z2 = z2;
    if (!z1 || sample->pressure < 300) { ++s_stats.weak; return ESP_OK; }
    if (sample->x < 50 || sample->x > 4045 || sample->y < 50 || sample->y > 4045) {
        ++s_stats.bounds; return ESP_OK;
    }
    sample->pressed = true; ++s_stats.valid;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

void xpt2046_get_stats(xpt2046_stats_t *stats)
{
    if (!stats) { return; }
#if RETRO_TOUCH_ENABLE
    *stats = s_stats;
    stats->irq_edges = __atomic_load_n(&s_irq_edges, __ATOMIC_RELAXED);
#else
    *stats = (xpt2046_stats_t){0};
#endif
}
