# 8 Troubleshooting Guide

[← Back to Index](Index.md)

This guide is a skeleton, seeded from known risks ([05 Risks, Assumptions & Dependencies](05-Risks-Assumptions-and-Dependencies.md)). It is expected to grow as real issues surface during [later phases](04-Implementation-Phases-and-Milestones.md).

## 8.1 Pre-Power-On Checklist

- [ ] Confirm the Click Shield's per-socket logic-level switches match the voltage requirements of the LCD Mono Click (slot 1) and Touchpad Click (slot 2) before first power-on. See [R-005](05-Risks-Assumptions-and-Dependencies.md#51-risks).
- [ ] Confirm USB-C is connected to either the Nucleo board or the shield, not both in a conflicting power configuration.

## 8.2 Toolchain Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| Board not detected by flashing tool | STLINK V3 driver not installed, or USB cable is charge-only | Install ST-Link drivers; use a data-capable USB-C cable. |
| Build succeeds but nothing happens on target | Wrong target selected / stale binary flashed | Re-check target/board configuration in the build tool; re-flash. |

## 8.3 Debug / Serial Console Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| No output on serial console | Wrong baud rate, or STLINK VCP port not selected | Check UART baud rate configuration; verify correct COM/tty device for the STLINK virtual COM port. |

## 8.4 Display Issues (LCD Mono Click)

| Symptom | Likely Cause | Fix |
|---|---|---|
| Screen stays blank | DISP pin held low, or wrong SPI mode/pin mapping | Confirm DISP (mikroBUS **MISO** line = PA6 on slot 1) is driven high; re-verify SPI pin mapping per [VT-INT-003](06-Verification-and-Validation.md). |
| Nothing appears but SPI toggles | CS wired/used active-low | The LS013B7DH03 SCS is **active-HIGH** — CS must be driven HIGH for the transfer (opposite the usual SPI convention). |
| Image looks inverted or "ghosted" | VCOM/COM polarity not inverting (stuck DC bias) | The firmware drives software VCOM per flush and pulses EXTCOMIN; check the **EXTMODE jumper (JP1)** on the LCD Mono Click matches — software VCOM needs EXTMODE=low. Ensure a VCOM tick at least once/second on a static image. |
| Blink (LD2) stops after a display test | PA5 is shared by LD2 and SPI1_SCK | Expected — the display OTT reconfigures PA5 as SCK; nominal blink is restored on the next boot (`led_init()` runs after the OTT path). |

## 8.5 Touchpad Issues (Touchpad Click)

| Symptom | Likely Cause | Fix |
|---|---|---|
| No I2C response from MTCH6102 (`ott touchpad` reports "not responding") | Wrong I2C address, SDA/SCL pin mapping, or RST held low | Address is **0x25** (7-bit); SCL/SDA are **PB8/PB9 (AF4)** on this board, *not* the Arduino A4/A5 route (PC0/PC1 have no I2C AF); confirm RST (PA4) is released high. Re-verify per [VT-INT-004](06-Verification-and-Validation.md). |
| Touch position axes swapped/mirrored vs. the dot | Raw touch origin/orientation differs from the panel | Adjust the X/Y→screen mapping (flip an axis) in `ott_touchdot.c`; the controller reports X in 0..576, Y in 0..384. |
| Game-Control-Cross feels unreliable as a d-pad | Quadrant boundaries/dead-zone untuned for raw touch position | Tune quadrant boundaries empirically, see [R-003](05-Risks-Assumptions-and-Dependencies.md#51-risks); fall back to the Touchpad Click's gesture-detection API if raw-position mapping proves unworkable. |
