"""COM14 HC-05 master -> paired slave -> UART1 -> TFT model smoke test.

Requires pyserial. Does not reset ESP32, send AT commands, or use UART2.
Verifies received/parsed bytes and CRC32; visual TFT output needs observation.
"""
import argparse
import json
from pathlib import Path
import re
import time
import zlib
import serial

RX = re.compile(rb"HC05: RX=(\d+) errors=(\d+) CRC32=([0-9a-fA-F]+)")
SCREEN = re.compile(rb"SCREEN: parsed=(\d+) drop=(\d+) controls=(\d+) CRC32=([0-9a-fA-F]+)")


def open_port(name):
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=5)
    port.dtr = False
    port.rts = False
    port.port = name
    port.open()
    return port


def collect(console, peer, duration):
    log, reply = bytearray(), bytearray()
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        log.extend(console.read(max(1, console.in_waiting)))
        reply.extend(peer.read(peer.in_waiting))
    return bytes(log), bytes(reply)


def snapshot(log):
    rx, screen = RX.findall(log), SCREEN.findall(log)
    if not rx or not screen:
        return None
    r, s = rx[-1], screen[-1]
    return {"rx": int(r[0]), "errors": int(r[1]), "rx_crc": int(r[2], 16),
            "parsed": int(s[0]), "drop": int(s[1]), "controls": int(s[2]),
            "parsed_crc": int(s[3], 16)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--console", default="COM5")
    parser.add_argument("--peer", default="COM14")
    parser.add_argument("--output", type=Path, default=Path(".pio/audit/serial_smoke_v0200"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Leave a short, human-readable final screen after the sustained stream.
    split = ("Plain UART log 115200 8N1\r\n"
             "\x1b[31mRED\x1b[0m \x1b[32mGREEN\x1b[0m \x1b[34mBLUE\x1b[0m\r\n"
             "UTF-8: ąćęłńóśźż\r\n").encode("utf-8")
    burst = b"0123456789 HC05 UART1 sustained log data ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n" * 256
    final = ("\x1b[2J\x1b[HHC-05 -> UART1 -> TFT\r\n"
             "\r\nCOM14 data test\r\n"
             "\x1b[31mRED \x1b[32mGREEN \x1b[34mBLUE\x1b[0m\r\n"
             "White on black\r\nUTF-8: ąćęłńóśźż\r\n"
             "\x1b[?25l").encode("utf-8")
    payload = split + burst + final
    with open_port(args.console) as console, open_port(args.peer) as peer:
        before, old_reply = collect(console, peer, 6)
        baseline = snapshot(before)
        if baseline is None or baseline["rx"] != baseline["parsed"] or baseline["rx_crc"] != baseline["parsed_crc"]:
            (args.output / "console.txt").write_bytes(before)
            print("FAIL: no idle, running UART1/display model; no pseudo data sent")
            return 1
        for start in range(0, len(split), 1):
            peer.write(split[start:start + 1])
            time.sleep(0.005)
        if peer.write(burst + final) != len(burst + final):
            raise RuntimeError("short PC serial write")
        peer.flush()
        after, reply = collect(console, peer, 10)
    (args.output / "payload.bin").write_bytes(payload)
    (args.output / "console.txt").write_bytes(before + after)
    (args.output / "peer_rx.bin").write_bytes(old_reply + reply)
    end = snapshot(after)
    passed = False
    if end:
        expected = zlib.crc32(payload, baseline["rx_crc"])
        passed = (end["rx"] - baseline["rx"] == len(payload)
                  and end["parsed"] - baseline["parsed"] == len(payload)
                  and end["errors"] == baseline["errors"]
                  and end["drop"] == baseline["drop"]
                  and end["controls"] - baseline["controls"] >= 8
                  and end["rx_crc"] == expected == end["parsed_crc"])
    result = {"passed": passed, "sent": len(payload), "baseline": baseline,
              "final": end, "peer_reply_bytes": len(reply),
              "scope": "HC-05 UART1 RX -> model, exact byte counts/CRC32, ANSI controls; TFT image unverified"}
    (args.output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
