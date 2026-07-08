# Drivers — Device Drivers

Higher-level drivers for external devices, built on top of `Bsp/` transports.
A driver turns a raw transport (SPI/I2C/GPIO) into a device-oriented API.

**What goes here** — one folder per device, `Drivers/<device>/<device>.c`/`.h`.
Populated in [Milestone 2](../../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md):
- `display/` — LCD Mono Click (Sharp LS013B7DH03) over `Bsp/spi`: RAM framebuffer,
  software VCOM, DISP/CS/EXTCOMIN control.
- `gfx/` — 1-bpp geometric primitives (lines/rects/circles/triangles) on the
  display framebuffer; no text/logo.
- `touchpad/` — Touchpad Click (MTCH6102) over `Bsp/i2c`: raw X/Y + touch-present.

**Depends on:** `Bsp/`, `Services/`. **Never** depends on `App/`.
