/* Classic ESP32 early-boot cache barrier around MSPI/PSRAM reconfiguration.
 * Runs once before the other CPU and scheduler start, from internal RAM/ROM. */
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
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
    esp_rom_printf(DRAM_STR("MMU %s drom=%x,%x irom0=%x,%x mapped=%08x\n"), stage,
        DPORT_PRO_FLASH_MMU_TABLE[0], DPORT_PRO_FLASH_MMU_TABLE[1],
        DPORT_PRO_FLASH_MMU_TABLE[205], DPORT_PRO_FLASH_MMU_TABLE[206],
        *(volatile uint32_t *)0x3f400020);
}

esp_err_t IRAM_ATTR __wrap_esp_psram_impl_enable(void)
{
    report(DRAM_STR("before"));
    /* Direct ROM entry points: CPU1 has not been released yet. Do not use
     * runtime cache/RTOS guards whose initialization occurs later in boot. */
    extern void Cache_Read_Disable_rom(int cpu);
    extern void Cache_Flush_rom(int cpu);
    extern void Cache_Read_Enable_rom(int cpu);
    Cache_Read_Disable_rom(0);
    esp_err_t err = __real_esp_psram_impl_enable();
    Cache_Flush_rom(0);
    Cache_Read_Enable_rom(0);
    report(DRAM_STR("after"));
    return err;
}
