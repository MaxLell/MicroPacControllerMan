cmake_minimum_required(VERSION 3.20)
project(pacman C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Cortex-M4F architecture flags (shared by compile, assemble, link).
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32G431RBTx_FLASH.ld)

add_executable(pacman.elf
    App/main.c
    Bsp/led/led.c
    Bsp/uart/uart.c
    Bsp/retain_ram/retain_ram.c
    Test/Target/ott.c
    Test/Target/ott_scenarios.c
    Test/Target/scripts/ott_blinky.c
    third_party/embedded_cli/Cli.c
    cmsis/device/system_stm32g4xx.c
    startup/startup_stm32g431xx.s)

target_compile_definitions(pacman.elf PRIVATE STM32G431xx)

# One include dir per module folder (headers are included by bare name),
# grouped by architecture layer — see the layer Readme.md files.
target_include_directories(pacman.elf PRIVATE
    App
    Bsp/led
    Bsp/uart
    Bsp/retain_ram
    Test/Target
    Test/Target/scripts
    third_party/embedded_cli
    cmsis/core
    cmsis/device/include)

target_compile_options(pacman.elf PRIVATE
    ${MCU_FLAGS}
    -Wall -Wextra
    -Og -g3
    -ffunction-sections -fdata-sections)

target_link_options(pacman.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/pacman.map
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--print-memory-usage)

# Produce .bin and .hex alongside the .elf.
add_custom_command(TARGET pacman.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pacman.elf> ${CMAKE_BINARY_DIR}/pacman.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:pacman.elf> ${CMAKE_BINARY_DIR}/pacman.hex
    COMMENT "Creating pacman.bin / pacman.hex")
