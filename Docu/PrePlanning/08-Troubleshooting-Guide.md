# 8 Troubleshooting Guide

[← Back to Index](Index.md)

Seeded from known risks ([05 Risks, Assumptions & Dependencies](05-Risks-Assumptions-and-Dependencies.md)) and since filled in with what actually went wrong during Board Bring-Up. Entries that describe hardware no longer on the bench have been removed rather than kept for reference — a troubleshooting guide that sends you to a device you do not own costs more than it saves.

## 8.1 Pre-Power-On Checklist

- [ ] Confirm the X-NUCLEO-GFX01M2 is seated on the ST-Morpho headers with CN2→CN7 and CN3→CN10 aligned; the shield draws its 3V3 from the board and needs no separate supply.
- [ ] Confirm USB is connected to the Nucleo board only.

## 8.2 Toolchain Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| Board not detected by flashing tool | STLINK V3 driver not installed, or USB cable is charge-only | Install ST-Link drivers; use a data-capable USB-C cable. |
| Build succeeds but nothing happens on target | Wrong target selected / stale binary flashed | Re-check target/board configuration in the build tool; re-flash. |

## 8.3 Debug / Serial Console Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| No output on serial console | Wrong baud rate, or STLINK VCP port not selected | Check UART baud rate configuration; verify correct COM/tty device for the STLINK virtual COM port. |

## 8.4 Display Issues (ST7789V on the GFX01M2)

| Symptom | Likely Cause | Fix |
|---|---|---|
| `ott display_id` reads `00 00 00` | Chip select driven at the wrong polarity | CS is **active LOW**, despite UM2750 saying "active high" in five places. `display_id` deliberately reads at both polarities so this shows up as data on one of them rather than as silence. |
| ID reads as plausible nonsense (`42 C2 A9`) | The **one-bit** dummy on register reads | Shift the received bytes left by one bit: `42 C2 A9` becomes `85 85 52`. Data that is off by one bit looks like garbage rather than like a wiring fault, which is the trap. |
| No answer at either polarity | Pin map taken from UM2750's GPIO columns | Those columns are per-board-family and **UM2750 lists no STM32U5 board at all**. Derive the map by connector position against UM3062 table 18; three of twelve positions differ from the NUCLEO-G431RB. See [M2 Board Bring-Up §1](../Design/M2-Board-Bring-Up.md). |
| Every colour appears as its complement — red shows cyan, yellow shows blue | Display inversion left on | `ST7789_USE_INVERSION` off. Note how it is caught: a test that announces "full red" passes anyway, because complemented colour bars still look like colour bars. Draw something with a **named** expectation — a yellow disc that comes out blue is exact in RGB565 (`~0xFFE0 = 0x001F`). |
| LD2 no longer responds | PA5 is now `SPI1_SCK` | Expected and permanent. An alternate-function pin is not a GPIO the firmware can drive, so the LED is out of the project; this is unrelated to solder bridge SB10, which stays at its default. |
| Panel works but the frame rate collapses | A full frame is being sent per update | A full 240 × 320 frame is 153,600 bytes and 252 ms. Use `display_present_region()` and send only what changed — see [M2 Board Bring-Up §3](../Design/M2-Board-Bring-Up.md). |

## 8.5 Joystick Issues (GFX01M2)

| Symptom | Likely Cause | Fix |
|---|---|---|
| A key never registers in `ott joystick` | Pin map, or an internal pull fighting the shield | All five keys are active low and **the shield pulls them up itself**, so `GPIO_NOPULL` is correct. The heartbeat line in `ott joystick` prints all five raw levels each second, which separates "this key never moves" from "no key is being seen". |
| The name printed does not match the key pushed | Two positions swapped in the map | That is exactly what the test exists to catch; a test that only counted presses would pass. Check LEFT (PC9) and CENTER (PC6) first — those are two of the three positions that differ from the NUCLEO-G431RB. |
| The dot in `ott joystick_dot` moves the wrong way | Panel orientation versus frame-buffer row order | With `MADCTL = 0x00` the naive mapping is correct and confirmed on hardware ([§1.7](../Design/M2-Board-Bring-Up.md)). If it is ever wrong, fix it in `st7789_init`'s MADCTL, not in the game. |
| Input feels laggy | The debounce window, not the display | Drawing a move costs 2.08 ms; `Bsp/switch` needs 32 samples at 1 ms before it reports a key at all. See [RF-014](../Refactoring-Backlog.md#rf-014). |

## 8.6 Test Harness Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| Console output arrives with characters missing; tests time out or report nonsense | **A second reader on the same serial port** | Two readers split the incoming bytes. A `console.py` left open in another terminal is the usual culprit, and it looks exactly like flaky hardware. `run_ott.py` now names the offending PID at start-up. |
| `STM32_Programmer_CLI` works but openocd's `program` fails with `auto_probe failed` | openocd 0.12.0 does not know device ID 0x455 | Flash with STM32CubeProgrammer; openocd is the gdb server only. Reasoning in `Firmware/openocd.cfg`. |
| An OTT never returns and the next run cannot reach the prompt | The previous scenario is still running its safety cap | Every scenario is time-capped and falls back to nominal mode; wait it out, or reset the board with `STM32_Programmer_CLI -c port=SWD -rst`. |
