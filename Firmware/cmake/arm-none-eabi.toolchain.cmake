# Cross-compile toolchain for the STM32G431 target (arm-none-eabi + STM32 HAL).
#
# MCU flags are set globally (via the *_INIT variables) so the STM32CubeMX-
# generated HAL/CMSIS objects and the startup assembly are built with the same
# Cortex-M4F ABI as our own code. The linker script, map file and specs are
# applied on the executable target in CMakeLists.txt (the script lives under the
# CubeMX export in ThirdParty/).
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

# Don't try to link a full executable when probing the compiler.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS_INIT   "${MCU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")
# newlib-nano; syscall stubs come from the generated syscalls.c / sysmem.c
# (so no --specs=nosys.specs, which would collide with them).
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} --specs=nano.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
