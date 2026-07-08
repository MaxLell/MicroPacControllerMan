# Drivers — Device Drivers

Higher-level drivers for external devices, built on top of `Bsp/` transports.
A driver turns a raw transport (SPI/I2C/GPIO) into a device-oriented API.

**What goes here** — one folder per device, `Drivers/<device>/<device>.c`/`.h`.
Empty for now; populated in [Milestone 2](../../Docu/PrePlanning/04-Implementation-Phases-and-Milestones.md):
- `display/` — LCD Mono Click driver over the `Bsp/` SPI.
- `touchpad/` — Touchpad Click driver over the `Bsp/` I2C.

**Depends on:** `Bsp/`, `Services/`. **Never** depends on `App/`.
