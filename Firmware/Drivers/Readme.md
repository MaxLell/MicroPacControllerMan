# Drivers — Device Drivers

Drivers for external devices, built on the `Bsp/` transports. A driver turns a raw
transport (SPI / I2C / digital I/O) into a device-oriented API and owns everything
device-specific: register maps, command bytes, timing, bit polarity, coordinate ranges.

A driver does **not** call the HAL. GPIO goes through `Bsp/dio_bsp` by logical pin name,
buses through `Bsp/spi_bsp` / `Bsp/i2c_bsp`. That is what makes a driver testable on the
host with the BSP mocked — see `Test/Host/test_display.c`, which pins the panel's wire
format down byte-for-byte without any hardware.

**What goes here** — one folder per device, `Drivers/<device>/<device>.c`/`.h`.

| Driver | What |
|---|---|
| `display/` | The display **port**: `display.h` is a platform-independent sink that shows a `framebuffer_t`. `display.c` is the target implementation (LCD Mono Click, Sharp LS013B7DH03, 128×128 1 bpp, write-only, pushed in full inside one chip-select window). `display_host.c` is the host implementation, headless — it keeps the last frame, which is what an SDL window blits. It also owns the panel's once-per-second COM inversion, via `display_service()`. |
| `touchpad/` | Touchpad Click (Microchip MTCH6102). One read yields a `touchpad_reading_t` (position + touch-present); the position is zeroed when nothing is touched. Coordinates are the controller's raw range, not panel pixels. **Not yet a port** — see RF-003. |

Note the split the display went through, because it is the shape to copy. The pixels
and the drawing live in `Services/framebuffer` and `Services/gfx`, which are pure logic;
the driver's whole job is to present a buffer someone else filled. So the panel's
inverted bit sense — it wants a set bit to mean white, the frame buffer uses a set bit
for ink — stays confined to `display.c` instead of leaking into everything that draws.

**Depends on:** `Bsp/`, `Services/`. **Never** depends on `App/`.
