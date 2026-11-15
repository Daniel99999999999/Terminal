"""VT100/ANSI regression on the deployed 40x29, CR=NL firmware.

Compile tests/vt100_probe.c first; pass its executable with --probe.
--model-only verifies explicit cell/color expectations without serial I/O.
Hardware checks exact RX/model bytes, CRC, command count and cursor after
every case. It cannot read back physical TFT pixels or remote model cells.
No reset, firmware upload, AT commands, native radio or UART2 are used.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time
import zlib
from serial_smoke import collect, open_port, snapshot

RESET = b"\x1bc"
VIEW = re.compile(rb"cursor=(\d+),(\d+) view=(\w+) CR=(\w+)")


def cup(row, col=1):
    return f"\x1b[{row};{col}H".encode("ascii")


def expected(x, y, text, fg=None, bg=None):
    return [(x+i, y, ord(ch), fg, bg) for i, ch in enumerate(text)]


def cases():
    fill = b"".join(cup(y+1) + bytes([65+y]) for y in range(5))
    data = [
        ("TAB_BS", [RESET+b"AB\tX\bY"], (9, 0), expected(0,0,"AB")+expected(8,0,"Y")),
        ("CUP_relative", [RESET+cup(6,10)+b"\x1b[2A\x1b[3DX\x1b[4B\x1b[2CY"], (10,7),
         expected(6,3,"X")+expected(9,7,"Y")),
        ("ED_EL", [RESET+b"JUNK"+cup(5,5)+b"\x1b[2JABCDEF"+cup(5,7)+b"\x1b[K"], (6,4),
         expected(0,0,"    ")+expected(4,4,"AB    ")),
        ("DECSC_DECRC", [RESET+b"\x1b[31mA\x1b7"+cup(10,20)+b"\x1b[32mX\x1b8B"], (2,0),
         expected(0,0,"AB",0xaa0000)+expected(19,9,"X",0x00aa00)),
        ("CSI_save_restore", [RESET+cup(3,4)+b"\x1b[s"+cup(20,10)+b"Z\x1b[uC"], (4,2),
         expected(3,2,"C")+expected(9,19,"Z")),
        ("DECAWM_wrap", [RESET+b"A"*40+b"\x1b[32mB"], (1,1),
         expected(0,0,"A"*40)+expected(0,1,"B",0x00aa00)),
        ("DECAWM_nowrap", [RESET+b"\x1b[?7l"+cup(2,40)+b"XY"], (39,1), expected(38,1," Y")),
        ("DECSTBM_IND_RI", [RESET+fill+b"\x1b[2;4r"+cup(4)+b"\x1bD"+cup(2)+b"\x1bMX"], (1,1),
         sum((expected(0,y,ch) for y,ch in enumerate("AXCDE")), [])),
        ("DECOM_origin", [RESET+b"\x1b[4;8r\x1b[?6h"+cup(99,3)+b"X"], (3,7), expected(2,7,"X")),
        ("ICH_DCH", [RESET+b"ABCDEF"+cup(1,3)+b"\x1b[2@XY\x1b[2P"], (4,0), expected(0,0,"ABXYEF  ")),
        ("IL_DL", [RESET+fill+b"\x1b[2;4r"+cup(3)+b"\x1b[LX\x1b[M"], (1,2),
         sum((expected(0,y,ch) for y,ch in enumerate("ABC E")), [])),
        ("LNM", [RESET+b"AB\x1b[20h\nC\x1b[20l\nD"], (2,2), expected(0,1,"C")+expected(1,2,"D")),
        ("alternate_1049", [RESET+b"\x1b[31mMAIN\x1b[?1049hALT"+cup(10,10)+b"X\x1b[?1049lB"], (5,0),
         expected(0,0,"MAINB",0xaa0000)+expected(9,9," ")),
        ("SGR_16_256_RGB_attributes", [RESET+b"\x1b[31;44;1;4mA\x1b[0mB\x1b[38;5;196mC"
         b"\x1b[38;2;1;2;3;48;2;4;5;6mD\x1b[0m\x1b[7mE\x1b[27mF"], (6,0),
         expected(0,0,"A",0x03aa0000,0x0000aa)+expected(1,0,"B",0xffffff,0)+
         expected(2,0,"C",0xff0000,0)+expected(3,0,"D",0x010203,0x040506)+
         expected(4,0,"E",0x04ffffff,0)+expected(5,0,"F",0xffffff,0)),
        ("DEC_G0_G1_SO_SI", [RESET+b"\x1b(0lqk\x1b(BQ\x1b)0\x0ex\x0fR"], (6,0), expected(0,0,"┌─┐Q│R")),
        ("OSC_DCS_cancel_overlong", [RESET+b"A\x1b]0;HIDDEN\aB\x1bPHIDDEN\x1b\\C\x1b[31\x18D"
         b"\x1b[38:2::255:0:0mE\x1b["+b"1"*200+b"mF"], (6,0), expected(0,0,"ABCDEF",0xffffff,0)),
        ("fragmented_ANSI_UTF8", [bytes([b]) for b in RESET+cup(3,4)+b"\x1b[32m"+"żółć".encode()+b"\x1b[0m"],
         (7,2), expected(3,2,"żółć",0x00aa00)),
        ("burst_ANSI", [RESET+(cup(8,5)+b"\x1b[2K\x1b[31mRED\x1b[32mGREEN\x1b[0m")*256+cup(6)+b"BURST"],
         (5,5), expected(0,5,"BURST")+expected(4,7,"RED",0xaa0000)+expected(7,7,"GREEN",0x00aa00)),
        ("unsupported_DSR_ignored", [RESET+b"A\x1b[6nB"], (2,0), expected(0,0,"AB")),
    ]
    # Final screen: the commands themselves must produce the expected words.
    visual = RESET+b"\x1b[?25l\x1b[96mVT100 / ANSI - v0.3.1\x1b[0m"
    visual += cup(3)+b"Kursor: [XX]"+cup(3,10)+b"OK"
    visual += cup(4)+b"Kasowanie: OK JUNK"+cup(4,14)+b"\x1b[K"
    visual += cup(5)+b"Zapis pozycji: \x1b7"+cup(28,35)+b"\x1b8OK"
    visual += cup(7)+b"Kolory ANSI:"+cup(8)
    for n,label in enumerate((b"RED ",b"GREEN ",b"YELLOW ",b"BLUE ",b"MAGENTA ",b"CYAN"),1):
        visual += f"\x1b[{30+n}m".encode()+label
    visual += b"\x1b[0m"+cup(10)+b"Normal \x1b[1mBold\x1b[0m \x1b[4mUnder\x1b[0m \x1b[7mInverse\x1b[0m"
    visual += cup(12)+b"Miganie: \x1b[5mBLINK\x1b[0m"
    visual += cup(14)+b"Ramka DEC (ciagla):\x1b(0"
    visual += cup(15,3)+b"l"+b"q"*28+b"k"
    visual += cup(16,3)+b"x\x1b(B  HC-05 -> UART1 -> TFT\x1b(0"+cup(16,32)+b"x"
    visual += cup(17,3)+b"m"+b"q"*28+b"j\x1b(B"
    visual += cup(19)+"UTF-8: ąćęłńóśźż".encode()
    visual += cup(21)+b"256: \x1b[38;5;196mRED \x1b[38;5;46mGREEN \x1b[38;5;21mBLUE\x1b[0m"
    visual += cup(22)+b"RGB: \x1b[38;2;255;0;0mRED \x1b[38;2;0;255;0mGREEN \x1b[38;2;0;0;255mBLUE\x1b[0m"
    visual += cup(25)+b"CR/CRLF: nowa linia dziala"
    visual += cup(27)+b"COM14 test - koniec"
    checks = expected(0,2,"Kursor: [OK]")+expected(0,3,"Kasowanie: OK     ")+expected(0,4,"Zapis pozycji: OK")
    checks += expected(0,18,"UTF-8: ąćęłńóśźż")+expected(2,14,"┌"+"─"*28+"┐")
    checks += expected(0,26,"COM14 test - koniec")
    data.append(("visual_pattern", [visual], (19,26), checks))
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--model-only", action="store_true")
    parser.add_argument("--console", default="COM5")
    parser.add_argument("--peer", default="COM14")
    parser.add_argument("--output", type=Path, default=Path(".pio/audit/v031_vt100"))
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    results, prepared = [], []
    for name, chunks, cursor, checks in cases():
        payload = b"".join(chunks); path = out/(name+".bin"); path.write_bytes(payload)
        command = [str(args.probe.resolve()), str(path)]
        if name == "visual_pattern": command.append(str(out/"expected.ppm"))
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        model = json.loads(result.stdout)
        failures = []
        if tuple(model["cursor"]) != cursor: failures.append("cursor")
        if model["active"] != 0: failures.append("active page")
        if model["visible"] != (name != "visual_pattern"): failures.append("cursor visibility")
        for x,y,ch,fg,bg in checks:
            actual = model["cells"][y][x]
            if actual[0] != ch or (fg is not None and actual[1] != fg) or (bg is not None and actual[2] != bg):
                failures.append({"cell":[x,y],"expected":[ch,fg,bg],"actual":actual})
        results.append({"case":name,"model_passed":not failures,"expected_cursor":cursor,
                        "model_cursor":model["cursor"],"controls":model["controls"],"failures":failures})
        prepared.append((name,chunks,cursor,model["controls"]))
    (out/"model.json").write_text(json.dumps(results,indent=2),encoding="utf-8")
    if not all(r["model_passed"] for r in results):
        print(json.dumps([r for r in results if not r["model_passed"]],indent=2)); return 1
    print(f"PASS host model: {len(results)} cases, explicit cells/colors/cursors; one-byte input",flush=True)
    if args.model_only: return 0
    log, sent = bytearray(), bytearray(); checks = []
    # Context managers always close both ports, including failures.
    with open_port(args.console) as console, open_port(args.peer) as peer:
        before,_ = collect(console,peer,6); log.extend(before); baseline=snapshot(before)
        v=VIEW.findall(before)
        if not baseline or not v or v[-1][2:] != (b"LIVE",b"NL") or baseline["rx"] != baseline["parsed"] or baseline["rx_crc"] != baseline["parsed_crc"]:
            (out/"console.txt").write_bytes(log); print("No idle LIVE CR=NL baseline; no test bytes sent"); return 1
        controls = baseline["controls"]
        for name,chunks,cursor,count in prepared:
            for chunk in chunks:
                if peer.write(chunk) != len(chunk): raise RuntimeError("short serial write")
                sent.extend(chunk)
                if len(chunks)>1: time.sleep(0.015)
            peer.flush()
            after,reply=collect(console,peer,5.6); log.extend(after); end=snapshot(after); v=VIEW.findall(after)
            controls += count
            crc=zlib.crc32(sent,baseline["rx_crc"])
            passed=bool(end and v and tuple(map(int,v[-1][:2]))==cursor and v[-1][2:]==(b"LIVE",b"NL")
                and end["rx"]-baseline["rx"]==len(sent) and end["parsed"]-baseline["parsed"]==len(sent)
                and end["rx_crc"]==crc==end["parsed_crc"] and end["controls"]==controls
                and end["errors"]==baseline["errors"] and end["drop"]==baseline["drop"] and not reply)
            checks.append({"case":name,"passed":passed,"expected_cursor":cursor,
                "observed_cursor":list(map(int,v[-1][:2])) if v else None,"expected_controls":controls,
                "snapshot":end,"peer_reply_bytes":len(reply)})
            print(name,"PASS" if passed else "FAIL",flush=True)
            (out/"console.txt").write_bytes(log)
            (out/"sent.bin").write_bytes(sent)
            (out/"result.json").write_text(json.dumps({"passed":all(c["passed"] for c in checks),
                "complete":len(checks)==len(prepared),"firmware":"v0.3.1 esp32dev_internal",
                "firmware_source":"0805f57","baseline":baseline,"sent":len(sent),"cases":checks,
                "unsupported":["DSR reply: intentionally absent; negative test checks safe consumption"],
                "scope":"Host: explicit cells/attributes. ESP32: cursor/commands/bytes/CRC. Physical pixels unverified."},indent=2),encoding="utf-8")
            if not passed: return 1
    print(f"PASS ESP32: {len(checks)} cases, {len(sent)} bytes; ports closed",flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
