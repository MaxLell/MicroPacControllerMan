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
#   After training — take a winner into the firmware
#     ./dev.sh adopt-weights                    # Training/winner.json
#     ./dev.sh adopt-weights path/to/other.json # a campaign run's winner
#     ./dev.sh adopt-weights --force ...        # adopt one that does not meet FR-037
#
#   Training in the container, detached — survives the terminal
#     ./dev.sh docker-train        # the campaign, in the background, logs followed
#     ./dev.sh docker-train --fresh # ...after throwing away previous winners
#     ./dev.sh docker-train-stop   # stop it
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
done_message() { printf '\033[1;32m%s\033[0m\n' "$*"; }

# A build directory belongs to the path it was configured at.
#
# CMake writes the source path into CMakeCache.txt as an absolute one, so a `build/` configured on
# the host and then used inside the container — where the same tree is /work/Firmware — stops with
# "the current CMakeCache.txt is different than the directory where CMakeCache.txt was created",
# which is accurate and reads like the tree is broken. It is not: the build directory is simply
# somebody else's. Said here in one sentence, with the fix, rather than left to CMake.
check_build_dir_belongs_here() {
    local directory=$1
    local cache="$directory/CMakeCache.txt"
    local configured_at

    [ -f "$cache" ] || return 0

    configured_at=$(sed -n 's|^CMAKE_HOME_DIRECTORY:INTERNAL=||p' "$cache" | head -1)

    [ -n "$configured_at" ] && [ "$configured_at" != "$PWD" ] || return 0

    fail "$directory was configured for $configured_at, and this is $PWD."
    {
        echo "  A build directory carries the absolute path it was configured at, so one made on the"
        echo "  host cannot be used in the container or the other way round. Throw it away:"
        echo ""
        echo "    rm -rf $directory"
        echo ""
        echo "  Nothing is lost — it is all regenerated, and the two can coexist no other way."
    } >&2

    exit 2
}

do_build() {
    step "Build (target)"
    check_build_dir_belongs_here "$BUILD_DIR"
    if [ ! -d "$BUILD_DIR" ]; then
        cmake -B "$BUILD_DIR" -G "Unix Makefiles"
    fi
    cmake --build "$BUILD_DIR" -j
}

do_host_build() {
    step "Build (host)"
    check_build_dir_belongs_here "$HOST_BUILD_DIR"
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

# Take a trained network into the firmware.
#
# This exists because the order matters and getting it wrong is silent. Exporting weights changes
# what the target computes, so the FR-039 state set recorded against the *old* weights becomes a
# recording of a different network — and `ott ai_equivalence` would then report a porting fault that
# is really a stale file. It refuses instead, on the digest, which is the safety net; this is the
# thing that keeps you off it.
#
# It also gates on VT-UNIT-010: a winner that does not meet FR-037 is not adopted unless you say
# --force. Training produces a winner every time, including a bad one, and the one thing that must
# not happen quietly is a worse agent replacing a better one in the firmware.
do_adopt_weights() {
    local force=""
    local winner=""

    while [ "$#" -gt 0 ]; do
        case "$1" in
            --force) force=yes ;;
            *) winner=$1 ;;
        esac
        shift
    done

    winner=${winner:-Training/winner.json}

    if [ ! -f "$winner" ]; then
        fail "$winner does not exist."
        exit 2
    fi

    local python=Training/.venv/bin/python
    [ -x "$python" ] || python=python3

    step "Measuring $winner (VT-UNIT-010)"

    # The host library first: evaluate.py and the recorder both load it, and a stale one measures a
    # different game than the sources describe.
    do_host_build

    if "$python" Training/evaluate.py --winner "$winner"; then
        echo ""
    elif [ "$force" = yes ]; then
        fail "It does not meet FR-037 — adopting anyway, because --force."
    else
        fail "$winner does not meet FR-037, so it is not being adopted."
        {
            echo "  Train longer, or adopt it deliberately with:"
            echo ""
            echo "    ./dev.sh adopt-weights --force $winner"
        } >&2
        exit 1
    fi

    # Tracked, and the one the exporter reads: a campaign's own winner files are not in git.
    if [ "$winner" != "Training/winner.json" ]; then
        step "Copying $winner over Training/winner.json"
        cp "$winner" Training/winner.json
    fi

    step "Exporting App/pacman_ai/ai_weights.[ch]"
    "$python" Training/export_c.py

    # Rebuilt *after* the export, because the recorder must run the new weights.
    do_host_build

    step "Re-recording the FR-039 state set"
    ./build-host/pacman_ai_record > Test/Target/scripts/ott_ai_equivalence_states.c

    step "Formatting what was generated"
    ./format.sh Test/Target/scripts/ott_ai_equivalence_states.c App/pacman_ai

    done_message "Adopted. Four files changed — commit them together:"
    echo "  Training/winner.json"
    echo "  App/pacman_ai/ai_weights.c"
    echo "  App/pacman_ai/ai_weights.h"
    echo "  Test/Target/scripts/ott_ai_equivalence_states.c"
    echo ""
    echo "Then, on the machine with the board: ./dev.sh suite"
    echo "ott ai_equivalence is what proves the port agrees with the host about the new weights."
}

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

    # Three different faults produce one error from docker, and they have three different fixes. So
    # say which one it is rather than listing all of them and letting the reader try each: the socket
    # either is not there, or is there and we are not allowed at it.
    {
        if [ -n "${DOCKER_HOST:-}" ]; then
            echo "  DOCKER_HOST is set to '$DOCKER_HOST', so this is not about the local socket."
            echo "  Whatever it points at is not answering. Unset it to use the local daemon."
        elif [ ! -S /var/run/docker.sock ]; then
            echo "  There is no socket at /var/run/docker.sock, so the daemon is not running:"
            echo ""
            echo "    sudo systemctl enable --now docker"
            echo ""
            echo "  (A rootless installation puts its socket elsewhere and sets DOCKER_HOST; this"
            echo "  has neither, so it is the ordinary one and it is stopped.)"
        elif id -nG | tr ' ' '\n' | grep -qx docker; then
            echo "  You are in the 'docker' group already, so the membership has not reached this"
            echo "  shell — a group is read at login, not looked up per command:"
            echo ""
            echo "    newgrp docker          # this shell only"
            echo ""
            echo "  ...or log out and back in, which fixes every shell. 'id -nG' in the new shell"
            echo "  should list docker before you try again."
        else
            echo "  The socket is there and you are not in the 'docker' group, which owns it:"
            echo ""
            echo "    sudo usermod -aG docker \"\$USER\" && newgrp docker"
            echo ""
            echo "  'newgrp docker' fixes the shell you are in; logging out and in fixes every shell."
        fi

        echo ""
        echo "  Do not reach for 'sudo ./dev.sh docker' as a workaround. It works, and it writes"
        echo "  every build artefact into your tree as root — see the Docker section of README.md."
    } >&2

    exit 2
}

TRAIN_CONTAINER="micropac-train"

# The campaign, detached, so that closing the terminal does not end the night.
#
# Wrapped rather than written out in the README for two reasons a person hits in that order: a
# container of this name left over from last time makes `docker run` refuse, and a winner file left
# over from last time makes the campaign *skip* that run — which shortens the night without looking
# like anything went wrong.
run_training_in_docker() {
    local fresh=""
    local keep=""

    while [ "$#" -gt 0 ]; do
        case "$1" in
            --fresh) fresh=yes ;;
            --keep) keep=yes ;;
            *) fail "Unknown option for docker-train: $1"; exit 2 ;;
        esac
        shift
    done

    require_docker
    build_docker_image

    # The trainer loads this through ctypes and dies on the first line without it. Checked here
    # rather than found out by a container that exits four seconds after being started detached,
    # which is a night lost to a missing file.
    if [ ! -f "$HOST_BUILD_DIR/libpacman_env.so" ]; then
        fail "$HOST_BUILD_DIR/libpacman_env.so does not exist, and the trainer loads it."
        {
            echo "  Build it first — in the container, so that its paths are the container's:"
            echo ""
            echo "    ./dev.sh docker host"
        } >&2

        exit 2
    fi

    check_build_dir_belongs_here "$HOST_BUILD_DIR"

    if docker container inspect "$TRAIN_CONTAINER" >/dev/null 2>&1; then
        step "Removing the previous $TRAIN_CONTAINER container"
        docker rm -f "$TRAIN_CONTAINER" >/dev/null
    fi

    # A run whose winner exists is measured rather than repeated — deliberate, and what makes a
    # campaign resumable after a reboot. It is also the thing that silently halves a night when the
    # leftovers were not meant to be kept, so it is said out loud either way.
    local -a leftovers=()
    if [ -d Training/campaign ]; then
        mapfile -t leftovers < <(find Training/campaign -maxdepth 1 -name '*.json' -printf '%f\n' 2>/dev/null | sort)
    fi

    if [ "${#leftovers[@]}" -gt 0 ] && [ "$keep" != yes ]; then
        if [ "$fresh" = yes ]; then
            step "Throwing away ${#leftovers[@]} previous winner(s): ${leftovers[*]}"
            rm -f Training/campaign/*.json
        else
            fail "Training/campaign already holds ${#leftovers[@]} winner(s): ${leftovers[*]}"
            {
                echo "  Those runs will be *measured, not trained* — which is what makes a campaign"
                echo "  resumable, and what shortens a night when you did not mean it. Either keep"
                echo "  them on purpose:"
                echo ""
                echo "    ./dev.sh docker-train --keep"
                echo ""
                echo "  ...or start over:"
                echo ""
                echo "    ./dev.sh docker-train --fresh"
            } >&2
            exit 2
        fi
    fi

    local repository_root
    repository_root=$(cd .. && pwd)

    local uid=${SUDO_UID:-$(id -u)}
    local gid=${SUDO_GID:-$(id -g)}

    step "Starting the campaign in $TRAIN_CONTAINER"

    docker run -d --name "$TRAIN_CONTAINER" \
        --user "$uid:$gid" \
        --volume "$repository_root:/work" \
        --workdir /work/Firmware \
        "$DOCKER_IMAGE" python3 Training/campaign.py >/dev/null

    echo ""
    echo "  Follow it:  ./dev.sh docker-train        (or docker logs -f $TRAIN_CONTAINER)"
    echo "  Stop it:    ./dev.sh docker-train-stop"
    echo "  Read it:    Training/campaign/summary.md, rewritten after every run"
    echo ""

    # Straight into the log, so the first thing seen is the campaign saying when it will be done.
    docker logs -f "$TRAIN_CONTAINER"
}

stop_training_in_docker() {
    require_docker

    if ! docker container inspect "$TRAIN_CONTAINER" >/dev/null 2>&1; then
        fail "There is no $TRAIN_CONTAINER container."
        exit 2
    fi

    step "Stopping $TRAIN_CONTAINER"

    # Thirty seconds rather than the default ten: nothing needs them, but a trainer mid-write of a
    # winner file should be allowed to finish it.
    docker stop -t 30 "$TRAIN_CONTAINER" >/dev/null

    echo "Stopped. Nothing is lost — train.py writes its winner on every improvement, and"
    echo "'./dev.sh docker-train' measures rather than repeats a run whose winner is on disk."
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
    adopt-weights) do_adopt_weights "$@" ;;
    docker) run_in_docker "$@" ;;
    docker-train) run_training_in_docker "$@" ;;
    docker-train-stop) stop_training_in_docker ;;
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
        echo "     docker | docker-build | docker-train | docker-train-stop |" >&2
        echo "     adopt-weights |" >&2
        echo "     all | suite | manual | display_id | display_test | joystick |" >&2
        echo "     joystick_dot | animation | user_button" >&2
        exit 2
        ;;
esac
