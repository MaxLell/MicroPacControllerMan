# Bsp — Board Support Package

Lowest layer: register-level access to the STM32G431 and the board. Talks to
CMSIS/peripheral registers directly; exposes a thin C API to the layers above.
No application logic here.

**What goes here** — one folder per peripheral/board facility,
`Bsp/<module>/<module>.c`/`.h`:
- `led/` — on-board LED LD2 (PA5) GPIO.
- `uart/` — LPUART1 on the ST-LINK VCP (serial transport).
- `retain_ram/` — the `.noinit` retained-RAM buffer that survives a soft reset
  (carries the OTT request across the reset, doc 09).
- Later (Milestone 2): the SPI and I2C register access for the mikroBUS slots.

**Depends on:** CMSIS only. **Never** depends on `Drivers/`, `Services/`, `App/`.
