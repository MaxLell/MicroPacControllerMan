# Drivers — Device Drivers

Drivers for external devices, built on the `Bsp/` transports. A driver turns a raw
transport (SPI / I2C / digital I/O) into a device-oriented API and owns everything
device-specific: register maps, command bytes, timing, bit polarity, coordinate ranges.

A driver does **not** call the HAL. GPIO goes through `Bsp/dio_bsp` by logical pin name,
buses through `Bsp/spi_bsp` and `Bsp/dio_bsp`. That is what makes a driver testable on the
host with the BSP mocked — see `Test/Host/test_display.c`, which pins the panel's wire
format down byte-for-byte without any hardware.

**What goes here** — one folder per device, `Drivers/<device>/<device>.c`/`.h`.

| Driver | What |
|---|---|
| `st7789/` | The display controller (Sitronix ST7789V, 240×320): reset and init sequence, an identification self-check, rectangle fills, and pixel blits that take a sub-rectangle plus a stride so a caller can send part of a larger image. |
| `display/` | The display **port**, above it: `display.h` is a platform-independent sink that shows a `framebuffer_t`, whole (`display_present`) or one rectangle at a time (`display_present_region` — the lever that makes the frame rate work). `display.c` is the target implementation; `display_host.c` is the headless host one, which keeps the last frame for an SDL window to blit. |

Note the split the display went through, because it is the shape to copy. The pixels
and the drawing live in `Services/framebuffer` and `Services/gfx`, which are pure logic;
the driver's whole job is to present a buffer someone else filled. So the panel's
inverted bit sense — it wants a set bit to mean white, the frame buffer uses a set bit
for ink — stays confined to `display.c` instead of leaking into everything that draws.

**Depends on:** `Bsp/`, `Services/`. **Never** depends on `App/`.
