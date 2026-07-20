#!/usr/bin/env python3
"""
Interactive serial console for the on-target EmbeddedCli.

Opens the ST-LINK virtual COM port and bridges your terminal to the board's CLI:
what you type is sent to the board, what the board prints appears live. The board
echoes your keystrokes itself, so you see your input as the firmware sees it — type
`help`, `ott`, `reset`, etc. and press Enter.

    ./console.py                      # auto-detect the ST-LINK VCP
    ./console.py --port /dev/ttyACM1  # or name it explicitly

Exit with Ctrl-C. Stdlib only — no pyserial required.
"""
import argparse
import glob
import os
import select
import subprocess
import sys
import termios
import tty

QUIT = 0x03  # Ctrl-C  -> leave the console (raw mode delivers it as a byte, not SIGINT)


def detect_port() -> str:
    """Find the ST-LINK virtual COM port so the ttyACM number doesn't matter."""
    for pat in ("/dev/serial/by-id/*STLINK*if02*",
                "/dev/serial/by-id/*STLINK*",
                "/dev/serial/by-id/*STM*"):
        hits = sorted(glob.glob(pat))
        if hits:
            return os.path.realpath(hits[0])
    acm = sorted(glob.glob("/dev/ttyACM*"))
    return acm[0] if acm else "/dev/ttyACM0"


def configure_tty(port: str, baud: str) -> None:
    subprocess.run(
        ["stty", "-F", port, baud, "raw", "-echo", "-echoe", "-echok", "-crtscts"],
        check=True,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None, help="serial port (default: auto-detect the ST-LINK VCP)")
    ap.add_argument("--baud", default="115200")
    args = ap.parse_args()

    port = args.port or detect_port()
    configure_tty(port, args.baud)
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)

    sys.stdout.write(f"Connected to {port} @ {args.baud}. Press Ctrl-C to exit.\r\n")
    sys.stdout.flush()

    stdin_fd = sys.stdin.fileno()
    old = termios.tcgetattr(stdin_fd)
    try:
        tty.setraw(stdin_fd)  # deliver each keystroke immediately, no local echo
        while True:
            r, _, _ = select.select([fd, stdin_fd], [], [])
            if fd in r:
                data = os.read(fd, 256)
                if data:
                    os.write(sys.stdout.fileno(), data)
            if stdin_fd in r:
                key = os.read(stdin_fd, 64)
                if QUIT in key:
                    break
                os.write(fd, key)
    finally:
        termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old)
        os.close(fd)
        sys.stdout.write("\r\nDisconnected.\r\n")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
