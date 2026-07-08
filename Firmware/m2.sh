#!/usr/bin/env bash
#
# One-shot M2 bring-up helper: build + flash + run an OTT, so you don't type the
# cmake/openocd/python steps by hand. Run it from anywhere.
#
#   ./m2.sh all         # build, flash once, then run the 3 interactive tests in a row
#   ./m2.sh display     # build + flash + run just the display test
#   ./m2.sh touchpad    # build + flash + run just the touchpad test
#   ./m2.sh touchdot    # build + flash + run just the touch-dot test
#   ./m2.sh suite        # build + flash + run the automatic suite (enum/banner/blinky)
#   ./m2.sh flash        # build + flash only (no test)
#   ./m2.sh build        # build only
#
# Override the serial port with PORT=/dev/ttyACMx ./m2.sh display
#
set -euo pipefail

cd "$(dirname "$0")"

PORT="${PORT:-/dev/ttyACM0}"
BUILD_DIR="build"
ELF="$BUILD_DIR/pacman.elf"
TOOLCHAIN="cmake/arm-none-eabi.toolchain.cmake"

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

do_build() {
    step "Build"
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
    fi
    cmake --build "$BUILD_DIR" -j
}

do_flash() {
    do_build
    step "Flash over ST-LINK"
    openocd -f openocd.cfg -c "program $ELF verify reset exit"
}

run_test() {
    step "Run: ott $1  (port $PORT)"
    python3 Test/run_ott.py "$1" --port "$PORT"
}

cmd="${1:-all}"
case "$cmd" in
    build) do_build ;;
    flash) do_flash ;;
    suite | blinky | display | touchpad | touchdot)
        do_flash
        run_test "$cmd"
        ;;
    all)
        do_flash
        for t in display touchpad touchdot; do
            printf '\n\033[1;33m--- Next test: %s. Get ready at the board; press ENTER to start ---\033[0m\n' "$t"
            read -r _
            run_test "$t"
        done
        step "All three interactive tests done."
        ;;
    -h | --help | help)
        sed -n '2,20p' "$0"
        ;;
    *)
        echo "Unknown command: $cmd (try: all | display | touchpad | touchdot | suite | flash | build)" >&2
        exit 2
        ;;
esac
