/* Keep the bootloader's flash CS timing across shared-bus PSRAM setup.
 * SDK chip detection/commands/clocks/pins are unchanged. Early boot only. */
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "soc/soc.h"
#include "soc/spi_reg.h"

esp_err_t __real_esp_psram_impl_enable(void);

esp_err_t IRAM_ATTR __wrap_esp_psram_impl_enable(void)
{
    uint32_t before = REG_READ(SPI_CTRL2_REG(0));
    esp_err_t err = __real_esp_psram_impl_enable();
    uint32_t after = REG_READ(SPI_CTRL2_REG(0));
    uint32_t mask = (SPI_HOLD_TIME_V << SPI_HOLD_TIME_S) |
                    (SPI_SETUP_TIME_V << SPI_SETUP_TIME_S);
    WRITE_PERI_REG(SPI_CTRL2_REG(0), (after & ~mask) | (before & mask));
    esp_rom_printf(DRAM_STR("FLASH CS timing before=%08x psram=%08x restored=%08x\n"),
        before, after, REG_READ(SPI_CTRL2_REG(0)));
    return err;
}
