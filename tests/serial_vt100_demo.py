"""Timed Norton Commander style VT100 demo, without reset or firmware upload.

--export writes raw ESC TXT files and escaped RealTerm commands.
Otherwise --probe is required: verify every cell using the project's C model,
then optionally send the five frames via HC-05 while capturing the console.
Buttons and menus are drawings in the UART stream, not interactive controls.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import zlib

from serial_smoke import collect, open_port, snapshot

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/vt100_demo"
COMMANDS = ROOT / "docs/VT100_DEMO_POLECENIA.txt"
VIEW = re.compile(rb"cursor=(\d+),(\d+) view=(\w+) CR=(\w+)")
ACS = dict(zip("lqkx m jtu".replace(" ", ""), "┌─┐│└┘├┤"))
COLORS = {30: 0x000000, 37: 0xaaaaaa, 44: 0x0000aa,
          46: 0x00aaaa, 47: 0xaaaaaa, 97: 0xffffff}
BLUE, BAR, WINDOW = (97, 44, False), (30, 46, False), (30, 47, False)


class Frame:
    def __init__(self, name):
        self.name = name
        self.data = bytearray(b"\x1b[?6l\x1b[r\x1b[?7l\x1b[?25l\x1b(B\x1b[0;97;44m\x1b[2J\x1b[H")
        self.cells = [[[32, 0xffffff, 0x0000aa] for _ in range(40)] for _ in range(29)]

    def put(self, row, col, text, style=BLUE, acs=False):
        assert 1 <= row <= 29 and 1 <= col and col + len(text) - 1 <= 40, (row, col, text)
        fg, bg, inverse = style
        self.data.extend(f"\x1b[{row};{col}H\x1b[0;{fg};{bg}{';7' if inverse else ''}m".encode())
        if acs:
            self.data.extend(b"\x1b(0")
        self.data.extend(text.encode("ascii"))
        if acs:
            self.data.extend(b"\x1b(B")
        for i, ch in enumerate(text):
            code = ord(ACS.get(ch, ch) if acs else ch)
            self.cells[row - 1][col - 1 + i] = [code, COLORS[fg] | (0x04000000 if inverse else 0), COLORS[bg]]

    def box(self, row, col, width, height, style=BLUE):
        self.put(row, col, "l" + "q" * (width - 2) + "k", style, True)
        for y in range(row + 1, row + height - 1):
            self.put(y, col, "x" + " " * (width - 2) + "x", style, True)
        self.put(row + height - 1, col, "m" + "q" * (width - 2) + "j", style, True)

    def finish(self):
        # Restore normal attributes and wrapping for later manual UART input.
        self.data.extend(b"\x1b(B\x1b[0m\x1b[?7h\x1b[29;1H")
        return self


def panels(name, stage):
    f = Frame(name)
    f.put(1, 1, " Panel Plik Polecenia Opcje Pomoc".ljust(40), BAR)
    f.put(2, 1, " RETRO COMMANDER - HC-05 / UART1".ljust(40))
    for col, path in ((1, "C:\\LOGI"), (21, "D:\\ARCHIWUM")):
        f.box(3, col, 20, 22)
        f.put(4, col + 1, path.ljust(18))
        f.put(5, col, "t" + "q" * 18 + "u", BLUE, True)
        f.put(6, col + 1, "Nazwa       Rozm.".ljust(18), BAR)
    left = ["..          <DIR>", "LOG_001.TXT  4096", "LOG_002.TXT  8192",
            "BOOT.LOG     2048", "CONFIG.INI    512", "README.TXT   1024"]
    right = ["..          <DIR>", "2026_11     <DIR>", "BACKUP.LOG  16384", "OLD.TXT      1024"]
    for col, entries in ((2, left), (22, right)):
        for y, entry in enumerate(entries, 7):
            f.put(y, col, entry.ljust(18), (97, 44, col == 2 and y == 8))
    f.put(23, 2, "6 plikow / katalog"[:18].ljust(18))
    f.put(23, 22, "4 pliki / katalog".ljust(18))
    f.put(26, 1, stage.ljust(40), BAR)
    f.put(27, 1, " Ramki DEC | CUP | SGR | inverse".ljust(40))
    f.put(29, 1, "1Pomoc 3Podgl 5Kopia 8Usun 10Koniec".ljust(40), BAR)
    return f


def frames():
    result = [panels("01_nc_panels", "1/5 Dwa panele i wybrany plik").finish()]
    f = panels("02_nc_menu", "2/5 Menu z podswietlonym wyborem")
    f.box(4, 7, 27, 13, WINDOW)
    f.put(4, 10, " Plik ", WINDOW)
    for row, text in enumerate(("Podglad          F3", "Edytuj           F4", "Kopiuj           F5",
                                "Przenies         F6", "Zmien nazwe", "Wlasciwosci", "Zamknij menu"), 7):
        f.put(row, 9, text.ljust(23), (30, 47, row == 9))
    result.append(f.finish())
    for name, stage, selected in (("03_dialog_ok", "3/5 Okno: zaznaczone OK", "ok"),
                                   ("04_dialog_cancel", "4/5 Okno: zaznaczone Anuluj", "cancel")):
        f = panels(name, stage)
        f.box(9, 4, 34, 12, WINDOW)
        f.put(9, 8, " Kopiowanie pliku ", WINDOW)
        f.put(12, 7, "Skopiowac LOG_001.TXT", WINDOW)
        f.put(13, 7, "do D:\\ARCHIWUM ?", WINDOW)
        f.put(15, 7, "Wybor przez SGR 7 (inverse)", WINDOW)
        f.put(18, 10, "[  OK  ]", (30, 47, selected == "ok"))
        f.put(18, 23, "[ Anuluj ]", (30, 47, selected == "cancel"))
        result.append(f.finish())
    f = panels("05_demo_done", "5/5 Koniec pokazu - ekran pozostaje")
    f.box(9, 4, 34, 12, WINDOW)
    f.put(9, 11, " Test VT100 ", WINDOW)
    f.put(12, 7, "Pokaz zakonczony", WINDOW)
    f.put(14, 7, "Ramki / menu / okno / wybor", WINDOW)
    f.put(16, 7, "Przyciski sa wzorem testowym", WINDOW)
    f.put(18, 10, "[  OK  ]", (30, 47, True))
    f.put(18, 23, "[ Anuluj ]", WINDOW)
    result.append(f.finish())
    return result


def export(data):
    FIXTURES.mkdir(parents=True, exist_ok=True)
    lines = ["POKAZ VT100 / ANSI - RetroTerminal 40 x 29", "",
             "Pliki tests/fixtures/vt100_demo/*.txt zawieraja PRAWDZIWE bajty ESC (0x1B).",
             "RealTerm: 115200, 8N1, bez flow control. Send File wysyla je bez konwersji.",
             "Wysylaj pliki 01..05, czekajac okolo 7 sekund pomiedzy ekranami.",
             "Plik TXT nie ustala opoznienia; automatyczny pokaz realizuje skrypt Python.",
             "Menu i przyciski sa obrazem testowym; nie reaguja na dotyk ani klawiature.", "",
             "Alternatywa: ponizsze pojedyncze linie do Send ASCII, Literal OFF, +CR i +LF OFF.",
             "Sekwencja \\x1b oznacza jeden bajt ESC, nie doslownie napis z ukosnikiem.",
             "Nie wysylaj calego tego pliku z opisami jako Send File.", "",
             "Pokaz automatyczny (z katalogu projektu, pyserial, skompilowany vt100_probe):",
             "python tests/serial_vt100_demo.py --probe .pio/host-tests/vt100_probe.exe --console COM5 --peer COM14 --seconds 7", ""]
    for f in data:
        (FIXTURES / (f.name + ".txt")).write_bytes(f.data)
        lines += [f.name + ":", bytes(f.data).decode("ascii").replace("\x1b", "\\x1b"), ""]
    COMMANDS.write_text("\n".join(lines), encoding="utf-8")
    print(f"Exported {len(data)} raw ESC TXT files and {COMMANDS}", flush=True)


def verify(data, probe, out):
    results = []
    for f in data:
        path = out / (f.name + ".txt")
        path.write_bytes(f.data)
        run = subprocess.run([str(probe.resolve()), str(path), str(out / (f.name + ".ppm"))],
                             capture_output=True, text=True, check=True)
        model = json.loads(run.stdout)
        differences = [[x, y, f.cells[y][x], model["cells"][y][x]]
                       for y in range(29) for x in range(40) if f.cells[y][x] != model["cells"][y][x]]
        passed = not differences and model["cursor"] == [0, 28] and not model["visible"] and model["active"] == 0
        results.append({"frame": f.name, "passed": passed, "bytes": len(f.data),
                        "checked_cells": 1160, "controls": model["controls"],
                        "cursor": model["cursor"], "differences": differences})
    (out / "model.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    if not all(r["passed"] for r in results):
        raise RuntimeError("Frame model check failed; see model.json; no serial data sent")
    print("PASS host model: 5 frames, 5800 cells with colors/attributes, cursor and page", flush=True)
    return results


def play(args, data, models, out):
    log, replies, sent, stages = bytearray(), bytearray(), bytearray(), []
    try:
        with open_port(args.console) as console, open_port(args.peer) as peer:
            print(f"OPEN {args.console} + {args.peer}; capture active, checking baseline", flush=True)
            chunk, reply = collect(console, peer, 6)
            log.extend(chunk); replies.extend(reply)
            base = snapshot(chunk); view = VIEW.findall(chunk)
            if (not base or not view or view[-1][2:] != (b"LIVE", b"NL") or
                    base["rx"] != base["parsed"] or base["rx_crc"] != base["parsed_crc"]):
                raise RuntimeError("No idle LIVE CR=NL baseline; no demo bytes sent")
            expected_controls = base["controls"]
            for i, (f, model) in enumerate(zip(data, models), 1):
                print(f"SCREEN {i}/5: {f.name} ({args.seconds:g} s)", flush=True)
                if peer.write(f.data) != len(f.data):
                    raise RuntimeError("Short serial write")
                peer.flush(); sent.extend(f.data)
                chunk, reply = collect(console, peer, args.seconds)
                log.extend(chunk); replies.extend(reply)
                end = snapshot(chunk); view = VIEW.findall(chunk)
                expected_controls += model["controls"]
                crc = zlib.crc32(sent, base["rx_crc"])
                passed = bool(end and view and end["rx"] - base["rx"] == len(sent)
                              and end["parsed"] - base["parsed"] == len(sent)
                              and end["errors"] == base["errors"] and end["drop"] == base["drop"]
                              and end["rx_crc"] == crc == end["parsed_crc"]
                              and end["controls"] == expected_controls
                              and tuple(map(int, view[-1][:2])) == (0, 28)
                              and view[-1][2:] == (b"LIVE", b"NL"))
                stages.append({"frame": f.name, "passed": passed, "sent_total": len(sent), "snapshot": end})
                if not passed:
                    raise RuntimeError(f"Transport/model check failed after {f.name}")
            restart = any(token in log for token in (b"rst:0x", b"Guru Meditation", b"abort()", b"Brownout"))
            passed = not restart and not replies
            result = {"passed": passed, "frames": stages, "sent": len(sent), "baseline": base,
                      "seconds_per_frame": args.seconds, "unexpected_reply_bytes": len(replies),
                      "restart_or_panic": restart, "firmware_upload": False,
                      "physical_display": "requires visual confirmation"}
            (out / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            print(json.dumps(result, indent=2), flush=True)
            return 0 if passed else 1
    finally:
        (out / "console.txt").write_bytes(log)
        (out / "sent.txt").write_bytes(sent)
        (out / "peer_rx.bin").write_bytes(replies)
        print("CLOSED serial ports; capture saved", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--export", action="store_true")
    p.add_argument("--probe", type=Path)
    p.add_argument("--model-only", action="store_true")
    p.add_argument("--console", default="COM5")
    p.add_argument("--peer", default="COM14")
    p.add_argument("--seconds", type=float, default=7)
    p.add_argument("--output", type=Path, default=ROOT / ".pio/audit/v0307_vt100_demo")
    args = p.parse_args()
    data = frames()
    if args.export:
        export(data)
        return 0
    if not args.probe:
        p.error("--probe is required unless --export")
    if args.seconds < 6:
        p.error("--seconds must be >= 6 for the 5-second firmware statistics")
    args.output.mkdir(parents=True, exist_ok=True)
    models = verify(data, args.probe, args.output)
    return 0 if args.model_only else play(args, data, models, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
