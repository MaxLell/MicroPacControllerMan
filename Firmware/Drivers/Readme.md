# Drivers — Device Drivers

Higher-level drivers for external devices, built on the `Bsp/` transports. A driver
turns a raw transport (SPI / I2C / digital I/O) into a device-oriented API and owns
everything device-specific: register maps, command bytes, timing, coordinate ranges.

A driver does **not** call the HAL. GPIO goes through `Bsp/dio_bsp` by logical pin
name, buses through `Bsp/spi_bsp` / `Bsp/i2c_bsp`.

**What goes here** — one folder per device, `Drivers/<device>/<device>.c`/`.h`.

| Driver | What |
|---|---|
| `display/` | LCD Mono Click (Sharp LS013B7DH03, 128×128, 1 bpp). The panel is write-only, so pixels go into a RAM frame buffer and nothing appears until `display_flush()`. Also owns the once-per-second COM inversion the panel needs while a static image is held — `display_service_vcom()` serves both the software and the external inversion mode, so the board's jumper setting does not matter. |
| `gfx/` | 1-bpp geometric primitives on the frame buffer: lines, rectangles, circles, triangles, filled and outlined. No text or logo. Shapes may extend past the panel edges; `display_set_pixel()` clips them. |
| `touchpad/` | Touchpad Click (Microchip MTCH6102). One read yields a `touchpad_reading_t` (position + touch-present); the position is zeroed when nothing is touched. Coordinates are the controller's raw range, not panel pixels — mapping them onto a display is the caller's job (see `ott_touchdot.c`). |

Colours are `display_color_e` (`DISPLAY_COLOR_BLACK` is ink on), and coordinates are
`int16_t` so clipping arithmetic can go negative.

**Depends on:** `Bsp/`, `Services/`. **Never** depends on `App/`.
