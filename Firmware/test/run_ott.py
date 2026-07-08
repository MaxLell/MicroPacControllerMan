#!/usr/bin/env python3
"""
Drive an On-Target Test over the ST-LINK serial console and report PASS/FAIL.

Sends `ott <test>` to the board, waits for `OTT PASSED [<test>]` /
`OTT FAILED [<test>]: <reason>`, and exits 0 (pass) / 1 (fail) / 2 (timeout).
Stdlib only — no pyserial required.

    ./run_ott.py                 # runs 'blinky' on /dev/ttyACM0
    ./run_ott.py blinky --port /dev/ttyACM0
"""
import argparse
import os
import select
import subprocess
import sys
import time


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("test", nargs="?", default="blinky")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", default="115200")
    ap.add_argument("--timeout", type=float, default=5.0)
    args = ap.parse_args()

    subprocess.run(
        ["stty", "-F", args.port, args.baud, "raw", "-echo", "-echoe", "-echok", "-crtscts"],
        check=True,
    )

    passed = f"OTT PASSED [{args.test}]"
    failed = f"OTT FAILED [{args.test}]"
    error = f"OTT ERROR: unknown test '{args.test}'"

    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY)
    try:
        os.write(fd, f"ott {args.test}\r\n".encode())
        deadline = time.monotonic() + args.timeout
        text = ""
        while time.monotonic() < deadline:
            r, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
            if not r:
                break
            text += os.read(fd, 256).decode(errors="replace")
            if passed in text:
                print(f"PASS: {args.test}")
                return 0
            if failed in text:
                for line in text.splitlines():
                    if line.startswith(failed):
                        print(line)
                        break
                return 1
            if error in text:
                print(error)
                return 1
        sys.stderr.write(text)
        print(f"TIMEOUT: no result for '{args.test}' within {args.timeout}s")
        return 2
    finally:
        os.close(fd)


if __name__ == "__main__":
    sys.exit(main())
