#!/usr/bin/env bash
#
# One-shot bring-up helper: build + flash + run an OTT, so you don't type the
# cmake/programmer/python steps by hand. Run it from anywhere.
#
#   ./m2.sh all         # build, flash, run the automatic suite then the interactive tests
#   ./m2.sh display_id      # build + flash + read the display ID (automatic)
#   ./m2.sh display_test    # build + flash + draw display patterns (needs you)
#   ./m2.sh joystick    # build + flash + run just the joystick test (needs you)
#   ./m2.sh user_button # build + flash + run just the user-button test (needs you)
#   ./m2.sh suite       # build + flash + run the automatic suite (enum + banner)
#   ./m2.sh flash        # build + flash only (no test)
#   ./m2.sh build        # build only
#
# Override the serial port with PORT=/dev/ttyACMx ./m2.sh user_button
#
set -euo pipefail

cd "$(dirname "$0")"

# Auto-detect the ST-LINK VCP so the ttyACM number doesn't matter (override with
# PORT=/dev/ttyACMx). run_ott.py does its own detection; this is just for messages.
detect_port() {
    local p
    for pat in '/dev/serial/by-id/'*STLINK*if02* '/dev/serial/by-id/'*STLINK* '/dev/ttyACM'*; do
        for p in $pat; do
            [ -e "$p" ] && { readlink -f "$p"; return; }
        done
    done
    echo /dev/ttyACM0
}
PORT="${PORT:-$(detect_port)}"
BUILD_DIR="build"
ELF="$BUILD_DIR/pacman.elf"

# STM32CubeProgrammer, not openocd: openocd 0.12.0 attaches to this part but its
# flash driver does not know device ID 0x455 (STM32U535/U545), so `program` fails.
# See openocd.cfg. Override with PROGRAMMER=/path/to/STM32_Programmer_CLI.
PROGRAMMER="${PROGRAMMER:-STM32_Programmer_CLI}"

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

do_build() {
    step "Build"
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" -G "Unix Makefiles"
    fi
    cmake --build "$BUILD_DIR" -j
}

do_flash() {
    do_build
    step "Flash over ST-LINK"
    "$PROGRAMMER" -c port=SWD -w "$ELF" -v -rst
}

run_test() {
    if [ "$1" = "suite" ]; then
        step "Run: automatic suite  (port $PORT)"
        python3 Test/run_ott.py --suite --port "$PORT"
    else
        step "Run: ott $1  (port $PORT)"
        python3 Test/run_ott.py "$1" --port "$PORT"
    fi
}

cmd="${1:-all}"
case "$cmd" in
    build) do_build ;;
    flash) do_flash ;;
    suite | display_id | display_test | joystick | user_button)
        do_flash
        run_test "$cmd"
        ;;
    all)
        do_flash
        run_test display_id
        for t in display_test joystick user_button; do
            printf '\n\033[1;33m--- Next test: %s. Get ready at the board; press ENTER to start ---\033[0m\n' "$t"
            read -r _
            run_test "$t"
        done
        step "All interactive tests done."
        ;;
    -h | --help | help)
        sed -n '2,20p' "$0"
        ;;
    *)
        echo "Unknown command: $cmd (try: all | display_id | display_test | joystick | user_button | suite | flash | build)" >&2
        exit 2
        ;;
esac
