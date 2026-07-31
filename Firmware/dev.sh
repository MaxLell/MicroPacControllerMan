#!/usr/bin/env bash
#
# The one command worth remembering. Wraps the cmake / programmer / ceedling / python steps
# so none of them has to be typed by hand. Run it from anywhere.
#
#   Host — no board needed
#     ./dev.sh test        # host unit tests (ceedling)
#     ./dev.sh format      # format the source tree to the coding standard
#     ./dev.sh check       # formatting + unit tests + both builds, writing nothing
#     ./dev.sh host        # build the host library, don't run anything
#
#   Commit gate
#     ./dev.sh install-hook # pre-commit: format the staged files, then run the unit tests
#     ./dev.sh remove-hook  # take it back out
#     ./dev.sh pre-commit   # what that hook runs, if you want it by hand
#
#   Target — needs the Nucleo on USB
#     ./dev.sh build       # cross-build pacman.elf
#     ./dev.sh flash       # build + flash over ST-LINK
#     ./dev.sh suite       # build + flash + every automatic OTT
#     ./dev.sh manual      # build + flash + every OTT that needs you at the board
#     ./dev.sh display_test   # build + flash + one named OTT
#     ./dev.sh joystick_dot   #   "
#     ./dev.sh animation      #   "
#     ./dev.sh all         # build + flash once, then the automatic suite and the manual one
#
# Override the serial port with PORT=/dev/ttyACMx ./dev.sh display_test
#
# This is an umbrella, not a second implementation: `format` and `check` call
# ./format.sh, the OTTs call Test/run_ott.py, and the unit tests call ceedling. Each of
# those stays usable on its own, and none of them is reimplemented here — so there is one
# definition of each job however it is reached. The pre-commit hook is the exception that
# proves it: it is a one-line `exec` into `./dev.sh pre-commit`, because deciding *which*
# jobs guard a commit is exactly the kind of policy this file exists to hold.
#
set -euo pipefail

cd "$(dirname "$0")"

# Auto-detect the ST-LINK VCP so the ttyACM number doesn't matter (override with
# PORT=/dev/ttyACMx). run_ott.py does its own detection; this is just for messages.
detect_port() {
    local p
    for pat in '/dev/serial/by-id/'*STLINK*if02* '/dev/serial/by-id/'*STLINK* '/dev/ttyACM'*; do
        for p in $pat; do
            [ -e "$p" ] && {
                readlink -f "$p"
                return
            }
        done
    done
    echo /dev/ttyACM0
}
PORT="${PORT:-$(detect_port)}"
BUILD_DIR="build"
HOST_BUILD_DIR="build-host"
ELF="$BUILD_DIR/pacman.elf"
PROGRAMMER="${PROGRAMMER:-STM32_Programmer_CLI}"
HOOK_PATH="../.git/hooks/pre-commit"

step() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
fail() { printf '\033[1;31m%s\033[0m\n' "$*" >&2; }

do_build() {
    step "Build (target)"
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" -G "Unix Makefiles"
    fi
    cmake --build "$BUILD_DIR" -j
}

do_host_build() {
    step "Build (host)"
    if [ ! -d "$HOST_BUILD_DIR" ]; then
        cmake -B "$HOST_BUILD_DIR" -DPACMAN_HOST_BUILD=ON -G "Unix Makefiles"
    fi
    cmake --build "$HOST_BUILD_DIR" -j
}

# STM32CubeProgrammer, not openocd: openocd 0.12.0 attaches to this part but its flash
# driver does not know device ID 0x455 (STM32U535/U545), so `program` fails. See
# openocd.cfg. Override with PROGRAMMER=/path/to/STM32_Programmer_CLI.
do_flash() {
    do_build
    step "Flash over ST-LINK"
    "$PROGRAMMER" -c port=SWD -w "$ELF" -v -rst
}

do_test() {
    step "Host unit tests"
    ceedling test:all
}

# Everything a reviewer would want green, and nothing that writes to the tree — the same
# set a CI job should run.
do_check() {
    step "Check: formatting"
    ./format.sh --check

    do_test
    do_host_build
    do_build

    step "All checks passed."
}

# --- the commit gate -------------------------------------------------------
#
# What the hook runs. Formatting and the unit tests, and deliberately nothing else: the
# OTTs need the board plugged in and a human watching it, which is not a thing to owe on
# every commit, and the cross-build costs time without catching much the host build does
# not. `./dev.sh check` is the fuller gate — run that before opening a PR.
do_pre_commit() {
    local -a staged
    mapfile -t staged < <(git -C .. diff --cached --name-only --diff-filter=ACMR \
        | grep -E '^Firmware/.*\.[ch]$' | grep -vE '^Firmware/(ThirdParty|build)' || true)

    if [ ${#staged[@]} -eq 0 ]; then
        # A docs-only or config-only commit has nothing here to check.
        return 0
    fi

    step "Pre-commit: formatting ${#staged[@]} staged file(s)"
    ./format.sh --staged

    # Worth knowing: this tests the working tree, not the index. With unstaged changes
    # around, what runs is not exactly what is being committed. Testing the index properly
    # needs a throwaway checkout, which is more machinery than this earns.
    do_test

    step "Pre-commit passed."
}

install_hook() {
    cat >"$HOOK_PATH" <<'HOOK'
#!/usr/bin/env bash
# Installed by Firmware/dev.sh install-hook. Remove with: Firmware/dev.sh remove-hook
# Skip once with: git commit --no-verify
exec "$(git rev-parse --show-toplevel)/Firmware/dev.sh" pre-commit
HOOK

    chmod +x "$HOOK_PATH"
    step "Installed $HOOK_PATH"
    echo "   Staged C sources are formatted and the unit tests run on every commit."
    echo "   Skip once with: git commit --no-verify"
}

remove_hook() {
    rm -f "$HOOK_PATH"
    step "Removed the pre-commit hook."
}

run_test() {
    case "$1" in
        suite)
            step "Run: the automatic OTTs  (port $PORT)"
            python3 Test/run_ott.py --suite --port "$PORT"
            ;;
        manual)
            step "Run: the OTTs that need you at the board  (port $PORT)"
            python3 Test/run_ott.py --manual --port "$PORT"
            ;;
        *)
            step "Run: ott $1  (port $PORT)"
            python3 Test/run_ott.py "$1" --port "$PORT"
            ;;
    esac
}

cmd="${1:-all}"
shift || true

case "$cmd" in
    build) do_build ;;
    host) do_host_build ;;
    flash) do_flash ;;
    test) do_test ;;
    check) do_check ;;
    format) ./format.sh "$@" ;;
    pre-commit) do_pre_commit ;;
    install-hook) install_hook ;;
    remove-hook) remove_hook ;;
    suite | manual | display_id | display_test | joystick | joystick_dot | animation | user_button)
        do_flash
        run_test "$cmd"
        ;;
    all)
        do_flash
        run_test suite
        run_test manual
        ;;
    -h | --help | help)
        sed -n '2,35p' "$0" | sed 's|^# \{0,1\}||'
        ;;
    *)
        fail "Unknown command: $cmd"
        echo "Try: test | format | check | host | build | flash | install-hook |" >&2
        echo "     all | suite | manual | display_id | display_test | joystick |" >&2
        echo "     joystick_dot | animation | user_button" >&2
        exit 2
        ;;
esac
