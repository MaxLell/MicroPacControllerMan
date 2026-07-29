# Bsp — Board Support Package

Lowest layer: access to the STM32G431 and the board. Wraps the **STM32 HAL** (from
the CubeMX export under `ThirdParty/`) into a thin, project-shaped C API for the
layers above. No application logic here.

Direct register access needs a justification in a comment. There is exactly one
today: `uart_bsp_read_character()` reads `RDR`, because the HAL offers no
non-blocking single-character read.

**What goes here** — one folder per peripheral or board facility,
`Bsp/<module>/<module>.c`/`.h`. Modules that wrap a peripheral carry the `_bsp`
suffix, matching the reference project.

| Module | What |
|---|---|
| `dio_bsp/` | Digital I/O. **The only module allowed to call HAL GPIO.** Callers name a pin via `dio_bsp_pin_e`; the port/mask lookup is a single table. |
| `uart_bsp/` | Blocking console transport on LPUART1 (ST-LINK VCP). The instance and the 115200 8N1 contract are `#define`s, so the module ports to another project by changing one line. |
| `i2c_bsp/` | Blocking I2C master for mikroBUS slot 2. Device-agnostic (7-bit address in, status enum out) and timeout-bounded, so an absent device reports rather than hangs. |
| `spi_bsp/` | Blocking transmit-only SPI master for mikroBUS slot 1. Chip-select belongs to the device driver, because the framing differs per device. |
| `systick_bsp/` | 1 kHz tick source: `systick_bsp_get_tick()` plus a 1 ms callback hook for work that needs a steady rate (debouncing). Implemented as a strong override of the HAL's `__weak HAL_IncTick()`, so the generated ISR file stays untouched. |
| `switch/` | Reusable debounced-GPIO input primitive. Shifts samples into a 32-bit history, so at the 1 ms rate the debounce window is 32 ms. Symmetric: the state only changes on an all-ones or all-zeros history. |
| `user_button/` | This board's B1 (PC13) instance of `switch`. Exposes the live state and a *latched press edge*, so callers wait for an operator confirmation without tracking edges themselves. |
| `retain_ram/` | The `.noinit` byte buffer that survives a software reset. Owns the memory only — what the bytes mean belongs to the caller (today `Test/Target/ott.c`). |

Note the deliberate split between a **generic primitive** (`switch`) and its
**concrete instance** (`user_button`). Expect the same shape for the game's inputs.

Pin assignments follow the HW-confirmed mikroBUS map — see
[M2 Board Bring-Up §1](../../Docu/Design/M2-Board-Bring-Up.md).
They are configured by CubeMX (`MX_GPIO_Init` and friends) before `app_main()`, so a
`*_bsp_init()` here usually only resets module state.

**Depends on:** the STM32 HAL / CMSIS only. **Never** depends on `Drivers/`,
`Services/`, `App/`. The one intra-layer dependency is `switch` and `user_button`
using `dio_bsp`, which is how the GPIO encapsulation is kept.
