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
- `systick/` — 1 kHz time base (`millis()` / `delay_ms()`), shared by the
  super-loop and the OTT scenarios.
- `spi/` — SPI1 (PA5/PA7, AF5) transmit-only transport for mikroBUS slot 1.
- `i2c/` — I2C1 (PB8/PB9, AF4) transport for mikroBUS slot 2, timeout-bounded.
- `button/` — on-board user button B1 (PC13), for the button-confirmed OTTs.

Pin assignments follow the derived mikroBUS map — see
[02 §2.3.3](../../Docu/PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001).

**Depends on:** CMSIS only. **Never** depends on `Drivers/`, `Services/`, `App/`.
