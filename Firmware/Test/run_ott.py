#!/usr/bin/env python3
"""
Drive the On-Target Tests (OTT) over the ST-LINK serial console.

Two ways to use it:

  ./run_ott.py --suite                 # run the AUTOMATIC regression suite
  ./run_ott.py blinky                  # run one automatic test
  ./run_ott.py touchpad                # run one INTERACTIVE test (streams live,
  ./run_ott.py display                 #   long timeout, you confirm on the board
  ./run_ott.py touchdot                #   and press the USER button to finish)

The automatic suite covers the Board-Bring-Up tests a machine can judge on its own:
  VT-INT-001  Power-On & Enumeration   (the VCP device node exists)
  VT-INT-002  Serial Console Output    (`reset` re-emits the known boot banner)
  VT-INT-005  Blinky                   (`ott blinky` -> OTT PASSED)

The display/touchpad tests (VT-INT-006/007) are interactive by design — the
firmware renders/prints and waits for you to confirm with the USER button — so
they are excluded from --suite and streamed live instead.

Stdlib only — no pyserial required. Exit 0 = all pass, 1 = fail, 2 = timeout.
"""
import argparse
import glob
import os
import select
import subprocess
import sys
import time

BANNER = "MicroPacControllerMan booted"
INTERACTIVE = {"touchpad", "display", "touchdot"}
SUITE_AUTOMATIC = ["blinky"]  # enumeration + banner are checked separately below


def detect_port() -> str:
    """Find the ST-LINK virtual COM port, so the tty number (ACM0/ACM1/…) and any
    other USB serial devices don't matter. Falls back to the first ttyACM."""
    # Stable name exposed by the ST-LINK VCP interface (…STLINK…-if02).
    for pat in ("/dev/serial/by-id/*STLINK*if02*",
                "/dev/serial/by-id/*STLINK*",
                "/dev/serial/by-id/*STM*"):
        hits = sorted(glob.glob(pat))
        if hits:
            return os.path.realpath(hits[0])
    acm = sorted(glob.glob("/dev/ttyACM*"))
    if acm:
        return acm[0]
    return "/dev/ttyACM0"


def configure_tty(port: str, baud: str) -> None:
    subprocess.run(
        ["stty", "-F", port, baud, "raw", "-echo", "-echoe", "-echok", "-crtscts"],
        check=True,
    )


def read_until(fd: int, needles, timeout: float, echo: bool = False) -> "tuple[str|None, str]":
    """Read until one of `needles` (list of substrings) appears or timeout.
    Returns (matched_needle_or_None, full_text). Echoes bytes live if echo=True."""
    deadline = time.monotonic() + timeout
    text = ""
    while time.monotonic() < deadline:
        r, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
        if not r:
            break
        chunk = os.read(fd, 256).decode(errors="replace")
        if echo:
            sys.stdout.write(chunk)
            sys.stdout.flush()
        text += chunk
        for n in needles:
            if n in text:
                return n, text
    return None, text


def run_single(port: str, baud: str, test: str, timeout: float) -> int:
    interactive = test in INTERACTIVE
    if timeout is None:
        timeout = 130.0 if interactive else 8.0

    configure_tty(port, baud)
    passed = f"OTT PASSED [{test}]"
    failed = f"OTT FAILED [{test}]"
    unknown = f"OTT ERROR: unknown test '{test}'"

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    try:
        if interactive:
            print(f"--- {test}: interactive; follow the prompts on the board, "
                  f"press the USER button (B1) to finish ---")
        os.write(fd, f"ott {test}\r\n".encode())
        match, text = read_until(fd, [passed, failed, unknown], timeout, echo=interactive)
        if match == passed:
            print(f"\nPASS: {test}")
            return 0
        if match == failed:
            for line in text.splitlines():
                if line.startswith(failed):
                    print(f"\n{line}")
                    break
            return 1
        if match == unknown:
            print(unknown)
            return 1
        if not interactive:
            sys.stderr.write(text)
        print(f"\nTIMEOUT: no result for '{test}' within {timeout}s")
        return 2
    finally:
        os.close(fd)


def check_enumeration(port: str) -> bool:
    ok = os.path.exists(port)
    print(f"[VT-INT-001] enumeration: {port} {'present' if ok else 'MISSING'}")
    return ok


def check_banner(port: str, baud: str, timeout: float = 5.0) -> bool:
    configure_tty(port, baud)
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    try:
        os.write(fd, b"reset\r\n")
        match, _ = read_until(fd, [BANNER], timeout)
        ok = match is not None
        print(f"[VT-INT-002] boot banner: {'seen' if ok else 'NOT SEEN'}")
        return ok
    finally:
        os.close(fd)


def run_suite(port: str, baud: str) -> int:
    print("=== OTT automatic regression suite ===")
    results = []

    results.append(("VT-INT-001 enumeration", check_enumeration(port)))
    if not results[-1][1]:
        print("Board not enumerated; aborting suite.")
        return 1

    results.append(("VT-INT-002 boot banner", check_banner(port, baud)))

    for test in SUITE_AUTOMATIC:
        rc = run_single(port, baud, test, timeout=8.0)
        results.append((f"ott {test}", rc == 0))

    print("\n--- summary ---")
    all_ok = True
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
        all_ok = all_ok and ok
    return 0 if all_ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("test", nargs="?", default=None,
                    help="test name (blinky/touchpad/display/touchdot); omit with --suite")
    ap.add_argument("--suite", action="store_true", help="run the automatic regression suite")
    ap.add_argument("--port", default=None, help="serial port (default: auto-detect the ST-LINK VCP)")
    ap.add_argument("--baud", default="115200")
    ap.add_argument("--timeout", type=float, default=None,
                    help="override timeout (s); default 8 (auto) / 130 (interactive)")
    args = ap.parse_args()

    port = args.port or detect_port()
    if not args.port:
        print(f"(auto-detected serial port: {port})")

    if args.suite:
        return run_suite(port, args.baud)
    test = args.test or "blinky"
    return run_single(port, args.baud, test, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
