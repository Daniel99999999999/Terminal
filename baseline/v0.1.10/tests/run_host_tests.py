"""Compile and run production TX, LED, HCI and terminal regression scenarios.

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
    parser.add_argument("--suite", choices=("all", "bt", "led", "hci", "terminal"), default="all")
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
                        ("hci", "bt_hci_scan"), ("terminal", "terminal")):
        if args.suite not in ("all", suite):
            continue
        executable = output / ("test_" + stem + (".exe" if os.name == "nt" else ""))
        command = [compiler, *flags, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(root / "src"), str(root / ("tests/test_" + stem + ".c")),
                   str(root / ("src/" + stem + ".c")), "-o", str(executable)]
        if suite == "terminal":
            command.insert(command.index("-o"), str(root / "src/ansi_parser.c"))
        for operation in (command, [str(executable)]):
            result = subprocess.run(operation, cwd=output, capture_output=True, text=True)
            log.append(result.stdout + result.stderr)
            print(log[-1], end="")
            if result.returncode:
                (output / "result.txt").write_text("".join(log), encoding="utf-8")
                return result.returncode
    if args.suite in ("all", "hci"):
        for enabled in (1, 0):
            executable = output / ("test_bt_hci_diag_" + str(enabled) +
                                   (".exe" if os.name == "nt" else ""))
            command = [compiler, *flags, "-std=c11", "-Wall", "-Wextra", "-Werror",
                       "-DBT_HCI_SCAN_DIAG=" + str(enabled),
                       "-I" + str(root / "tests/hci_stubs"), "-I" + str(root / "src"),
                       str(root / "tests/test_bt_hci_diag.c"),
                       str(root / "src/bt_hci_diag.c"), str(root / "src/bt_hci_scan.c"),
                       "-o", str(executable)]
            for operation in (command, [str(executable)]):
                result = subprocess.run(operation, cwd=output, capture_output=True, text=True)
                log.append(result.stdout + result.stderr)
                print(log[-1], end="")
                if result.returncode:
                    (output / "result.txt").write_text("".join(log), encoding="utf-8")
                    return result.returncode
    (output / "result.txt").write_text("".join(log), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
