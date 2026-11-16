"""Compile/run terminal, HC-05, display, XPT2046 and archived BT regressions.

Usage: python tests/run_host_tests.py [--cc path/to/a/C/compiler]
Generated files stay in .pio/host-tests. No ESP32 connection is needed.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", help="C compiler (GCC, Clang, or bcc64x)")
    parser.add_argument("--suite", choices=("all", "bt", "led", "hci", "terminal", "hc05", "raster", "profiles", "xpt", "calibration", "lcd"), default="all")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / ".pio" / "host-tests"
    output.mkdir(parents=True, exist_ok=True)
    bcc = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / (
        "Embarcadero/Studio/23.0/bin64/bcc64x.exe"
    )
    compiler = args.cc or (str(bcc) if os.name == "nt" and bcc.exists() else None)
    compiler = compiler or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        parser.error("No C compiler found; pass --cc /path/to/compiler")
    flags = ["-tWC"] if Path(compiler).stem.lower() == "bcc64x" else []
    log = []
    for suite, stem in (("bt", "bt_tx_state"), ("led", "debug_led_logic"),
                        ("hci", "bt_hci_scan"), ("terminal", "terminal"),
                        ("hc05", "hc05_link"), ("raster", "terminal_raster")):
        if args.suite not in ("all", suite):
            continue
        source = root / ("baseline/v0.1.10/src" if suite in ("bt", "hci") else "src")
        if suite in ("bt", "hci"):
            print("Archived v0.1.10 regression (excluded from current firmware): " + suite)
        executable = output / ("test_" + stem + (".exe" if os.name == "nt" else ""))
        command = [compiler, *flags, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(source), str(root / ("tests/test_" + stem + ".c")),
                   str(source / (stem + ".c")), "-o", str(executable)]
        if suite == "terminal":
            command.insert(command.index("-o"), str(root / "src/ansi_parser.c"))
        if suite == "raster":
            command[command.index("-o"):command.index("-o")] = [
                str(root / "src/terminal.c"), str(root / "src/ansi_parser.c")]
        for operation in (command, [str(executable)]):
            result = subprocess.run(operation, cwd=output, capture_output=True, text=True)
            log.append(result.stdout + result.stderr)
            print(log[-1], end="")
            if result.returncode:
                (output / "result.txt").write_text("".join(log), encoding="utf-8")
                return result.returncode
    if args.suite in ("all", "hci"):
        source = root / "baseline/v0.1.10/src"
        for enabled in (1, 0):
            executable = output / ("test_bt_hci_diag_" + str(enabled) +
                                   (".exe" if os.name == "nt" else ""))
            command = [compiler, *flags, "-std=c11", "-Wall", "-Wextra", "-Werror",
                       "-DBT_HCI_SCAN_DIAG=" + str(enabled),
                       "-I" + str(root / "tests/hci_stubs"), "-I" + str(source),
                       str(root / "tests/test_bt_hci_diag.c"),
                       str(source / "bt_hci_diag.c"), str(source / "bt_hci_scan.c"),
                       "-o", str(executable)]
            for operation in (command, [str(executable)]):
                result = subprocess.run(operation, cwd=output, capture_output=True, text=True)
                log.append(result.stdout + result.stderr)
                print(log[-1], end="")
                if result.returncode:
                    (output / "result.txt").write_text("".join(log), encoding="utf-8")
                    return result.returncode
    extra = []
    if args.suite in ("all", "lcd"):
        extra.append(("lcd_startup", [root / "tests/test_lcd_startup.c", root / "src/lcd_panel.c",
            root / "vendor/esp_lcd_ili9341/esp_lcd_ili9341.c", root / "src/terminal_raster.c",
            root / "src/terminal.c", root / "src/ansi_parser.c"],
            ["-DRETRO_TFT_PROFILE=2", "-DESP_LCD_ILI9341_VER_MAJOR=2",
             "-DESP_LCD_ILI9341_VER_MINOR=1", "-DESP_LCD_ILI9341_VER_PATCH=0"],
            [root / "tests/lcd_stubs", root / "vendor/esp_lcd_ili9341/include"]))
    if args.suite in ("all", "profiles"):
        for profile in (0, 1, 2):
            extra.append(("profile_" + str(profile),
                          [root / "tests/test_display_profile.c"],
                          ["-DRETRO_TFT_PROFILE=" + str(profile)], []))
    if args.suite in ("all", "xpt"):
        for require_irq in (0, 1):
            extra.append(("xpt2046_" + str(require_irq),
                          [root / "tests/test_xpt2046.c", root / "src/xpt2046.c"],
                          ["-DRETRO_TFT_PROFILE=2", "-DRETRO_TOUCH_ENABLE=1",
                           "-DRETRO_TOUCH_REQUIRE_IRQ=" + str(require_irq)], [root / "tests/spi_stubs"]))
    if args.suite in ("all", "calibration"):
        extra.append(("touch_calibration", [root / "tests/test_touch_calibration.c",
            root / "src/touch_calibration.c", root / "src/touch_ui.c",
            root / "src/terminal_raster.c", root / "src/terminal.c", root / "src/ansi_parser.c"],
            ["-DRETRO_TFT_PROFILE=2", "-DRETRO_TOUCH_ENABLE=1"],
            [root / "tests/touch_ui_stubs", root / "tests/spi_stubs"]))
    for name, sources, defines, includes in extra:
        executable = output / ("test_" + name + (".exe" if os.name == "nt" else ""))
        command = [compiler, *flags, "-std=c11", "-Wall", "-Wextra", "-Werror", *defines,
                   *["-I" + str(p) for p in includes], "-I" + str(root / "src"),
                   *[str(p) for p in sources], "-o", str(executable)]
        if name == "touch_calibration" and Path(compiler).stem.lower() != "bcc64x":
            command.append("-lm")
        operations = [command, [str(executable)]]
        if name == "lcd_startup":
            operations.extend(([str(executable), "init_error"], [str(executable), "dma_timeout"]))
        for operation in operations:
            result = subprocess.run(operation, cwd=output, capture_output=True, text=True)
            log.append(result.stdout + result.stderr)
            print(log[-1], end="")
            if result.returncode:
                (output / "result.txt").write_text("".join(log), encoding="utf-8")
                return result.returncode
    if args.suite in ("all", "profiles"):
        for defines, message in ((["-DRETRO_TFT_PROFILE=3"], "Unknown RETRO_TFT_PROFILE"),
                                 (["-DRETRO_TFT_PROFILE=1", "-DRETRO_TOUCH_ENABLE=1"], "require the SPI pin plan"),
                                 (["-DRETRO_TFT_ROTATION=4"], "must be 0..3")):
            command = [compiler, *flags, "-std=c11", *defines, "-I" + str(root / "src"),
                       "-E", str(root / "tests/test_display_profile.c")]
            result = subprocess.run(command, cwd=output, capture_output=True, text=True)
            if not result.returncode or message not in result.stdout + result.stderr:
                log.append("FAIL: configuration guard did not reject " + repr(defines) + "\n")
                (output / "result.txt").write_text("".join(log), encoding="utf-8")
                return 1
            log.append("PASS: rejected invalid configuration " + " ".join(defines) + "\n")
            print(log[-1], end="")
    (output / "result.txt").write_text("".join(log), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
