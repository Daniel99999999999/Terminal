"""Verify early ESP32 PSRAM code placement in an already built firmware ELF.

Example: python tests/check_psram_iram.py .pio/build/esp32dev_spi/firmware.elf
No serial port access, flashing, or hardware changes.
"""
import argparse
from pathlib import Path
import os
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--nm", help="Xtensa nm executable")
    args = parser.parse_args()
    installed = Path(os.environ.get("USERPROFILE", str(Path.home()))) / (
        ".platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-nm.exe")
    nm = args.nm or shutil.which("xtensa-esp32-elf-nm") or str(installed)
    output = subprocess.check_output([nm, "--defined-only", str(args.elf)], text=True)
    symbols = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = (int(parts[0], 16), parts[1])
    required = ("s_psram_chip_init", "esp_psram_chip_init", "esp_psram_init", "mspi_init",
                "__wrap_esp_psram_impl_enable", "bootloader_common_vddsdio_configure",
                "bootloader_flash_get_wp_pin", "esp_log_timestamp",
                "esp_psram_impl_enable", "psram_is_32mbit_ver0", "psram_read_id",
                "psram_disable_qio_mode", "esp_gpio_reserve", "__atomic_fetch_or_8")
    for name in required:
        address, kind = symbols.get(name, (0, "?"))
        if kind.lower() != "t" or not 0x40080000 <= address < 0x400A0000:
            raise SystemExit(f"FAIL: {name} at 0x{address:08x} ({kind}) is not in ESP32 IRAM")
        print(f"IRAM {name}: 0x{address:08x}")
    for name in ("s_psram_id", "s_reserved_pin_mask", "GPIO_PIN_MUX_REG"):
        address, kind = symbols.get(name, (0, "?"))
        if kind.lower() not in ("b", "d") or not 0x3FF80000 <= address < 0x40000000:
            raise SystemExit(f"FAIL: {name} is not in internal DRAM")
    print("PASS: PSRAM startup helpers, GPIO reservation, atomic dependency and state are in internal RAM")


if __name__ == "__main__":
    main()
