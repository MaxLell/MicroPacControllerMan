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
| Screen stays blank | DISP pin held low, or wrong SPI mode/pin mapping | Confirm DISP is driven high; re-verify SPI pin mapping per [VT-INT-003](06-Verification-and-Validation.md). |
| Image looks inverted or "ghosted" | EXTCOMIN polarity-inversion signal not toggling at the required rate | Check EXTMODE/EXTCOMIN handling per the LS013B7DH03 datasheet. |

## 8.5 Touchpad Issues (Touchpad Click)

| Symptom | Likely Cause | Fix |
|---|---|---|
| No I2C response from MTCH6102 | Wrong I2C address, or SDA/SCL pin mapping incorrect | Re-verify I2C pin mapping per [VT-INT-004](06-Verification-and-Validation.md). |
| Game-Control-Cross feels unreliable as a d-pad | Quadrant boundaries/dead-zone untuned for raw touch position | Tune quadrant boundaries empirically, see [R-003](05-Risks-Assumptions-and-Dependencies.md#51-risks); fall back to the Touchpad Click's gesture-detection API if raw-position mapping proves unworkable. |
