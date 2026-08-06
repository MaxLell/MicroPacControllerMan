# The development environment, so that a second machine needs Docker and nothing else.
#
# **Ubuntu 24.04 is not a preference, it is the point.** Every tool version this project has been
# verified against is the one Ubuntu noble packages — gcc-arm-none-eabi 13.2.1, cmake 3.28.3,
# clang-format 18, ruby 3.2, SDL2 2.30 — and the machine it was developed on is Linux Mint 22.3,
# which is noble underneath. So `apt-get install` here reproduces the *verified* toolchain rather
# than approximating it. Pinning individual versions on top of that would add a second thing to
# maintain and could only ever disagree with the distribution.
#
# What is deliberately **not** in here: STM32CubeProgrammer, which is what flashes this board. It is
# behind an ST account and cannot be fetched unattended, so the image cannot own it. Mount the host's
# install and point `PROGRAMMER` at it — see the Docker section of `Firmware/README.md`. Everything
# that does *not* touch the board — both builds, the unit tests, the formatter, the host game, the
# whole AI training — is self-contained here.
#
# The packages, and why each is here:
#
#   build-essential cmake make   the host build, and gcc for the tests
#   gcc-arm-none-eabi            the cross-toolchain, 13.2.1 on noble
#   binutils-arm-none-eabi       arm-none-eabi-size, which the build prints its usage with
#   libnewlib-arm-none-eabi      the C library for the target: math.h, libc.a, libm.a. Named
#                                explicitly because it is only a *Recommends* of the compiler, and
#                                --no-install-recommends therefore leaves it out — which is how the
#                                first build of this image failed, on `math.h: No such file or
#                                directory` from the HAL. It pulls libnewlib-dev, where the headers
#                                actually live.
#   clang-format                 format.sh, against the vendored .clang-format
#   ruby ruby-dev                ceedling is a gem; ruby-dev is for native extensions in its deps
#   libsdl2-dev                  the host application's window (CON-103 / FR-104)
#   python3 python3-venv         run_ott.py, and the trainer below
#   openocd gdb-multiarch        the debug server. It cannot flash this part — see openocd.cfg
#   psmisc                       fuser, which run_ott.py uses to say the serial port is already open
#   git ca-certificates          a mounted repository is still a repository
#
# Comments are deliberately kept out of the RUN below: a `#` line inside a line continuation is a
# Dockerfile trap, and this file is easier to trust with the explanation above it.
FROM ubuntu:24.04

# Keeps apt from asking about a keyboard layout in the middle of a build.
ARG DEBIAN_FRONTEND=noninteractive

# One layer, and the cache is dropped in the same one: an apt cache in an image is a few hundred
# megabytes nobody will ever read.
RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        binutils-arm-none-eabi \
        build-essential \
        ca-certificates \
        clang-format \
        cmake \
        gcc-arm-none-eabi \
        gdb-multiarch \
        libnewlib-arm-none-eabi \
        git \
        libsdl2-dev \
        make \
        openocd \
        psmisc \
        python3 \
        python3-venv \
        ruby \
        ruby-dev \
    && rm -rf /var/lib/apt/lists/*

# The unit-test runner, pinned: Ceedling generates the mocks and the runners, so a different version
# is a different test build, and "it passes on my machine" is the thing this image exists to remove.
# 1.1.1 is the version the suite has been run against throughout.
RUN gem install --no-document ceedling -v 1.1.1

# The trainer's Python, in a virtual environment on PATH rather than installed over the system's.
# Ubuntu 24.04 refuses the latter (PEP 668) and it is right to: `--break-system-packages` would put
# neat-python where apt believes it owns the directory. With the venv first on PATH, `python3` inside
# the container *is* this environment, so nothing in `Training/` needs to know it is in a container
# and `Training/.venv` is not used here at all.
#
# The requirements file is copied rather than the package being named here, so there is one list of
# what training needs and it is the one in the repository.
COPY Training/requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt
ENV PATH="/opt/venv/bin:${PATH}"

# Ruby and Python both want somewhere to write a cache, and the container runs as *the host's* user
# id (see `dev.sh docker`) — which has no home directory in here. `/tmp` is writable for anybody,
# which is what makes an arbitrary uid work without the image knowing it in advance.
ENV HOME=/tmp

# Nothing is copied in: the repository is mounted, so an edit on the host is an edit in the container
# and a build artefact belongs to the host user. An image with the sources baked in would be a
# snapshot to rebuild after every change. `dev.sh docker` mounts the repository *root* at /work and
# works in /work/Firmware, because `.git` is a level above the firmware.
WORKDIR /work/Firmware

# Git refuses to work in a tree owned by another user, which is what a mounted repository looks like
# when the uid does not match. It does match — `dev.sh docker` passes the host's — but somebody
# running `docker run` by hand may not, and a refusal there is a confusing first impression.
RUN git config --system --add safe.directory /work

CMD ["/bin/bash"]
