#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#define SPI2_HOST 1
#define SPI_DMA_DISABLED 0
#define SPI_TRANS_USE_RXDATA 4
#define SPI_TRANS_USE_TXDATA 8
typedef void *spi_device_handle_t;
typedef struct {
    int sclk_io_num, mosi_io_num, miso_io_num, quadwp_io_num, quadhd_io_num, max_transfer_sz;
} spi_bus_config_t;
typedef struct { int clock_speed_hz, mode, spics_io_num, queue_size; } spi_device_interface_config_t;
typedef struct {
    uint32_t flags;
    size_t length, rxlength;
    uint8_t tx_data[4], rx_data[4];
    const void *tx_buffer;
    void *rx_buffer;
} spi_transaction_t;
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma);
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *config, spi_device_handle_t *device);
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t *transaction, uint32_t wait);
esp_err_t spi_device_get_trans_result(spi_device_handle_t device, spi_transaction_t **transaction, uint32_t wait);
