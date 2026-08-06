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
#   Another machine — Docker, and nothing else installed
#     ./dev.sh docker      # a shell in the development image, this tree mounted
#     ./dev.sh docker check # run any of the commands above inside it
#     ./dev.sh docker-build # rebuild the image after editing docker/Dockerfile
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

# --- Docker -----------------------------------------------------------------
#
# The image carries the toolchain; the tree stays on the host and is mounted. So an edit is an edit
# on both sides, a build artefact belongs to the host user, and nothing has to be copied or rebuilt
# into an image after a change.
#
# What the container cannot do is flash the board: STM32CubeProgrammer is behind an ST account and
# cannot be fetched unattended, so the image does not have it. Mount the host's install and point
# PROGRAMMER at it — see Firmware/README.md.
DOCKER_IMAGE="micropac-dev"

build_docker_image() {
    local force=${1:-}

    require_docker

    if [ "$force" != "--force" ] && docker image inspect "$DOCKER_IMAGE" >/dev/null 2>&1; then
        return 0
    fi

    step "Building the $DOCKER_IMAGE image"

    # The context is Firmware/ because the image copies Training/requirements.txt out of it — one
    # list of what training needs, and it is the repository's.
    docker build -f docker/Dockerfile -t "$DOCKER_IMAGE" .
}

# Docker installed *and* reachable, or a sentence saying which of the two is missing.
#
# Checked before building rather than letting `docker build` fail, because what it prints then is
# "permission denied while trying to connect to the docker API at unix:///var/run/docker.sock" — true,
# and no help at all about the fix being a group.
require_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        # `fail` prints and returns — every other caller follows it with its own exit, and so does
        # this one. Without the exit the next line reports "docker: command not found" on top of a
        # perfectly good message.
        fail "docker is not installed. See the Docker section of README.md."
        exit 2
    fi

    if docker info >/dev/null 2>&1; then
        return 0
    fi

    fail "docker is installed, but its daemon cannot be reached."
    {
        echo "  Nearly always group membership rather than a broken daemon:"
        echo ""
        echo "    sudo usermod -aG docker \"\$USER\" && newgrp docker"
        echo ""
        echo "  'newgrp docker' fixes the shell you are in; logging out and in fixes every shell."
        echo "  If that is not it, the daemon itself: systemctl status docker"
        echo ""
        echo "  Do not reach for 'sudo ./dev.sh docker' as a workaround. It works, and it writes"
        echo "  every build artefact into your tree as root — see the Docker section of README.md."
    } >&2

    exit 2
}

run_in_docker() {
    require_docker

    build_docker_image

    # `--user` with the host's ids is what keeps a file written in the container owned by whoever ran
    # it. The image has no matching passwd entry and does not need one: HOME is /tmp, which is
    # writable for any uid, and that is all ruby and python want.
    #
    # Under sudo, `id -u` is 0 and every artefact would come out root-owned — so the ids sudo
    # remembers are preferred where they exist. That makes `sudo ./dev.sh docker` merely unnecessary
    # rather than something that leaves a tree you cannot build in afterwards.
    local uid=${SUDO_UID:-$(id -u)}
    local gid=${SUDO_GID:-$(id -g)}
    # The **repository root** is mounted, not `Firmware/`: `.git` lives one level up, so anything
    # git-shaped — `install-hook`, a commit, `git describe` — needs to see it, and the documents the
    # sources cross-reference are up there too. The working directory is `Firmware/`, which is where
    # every command in this file expects to be.
    # The repository root, spelled without a `..`: docker accepts the relative form, but a mount
    # argument is something people read in error messages and `-v /a/b/Firmware/..:/work` invites a
    # second look it does not deserve.
    local repository_root
    repository_root=$(cd .. && pwd)

    local -a arguments=(
        --rm
        --user "$uid:$gid"
        --volume "$repository_root:/work"
        --workdir /work/Firmware
    )

    # Interactive only when there is a terminal, so `./dev.sh docker check` works from a script or a
    # CI job as well as from a keyboard.
    if [ -t 0 ]; then
        arguments+=(--interactive --tty)
    fi

    # The host application opens a window. Handed through when there is a display to hand through,
    # and silently skipped when there is not — building and testing need no X server.
    if [ -n "${DISPLAY:-}" ] && [ -d /tmp/.X11-unix ]; then
        arguments+=(--env "DISPLAY=$DISPLAY" --volume /tmp/.X11-unix:/tmp/.X11-unix)
    fi

    # The serial console, when the board is plugged in: that is all `run_ott.py` needs, and it is
    # what makes the on-target tests runnable from in here once the programmer is reachable too.
    local port
    port=$(detect_port || true)

    if [ -n "$port" ] && [ -e "$port" ]; then
        arguments+=(--device "$port:$port")
    fi

    # Whatever the caller asked for, or a shell. `dev.sh` is re-entered inside the container rather
    # than a command being duplicated here, so `docker check` and `check` are the same job.
    if [ "$#" -eq 0 ]; then
        docker run "${arguments[@]}" "$DOCKER_IMAGE" bash
    else
        docker run "${arguments[@]}" "$DOCKER_IMAGE" ./dev.sh "$@"
    fi
}

case "$cmd" in
    build) do_build ;;
    host) do_host_build ;;
    flash) do_flash ;;
    test) do_test ;;
    check) do_check ;;
    format) ./format.sh "$@" ;;
    docker) run_in_docker "$@" ;;
    docker-build) build_docker_image --force ;;
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
        # Every comment line of the header, however long it grows: a fixed line range was truncating
        # the last paragraph mid-sentence the first time a command was added to it.
        awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
        ;;
    *)
        fail "Unknown command: $cmd"
        echo "Try: test | format | check | host | build | flash | install-hook |" >&2
        echo "     docker | docker-build |" >&2
        echo "     all | suite | manual | display_id | display_test | joystick |" >&2
        echo "     joystick_dot | animation | user_button" >&2
        exit 2
        ;;
esac
