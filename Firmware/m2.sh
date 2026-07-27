#!/usr/bin/env bash
#
# One-shot M2 bring-up helper: build + flash + run an OTT, so you don't type the
# cmake/openocd/python steps by hand. Run it from anywhere.
#
#   ./m2.sh all         # build, flash once, then run the 4 interactive tests in a row
#   ./m2.sh display     # build + flash + run just the display test
#   ./m2.sh touchpad    # build + flash + run just the touchpad test
#   ./m2.sh touchdot    # build + flash + run just the touch-dot test
#   ./m2.sh user_button # build + flash + run just the user-button test
#   ./m2.sh suite       # build + flash + run the automatic suite (enum + banner)
#   ./m2.sh flash        # build + flash only (no test)
#   ./m2.sh build        # build only
#
# Override the serial port with PORT=/dev/ttyACMx ./m2.sh display
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
    openocd -f openocd.cfg -c "program $ELF verify reset exit"
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
    suite | display | touchpad | touchdot | user_button)
        do_flash
        run_test "$cmd"
        ;;
    all)
        do_flash
        for t in user_button display touchpad touchdot; do
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
        echo "Unknown command: $cmd (try: all | user_button | display | touchpad | touchdot | suite | flash | build)" >&2
        exit 2
        ;;
esac
