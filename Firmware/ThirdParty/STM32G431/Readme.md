# STM32G431 — vendor device support

ST/ARM-provided, device-specific files for the STM32G431RB. **Not our code** —
vendored verbatim from ST's public repositories (`STM32CubeG4` /
`cmsis_device_g4`), so they live under `ThirdParty/` rather than in an
architecture layer. The one local change is the `.noinit` region added to the
linker script for the OTT retained-RAM mechanism ([doc 09](../../../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)).

- `CMSIS/Include/` — CMSIS core headers (`core_cm4.h`, `cmsis_gcc.h`, …).
- `CMSIS/Device/ST/STM32G4xx/Include/` — device headers (`stm32g431xx.h`,
  `stm32g4xx.h`, `system_stm32g4xx.h`).
- `CMSIS/Device/ST/STM32G4xx/Source/Templates/system_stm32g4xx.c` — system init.
- `startup_stm32g431xx.s` — vector table + reset handler (copies `.data`,
  zero-fills `.bss`, leaves `.noinit` intact).
- `STM32G431RBTx_FLASH.ld` — memory map (128 KB FLASH, 32 KB RAM) incl. the
  `.noinit` region.
