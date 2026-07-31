cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# Which of the two builds is this?
#
#   cmake -B build                              -> STM32 target firmware (default)
#   cmake -B build-host -DPACMAN_HOST_BUILD=ON  -> host build, no hardware needed
#
# [03 Architecture §3.8] requires one source tree to build for both. Only code that
# is genuinely hardware-independent takes part in the host build; the seams that let
# more of it do so are being cut incrementally (RF-003).
# ==============================================================================
option(PACMAN_HOST_BUILD "Build the hardware-independent code for the host instead of the STM32 target" OFF)

# ==============================================================================
# Cross-compilation setup (target build only)
#
# These variables normally live in a separate toolchain file, but CMake reads them
# during project() so setting them here works just as well — and keeps the whole
# build in one file. No -DCMAKE_TOOLCHAIN_FILE needed: `cmake -B build` is enough.
#
# The MCU flags are set through the *_INIT variables so they also apply to the
# STM32CubeMX-generated HAL/CMSIS objects and the startup assembly, which must be
# built with the same Cortex-M33 ABI as our own code.
#
# Guarded, because setting CMAKE_SYSTEM_NAME / CMAKE_C_COMPILER unconditionally is
# exactly what made a host build impossible before (RF-001).
# ==============================================================================
if(NOT PACMAN_HOST_BUILD)
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    CACHE FILEPATH "size")

# Emit <target>.elf directly (openocd.cfg / run_ott.py expect build/pacman.elf).
set(CMAKE_EXECUTABLE_SUFFIX_C   ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")

# Don't try to link a full executable when probing the compiler — there is no
# runtime to link against on a bare-metal target.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCU_FLAGS "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")
# newlib-nano; the syscall stubs come from the generated syscalls.c / sysmem.c, so
# no --specs=nosys.specs (which would collide with them).
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} --specs=nano.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()

# ==============================================================================
# Project
#
# The project name must equal the executable target the STM32CubeMX interface
# library appends its generated sources to — see
# ThirdParty/STM32_U545RE_HAL/cmake/stm32cubemx/CMakeLists.txt, which does
# target_sources(${CMAKE_PROJECT_NAME} ...).
# ==============================================================================
set(CMAKE_PROJECT_NAME pacman)
project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ==============================================================================
# Host build
#
# Only the modules that are already free of the HAL take part. `pacman_host` is the
# library the SDL application (CON-103 / FR-104) and the unit tests will link
# against; it grows as the platform-port seams are cut (RF-003).
#
# systick_bsp is included through its host implementation: the header is shared
# with the target, only the .c differs. That is the pattern for every future port.
# ==============================================================================
if(PACMAN_HOST_BUILD)
    add_library(pacman_host STATIC
        App/agent/agent.c
        App/game/game.c
    App/game_view/game_view.c
    App/render/render.c
        App/game_view/game_view.c
        App/render/render.c
        App/ghost/ghost.c
        App/ghost_path/ghost_path.c
        App/pacman/pacman.c
        App/playfield/playfield.c
        App/score/score.c
        App/sprite_set/sprite_set.c
        Bsp/systick_bsp/systick_bsp_host.c
        Drivers/display/display_host.c
        Services/crc/crc.c
        Services/delay/delay.c
        Services/framebuffer/framebuffer.c
        Services/gfx/gfx.c
        Services/sprite/sprite.c
        Services/active_object/active_object.c
        Services/circular_buffer/circular_buffer.c
        Services/msg_broker/msg_broker.c
        Services/msg_queue/msg_queue.c
        Services/sw_timer/sw_timer.c
        ThirdParty/embedded_utils/custom_assert.c)

    target_include_directories(pacman_host PUBLIC
        App/agent
        App/game
    App/game_view
    App/render
        App/game_view
        App/render
        App/ghost
        App/ghost_path
        App/pacman
        App/playfield
        App/score
        App/sprite_set
        Bsp/systick_bsp
        Drivers/display
        Services/crc
        Services/delay
        Services/framebuffer
        Services/gfx
        Services/sprite
        Services/active_object
        Services/circular_buffer
        Services/msg
        Services/msg_broker
        Services/msg_queue
        Services/sw_timer
        ThirdParty/embedded_utils)

    # Selects the host variant of anything that differs by platform.
    target_compile_definitions(pacman_host PUBLIC PACMAN_HOST_BUILD)
    target_compile_options(pacman_host PRIVATE -Wall -Wextra -g)

    # Unit tests are NOT built here — they run under Ceedling, which does its own
    # compilation with mocked dependencies: `ceedling test:all` (see project.yml).
    #
    # The host application (CON-103 / FR-104) is the playable one: the same game, the
    # same view and the same render path as the target, with SDL where the panel is.
    # It is optional, because everything else here builds and every unit test runs
    # without SDL — a missing library should cost you the window, not the build.
    find_package(SDL2 QUIET)

    if(SDL2_FOUND)
        add_executable(pacman_host_app App/host_main.c)
        target_link_libraries(pacman_host_app PRIVATE pacman_host SDL2::SDL2 SDL2::SDL2main)
        target_compile_options(pacman_host_app PRIVATE -Wall -Wextra -g)
    else()
        message(STATUS "SDL2 not found — skipping the host application. "
                       "Install it with: sudo apt-get install -y libsdl2-dev")
    endif()

    return()
endif()

# Root of the STM32CubeMX / HAL export. It owns the HAL + CMSIS, the Cortex
# startup, the linker script, and the clock + peripheral init (main.c, gpio.c,
# icache.c, usart.c, MSP, IRQ handlers). It is regenerated from CubeMX; we only
# edit the USER CODE blocks of its main.c (to call app_main()).
set(CUBE ${CMAKE_SOURCE_DIR}/ThirdParty/STM32_U545RE_HAL)

# Create the executable first, then pull in the generated HAL. The CubeMX
# interface lib (target `stm32cubemx`, INTERFACE) carries the HAL/CMSIS include
# dirs + the USE_HAL_DRIVER/STM32U545xx defines, builds the HAL sources into an
# object lib, and appends Core/Src/*.c + the startup to this executable.
add_executable(${CMAKE_PROJECT_NAME})
add_subdirectory(${CUBE}/cmake/stm32cubemx stm32cubemx)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/app_main.c
    Bsp/dio_bsp/dio_bsp.c
    Bsp/joystick/joystick.c
    Bsp/retain_ram/retain_ram.c
    Bsp/spi_bsp/spi_bsp.c
    Bsp/switch/switch.c
    Bsp/systick_bsp/systick_bsp.c
    Bsp/uart_bsp/uart_bsp.c
    Bsp/user_button/user_button.c
    Drivers/display/display.c
    Drivers/st7789/st7789.c
    Services/console/console.c
    Services/crc/crc.c
    Services/delay/delay.c
    Services/framebuffer/framebuffer.c
    Services/gfx/gfx.c
    Services/active_object/active_object.c
    Services/circular_buffer/circular_buffer.c
    Services/msg_broker/msg_broker.c
    Services/msg_queue/msg_queue.c
    Services/sw_timer/sw_timer.c
    Test/Target/ott.c
    Test/Target/ott_framebuffer.c
    Test/Target/ott_scenarios.c
    Test/Target/scripts/ott_animation.c
    Test/Target/scripts/ott_display_id.c
    Test/Target/scripts/ott_display_test.c
    Test/Target/scripts/ott_joystick.c
    Test/Target/scripts/ott_joystick_dot.c
    Test/Target/scripts/ott_user_button.c
    ThirdParty/EmbeddedCli/Cli.c
    ThirdParty/embedded_utils/custom_assert.c)

# One include dir per module folder (headers are included by bare name).
# ${CUBE}/Core/Inc gives the BSP the CubeMX pin macros + peripheral handles
# (main.h, i2c.h, spi.h, usart.h); the HAL/CMSIS headers arrive via the
# stm32cubemx interface library below.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App
    App/agent
    App/game
    App/ghost
    App/ghost_path
    App/pacman
    App/playfield
    App/score
    App/sprite_set
    Bsp/dio_bsp
    Bsp/joystick
    Bsp/retain_ram
    Bsp/spi_bsp
    Bsp/switch
    Bsp/systick_bsp
    Bsp/uart_bsp
    Bsp/user_button
    Drivers/display
    Drivers/st7789
    Services/console
    Services/crc
    Services/delay
    Services/framebuffer
    Services/gfx
    Services/active_object
    Services/circular_buffer
    Services/msg
    Services/msg_broker
    Services/msg_queue
    Services/sw_timer
    Test/Target
    Test/Target/scripts
    ThirdParty/EmbeddedCli
    ThirdParty/embedded_utils
    ${CUBE}/Core/Inc)

target_link_libraries(${CMAKE_PROJECT_NAME} stm32cubemx)

# Warnings on our translation units (no -Werror; the generated Core/Src files
# compiled into this target also see these, which is fine).
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Og -g3)

# Linker script from the CubeMX export. It carries our `.noinit` section for the
# OTT retained-RAM reset flow (added just after .bss) — that block is marked
# NON-GENERATED in the .ld and must be re-added after any CubeMX regeneration.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CUBE}/STM32U545xx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/pacman.map
    -Wl,--print-memory-usage)

# Produce .bin and .hex alongside the .elf.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/pacman.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/pacman.hex
    COMMENT "Creating pacman.bin / pacman.hex")
