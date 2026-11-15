"""Send pseudo UART data via the PC HC-05 and observe the ESP32 bridge.

Requires pyserial. Does not reset ESP32 or change HC-05 AT configuration.
COM14 -> radio -> UART1 tests UART1 RX and forwarding into UART2 TX FIFO.
It cannot verify TFT content (UART2 RX) or bytes at an unobserved UART2 pin.
"""
import argparse
import json
from pathlib import Path
import re
import time

import serial


REPORT = re.compile(rb"UART1->UART2 RX=(\d+) TX_fifo=(\d+) drop=(\d+) boundary=(\d+) errors=(\d+)")


def open_port(name):
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=2)
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--console", default="COM5")
    parser.add_argument("--peer", default="COM14")
    parser.add_argument("--output", type=Path, default=Path(".pio/audit/serial_smoke"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    payload = (b"RETROTERM_TEST plain UART 115200 8N1\r\n"
               b"\x1b[31mRED\x1b[0m \x1b[32mGREEN\x1b[0m \x1b[34mBLUE\x1b[0m\r\n"
               b"VT100 split commands and UTF-8: \xc4\x85\xc4\x87\xc4\x99\xc5\x82\r\n"
               b"RETROTERM_TEST END\r\n")
    with open_port(args.console) as console, open_port(args.peer) as peer:
        before, old_reply = collect(console, peer, 6)
        samples = REPORT.findall(before)
        if not samples:
            (args.output / "console.txt").write_bytes(before)
            print("FAIL: no running bridge report on console; no pseudo data sent")
            return 1
        baseline = tuple(map(int, samples[-1]))
        # Split ESC, CSI and UTF-8 across writes; no changes to the bridge bytes.
        for start in range(0, len(payload), 7):
            block = payload[start:start + 7]
            if peer.write(block) != len(block):
                raise RuntimeError("short PC serial write")
            time.sleep(0.02)
        after, reply = collect(console, peer, 7)
    (args.output / "payload.bin").write_bytes(payload)
    (args.output / "console.txt").write_bytes(before + after)
    (args.output / "peer_rx.bin").write_bytes(old_reply + reply)
    samples = REPORT.findall(after)
    final = tuple(map(int, samples[-1])) if samples else baseline
    delta = tuple(b - a for a, b in zip(baseline, final))
    passed = delta == (len(payload), len(payload), 0, 0, 0)
    result = {"passed": passed, "sent": len(payload), "baseline": baseline,
              "final": final, "delta_rx_tx_drop_boundary_errors": delta,
              "peer_reply_bytes": len(reply), "exact_echo": reply == payload,
              "scope": "HC-05 UART1 RX and UART2 TX FIFO counters; TFT/UART2 wire bytes unverified"}
    (args.output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
