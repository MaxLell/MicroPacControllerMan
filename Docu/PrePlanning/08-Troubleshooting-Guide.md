---
title: Troubleshooting Guide
---

# 8 Troubleshooting Guide

[[Index|← Back to Index]]

This guide is a skeleton, seeded from known risks ([[05-Risks-Assumptions-and-Dependencies]]). It is expected to grow as real issues surface during [[04-Implementation-Phases-and-Milestones|later phases]].

## 8.1 Pre-Power-On Checklist

- [ ] Confirm the Click Shield's per-socket logic-level switches match the voltage requirements of the LCD Mono Click (slot 1) and Touchpad Click (slot 2) before first power-on. See [[05-Risks-Assumptions-and-Dependencies|R-005]].
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
| Screen stays blank | DISP pin held low, or wrong SPI mode/pin mapping | Confirm DISP is driven high; re-verify SPI pin mapping per [[06-Verification-and-Validation\|VT-HW-002]]. |
| Image looks inverted or "ghosted" | EXTCOMIN polarity-inversion signal not toggling at the required rate | Check EXTMODE/EXTCOMIN handling per the LS013B7DH03 datasheet. |

## 8.5 Touchpad Issues (Touchpad Click)

| Symptom | Likely Cause | Fix |
|---|---|---|
| No I2C response from MTCH6102 | Wrong I2C address, or SDA/SCL pin mapping incorrect | Re-verify I2C pin mapping per [[06-Verification-and-Validation\|VT-HW-003]]. |
| Gestures feel unreliable as a d-pad | Gesture-detection API mismatch with discrete-direction use case | Fall back to raw touch-position thresholding, see [[05-Risks-Assumptions-and-Dependencies\|R-003]]. |

## 8.6 Related Documents

- [[05-Risks-Assumptions-and-Dependencies]]
- [[06-Verification-and-Validation]]
