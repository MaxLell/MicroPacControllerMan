cmake_minimum_required(VERSION 3.22)

# The project name must equal the executable target that the STM32CubeMX
# interface library appends its generated sources to — see
# ThirdParty/STM32_G431RB_HAL/cmake/stm32cubemx/CMakeLists.txt, which does
# target_sources(${CMAKE_PROJECT_NAME} ...).
set(CMAKE_PROJECT_NAME pacman)
project(${CMAKE_PROJECT_NAME} C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Root of the STM32CubeMX / HAL export. It owns the HAL + CMSIS, the Cortex
# startup, the linker script, and the clock + peripheral init (main.c, gpio.c,
# i2c.c, usart.c, MSP, IRQ handlers). It is regenerated from CubeMX; we only
# edit the USER CODE blocks of its main.c (to call app_main()).
set(CUBE ${CMAKE_SOURCE_DIR}/ThirdParty/STM32_G431RB_HAL)

# Create the executable first, then pull in the generated HAL. The CubeMX
# interface lib (target `stm32cubemx`, INTERFACE) carries the HAL/CMSIS include
# dirs + the USE_HAL_DRIVER/STM32G431xx defines, builds the HAL sources into an
# object lib, and appends Core/Src/*.c + the startup to this executable.
add_executable(${CMAKE_PROJECT_NAME})
add_subdirectory(${CUBE}/cmake/stm32cubemx stm32cubemx)

# --- our firmware: M2 board bring-up on HAL (UART + systick + the OTT console
# with the button and touchpad scenarios). The display/gfx drivers and their OTT
# scenarios are ported to HAL and added here next. ---
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/app_main.c
    Bsp/uart/uart.c
    Bsp/systick/systick.c
    Bsp/button/button.c
    Bsp/i2c/bsp_i2c.c
    Bsp/retain_ram/retain_ram.c
    Drivers/touchpad/touchpad.c
    Test/Target/ott.c
    Test/Target/ott_scenarios.c
    Test/Target/scripts/ott_button.c
    Test/Target/scripts/ott_touchpad.c
    ThirdParty/EmbeddedCli/Cli.c)

# One include dir per module folder (headers are included by bare name).
# ${CUBE}/Core/Inc gives our BSP the CubeMX pin macros + peripheral handles
# (main.h, usart.h); the HAL/CMSIS headers arrive via the stm32cubemx lib below.
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App
    Bsp/uart
    Bsp/systick
    Bsp/button
    Bsp/i2c
    Bsp/retain_ram
    Drivers/touchpad
    Test/Target
    Test/Target/scripts
    ThirdParty/EmbeddedCli
    ${CUBE}/Core/Inc)

target_link_libraries(${CMAKE_PROJECT_NAME} stm32cubemx)

# Warnings on our translation units (no -Werror; the generated Core/Src files
# compiled into this target also see these, which is fine).
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Og -g3)

# Linker script from the CubeMX export. It carries our `.noinit` section for the
# OTT retained-RAM reset flow (added just after .bss) — that block is marked
# NON-GENERATED in the .ld and must be re-added after any CubeMX regeneration.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${CUBE}/STM32G431xx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/pacman.map
    -Wl,--print-memory-usage)

# Produce .bin and .hex alongside the .elf.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/pacman.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/pacman.hex
    COMMENT "Creating pacman.bin / pacman.hex")
