#!/usr/bin/env python3
"""
Drive the On-Target Tests (OTT) over the ST-LINK serial console.

How to use it:

  ./run_ott.py                         # run the AUTOMATIC suite (default)
  ./run_ott.py --suite                 # same, explicitly
  ./run_ott.py --manual                # run every test that needs you at the board
  ./run_ott.py --list                  # which tests exist, and which kind each is
  ./run_ott.py display_test            # run one test by name (a manual one streams live,
  ./run_ott.py joystick_dot            #   with a long timeout, and you confirm at the
  ./run_ott.py animation               #   board with the USER button)

The automatic suite covers the Board-Bring-Up checks a machine can judge on its own:
  VT-INT-001  Power-On & Enumeration   (the VCP device node exists)
  VT-INT-002  Serial Console Output    (`reset` re-emits the known boot banner)

The display and joystick tests (VT-INT-006, VT-INT-019..021) are interactive by design —
the firmware renders/prints and waits for you to confirm with the USER button —
so they are excluded from --suite and streamed live instead.

Stdlib only — no pyserial required. Exit 0 = all pass, 1 = fail, 2 = timeout.
"""
import argparse
import codecs
import glob
import os
import select
import subprocess
import sys
import time

BANNER = "MicroPacControllerMan booted"

# The scenario split, defined once. AUTOMATIC tests judge themselves and are safe to run
# unattended; MANUAL ones render or print something only a person can assess and end on a
# USER-button press. `dev.sh` asks for a suite or a name and does not keep its own copy of
# this list, so adding a scenario means editing one place.
AUTOMATIC = ["display_id"]
MANUAL = ["display_test", "joystick", "joystick_dot", "animation", "user_button", "pacman"]

INTERACTIVE = set(MANUAL)

# A manual test gets a couple of minutes to be confirmed, which is plenty for "does the
# panel show the right colours". `pacman` is played rather than looked at, and the things
# worth confirming — a ghost leaving the house, a power pellet, a level turning over — take
# minutes at the arcade's own pace. The scenario itself allows 600 s, so the harness has to
# outlast it or it would report a timeout on a test that is still going.
INTERACTIVE_TIMEOUT_S = 130.0
LONG_TIMEOUT_S = {"pacman": 620.0}


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


def warn_if_port_is_busy(port: str) -> None:
    """Say so when something else already has the port open.

    Two readers on one tty split the incoming bytes between them, so the harness sees a
    stream with characters missing and reports a timeout or a mangled line — with nothing
    pointing at the real cause. A `console.py` left open in another terminal is the usual
    culprit, and the symptom looks exactly like a flaky board.
    """
    try:
        result = subprocess.run(["fuser", port], capture_output=True, text=True, timeout=5)
    except (FileNotFoundError, subprocess.SubprocessError):
        return  # No fuser: skip the check rather than fail because of a missing tool.

    holders = result.stdout.split()

    if not holders:
        return

    print(f"WARNING: {port} is already open by PID(s) {' '.join(holders)} — the two readers "
          f"will split the bytes between them and this run will look corrupted.",
          file=sys.stderr)

    for pid in holders:
        try:
            with open(f"/proc/{pid}/cmdline", "rb") as cmdline:
                command = cmdline.read().replace(b"\0", b" ").decode().strip()
                print(f"         PID {pid}: {command}", file=sys.stderr)
        except OSError:
            pass

    print("         Close it and run again.", file=sys.stderr)


def configure_tty(port: str, baud: str) -> None:
    subprocess.run(
        ["stty", "-F", port, baud, "raw", "-echo", "-echoe", "-echok", "-crtscts"],
        check=True,
    )


def write_command(fd: int, command: str, per_char_delay: float = 0.002) -> None:
    """Send a command one character at a time.

    The firmware polls a single-byte RX register and echoes each character over a
    blocking UART TX, so it is deaf for the ~90 us it takes to echo. Writing a
    whole command in one burst overruns that register: the character is lost, the
    hardware latches ORE and — until the firmware clears it — stops raising RXNE
    at all. The firmware clears ORE now, but pacing the host means the characters
    are not dropped in the first place. 2 ms per character is ~20x the echo time
    and still only ~35 ms for a full command."""
    for char in command.encode():
        os.write(fd, bytes([char]))
        time.sleep(per_char_delay)


def read_until(fd: int, needles, timeout: float, echo: bool = False) -> "tuple[str|None, str]":
    """Read until one of `needles` (list of substrings) appears or timeout.
    Returns (matched_needle_or_None, full_text). Echoes bytes live if echo=True."""
    deadline = time.monotonic() + timeout
    text = ""
    # Incremental, because a read can split a multi-byte character in half and
    # decoding each chunk on its own would turn every em dash into garbage.
    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
    while time.monotonic() < deadline:
        r, _, _ = select.select([fd], [], [], max(0.0, deadline - time.monotonic()))
        if not r:
            break
        chunk = decoder.decode(os.read(fd, 256))
        if echo:
            sys.stdout.write(chunk)
            sys.stdout.flush()
        text += chunk
        for n in needles:
            if n in text:
                return n, text
    return None, text


def wait_until_idle(fd: int, quiet: float = 0.3, timeout: float = 3.0) -> None:
    """Wait for the board to finish booting before sending a command.

    The firmware polls the UART with a single-byte RX register and no buffer, so a
    command sent while the boot banner is still being written (blocking UART TX)
    overruns that register and characters are silently dropped. We drain and
    discard boot output until the stream has been silent for `quiet` seconds — by
    then the board is idle at its prompt and reads every byte we send. `timeout`
    only caps the wait; an already-idle board returns after one quiet window."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        r, _, _ = select.select([fd], [], [], quiet)
        if not r:
            return  # `quiet` seconds elapsed with no bytes -> board is idle
        os.read(fd, 256)


def run_single(port: str, baud: str, test: str, timeout: float) -> int:
    interactive = test in INTERACTIVE
    if timeout is None:
        timeout = LONG_TIMEOUT_S.get(test, INTERACTIVE_TIMEOUT_S) if interactive else 8.0

    warn_if_port_is_busy(port)
    configure_tty(port, baud)
    passed = f"OTT PASSED [{test}]"
    failed = f"OTT FAILED [{test}]"
    unknown = f"OTT ERROR: unknown test '{test}'"

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    try:
        if interactive:
            print(f"--- {test}: interactive; follow the prompts on the board, "
                  f"press the USER button (B1) to finish ---")
        # Don't send until the board has finished booting — see wait_until_idle().
        wait_until_idle(fd)
        write_command(fd, f"ott {test}\r\n")
        match, text = read_until(fd, [passed, failed, unknown], timeout, echo=interactive)
        if match == passed:
            print(f"\nPASS: {test}")
            return 0
        if match == failed:
            # read_until returns the instant "OTT FAILED [<test>]" appears, which is
            # before the ": <reason>" tail has arrived — and the reason is the whole
            # point of a failure report. Give the rest of the line a moment.
            _, tail = read_until(fd, ["\n"], 1.0, echo=interactive)
            text += tail
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
        wait_until_idle(fd)
        write_command(fd, "reset\r\n")
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

    for test in AUTOMATIC:
        rc = run_single(port, baud, test, timeout=8.0)
        results.append((f"ott {test}", rc == 0))

    print("\n--- summary ---")
    all_ok = True
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
        all_ok = all_ok and ok
    return 0 if all_ok else 1


def run_manual(port: str, baud: str) -> int:
    """Run the tests that need somebody at the board, one after another.

    Announced before each one, because the operator has to know what they are about to be
    asked to judge — and a run that starts while nobody is looking is a wasted run.
    """
    print("=== OTT manual suite — you need to be at the board ===")
    print(f"    {len(MANUAL)} tests: {', '.join(MANUAL)}\n")
    results = []

    for index, test in enumerate(MANUAL, start=1):
        print(f"\n--- [{index}/{len(MANUAL)}] {test} — press ENTER when you are ready ---")
        try:
            input()
        except EOFError:
            pass  # Not a terminal: run straight through rather than fail.
        results.append((f"ott {test}", run_single(port, baud, test, timeout=None) == 0))

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
                    help="test name (display_id/display_test/joystick/joystick_dot/animation/user_button/pacman); omit to run the suite")
    ap.add_argument("--suite", action="store_true", help="run the automatic regression suite")
    ap.add_argument("--manual", action="store_true",
                    help="run every test that needs a human at the board, in sequence")
    ap.add_argument("--list", action="store_true", help="list the scenarios and their kind")
    ap.add_argument("--port", default=None, help="serial port (default: auto-detect the ST-LINK VCP)")
    ap.add_argument("--baud", default="115200")
    ap.add_argument("--timeout", type=float, default=None,
                    help="override timeout (s); default 8 (auto) / 130 (interactive)")
    args = ap.parse_args()

    if args.list:
        for test in AUTOMATIC:
            print(f"  {test:<14} automatic")
        for test in MANUAL:
            print(f"  {test:<14} needs you at the board")
        return 0

    port = args.port or detect_port()
    if not args.port:
        print(f"(auto-detected serial port: {port})")

    if args.manual:
        return run_manual(port, args.baud)

    if args.suite or args.test is None:
        return run_suite(port, args.baud)
    return run_single(port, args.baud, args.test, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
