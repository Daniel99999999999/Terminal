/* Temporary register evidence around the SDK PSRAM initialization.
 * Runs before app_main, exclusively from internal RAM / ROM. */
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_rom_spiflash.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/spi_reg.h"
#include "soc/io_mux_reg.h"

esp_err_t __real_esp_psram_impl_enable(void);

static void IRAM_ATTR report(const char *stage)
{
    esp_rom_printf(DRAM_STR("MSPI %s ctrl=%08x user=%08x user1=%08x pin=%08x clk=%08x mux6=%08x\n"),
        stage, REG_READ(SPI_CTRL_REG(0)), REG_READ(SPI_USER_REG(0)),
        REG_READ(SPI_USER1_REG(0)), REG_READ(SPI_PIN_REG(0)),
        REG_READ(SPI_CLOCK_REG(0)), REG_READ(PERIPHS_IO_MUX_SD_CLK_U));
    esp_rom_printf(DRAM_STR("MMU %s drom=%x,%x irom=%x,%x,%x mapped=%08x\n"), stage,
        DPORT_PRO_FLASH_MMU_TABLE[0], DPORT_PRO_FLASH_MMU_TABLE[1],
        DPORT_PRO_FLASH_MMU_TABLE[64], DPORT_PRO_FLASH_MMU_TABLE[65],
        DPORT_PRO_FLASH_MMU_TABLE[66], *(volatile uint32_t *)0x3f400020);
}

esp_err_t IRAM_ATTR __wrap_esp_psram_impl_enable(void)
{
    report(DRAM_STR("before"));
    esp_err_t err = __real_esp_psram_impl_enable();
    report(DRAM_STR("after"));
    uint32_t data[4] = {0};
    esp_rom_spiflash_result_t read = esp_rom_spiflash_read(0x10000, data, sizeof(data));
    esp_rom_printf(DRAM_STR("FLASH physical read=%d words=%08x,%08x,%08x,%08x\n"),
                   read, data[0], data[1], data[2], data[3]);
    return err;
}
