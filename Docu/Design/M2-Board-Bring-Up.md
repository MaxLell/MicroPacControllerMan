# M2 Board Bring-Up — Design

Design document for **Milestone 2: Board Bring-Up** — bringing the X-NUCLEO-GFX01M2
display and joystick to life on the STM32U545RE-Q.

This is where the *how* lives: pin assignments, clock settings, transfer budgets, tool
choices and the questions still to be answered. The
[requirements](../PrePlanning/02-Requirements.md) state only *what* the system must do
and deliberately carry none of this detail.

Requirements realised here: CON-002, CON-003, CON-004 (display, joystick, carrier),
FR-004 (directional control), FR-005 / FR-028 (colour rendering), NFR-002 (frame rate),
NFR-003 (input latency).

## 1. Pin mapping

Established by cross-referencing two documents **by connector position**, which is the only
way that is valid here: the shield's own positions from **UM2750 Rev 3** (tables 12 and 13),
and this board's position-to-pin assignment from **UM3062 Rev 4** (table 18). The shield's
CN2 mates with the board's CN7, its CN3 with the board's CN10.

| Signal | Shield | Board | STM32U545RE | Notes |
|---|---|---|---|---|
| Display SCK | CN3-11 | CN10-11 | **PA5** | SPI1_SCK. Also LD2 — see §1.2 |
| Display MISO (SDO) | CN3-13 | CN10-13 | **PA6** | SPI1_MISO |
| Display MOSI (SDI) | CN3-15 | CN10-15 | **PA7** | SPI1_MOSI |
| Display CS (NCS) | CN3-21 | CN10-21 | **PC7** | GPIO, **active HIGH** |
| Display DCX (WR) | CN3-25 | CN10-25 | **PB10** | GPIO, data/command select |
| Display RESET | CN2-30 | CN7-30 | **PA1** | GPIO, active low |
| Display TE (FMARK) | CN2-28 | CN7-28 | **PA0** | GPIO in, optional |
| Joystick LEFT | CN3-17 | CN10-17 | **PC9** | GPIO in, active low |
| Joystick CENTER | CN3-19 | CN10-19 | **PC6** | GPIO in, active low |
| Joystick DOWN | CN3-27 | CN10-27 | **PB4** | GPIO in, active low |
| Joystick RIGHT | CN2-34 | CN7-34 | **PB0** | GPIO in, active low |
| Joystick UP | CN2-38 | CN7-38 | **PC0** | GPIO in, active low |

Display interface mode is strapped on the shield by IM0..IM3: **4-line, 8-bit data SPI**.

The unused NOR flash sits on PA8 (CS), PB13 (SCK), PB14 (MISO) and PB15 (MOSI), with
alternates selectable by the shield's solder bridges SB1..SB6. Nothing needs to be done
about it beyond leaving those pins alone. The shield also wires its flash data lines to
CN3-35/37, which on this board are **PA2/PA3** — worth remembering only because UM3062
footnote 7 says the ST-LINK VCP can be re-routed onto PA2/PA3 by solder bridge. Doing that
would put the flash and the console on the same wires.

### 1.1 Why the position cross-reference was necessary

**UM2750 lists no STM32U5 board at all** — its newest families are G4, WB and WL, and it is
dated March 2022. Its "STM32 GPIO" columns therefore describe *other* boards, and reading
the closest neighbour's column as this board's pin map is invalid.

It is also wrong in practice. Comparing the NUCLEO-G431RB column against UM3062's table for
this board, **three of the twelve positions carry different pins:**

| Signal | Position | NUCLEO-G431RB | NUCLEO-U545RE-Q |
|---|---|---|---|
| Display CS | CN10-21 | PA9 | **PC7** |
| Joystick LEFT | CN10-17 | PB6 | **PC9** |
| Joystick CENTER | CN10-19 | PC7 | **PC6** |

The other nine agree, which is exactly what makes the mistake tempting.

### 1.2 The one real conflict: SCK against LD2, and its documented fix

Display SCK is **PA5**, which is also **LD2**, the green user LED. That is a property of the
Nucleo-64 boards, not of the shield.

UM3062 table 19 provides the intended remedy: solder bridge **SB10**. Default is ON, where
"PA5 drives the LD2 LED and the SPI_SCK". Setting it **OFF** disconnects the LED, and the
manual names precisely this situation as the reason it exists — "in case of a signal issue on
SPI-SCK depending on the ARDUINO shield". The LED is a capacitive load on a clock line that
will run at tens of MHz, so this is not a cosmetic choice.

**Decision: set SB10 OFF for M2.** Consequences:

- LD2 stops working, so `dio_bsp`'s `DIO_BSP_PIN_LED_GREEN` and the CubeMX `LED_GREEN`
  output go away.
- The **`blinky` OTT is retired** — it verifies PA5 by driving it and reading it back, which
  neither the SPI peripheral nor a disconnected LED permits. It served its purpose as the
  first self-judging OTT; the joystick test that replaces it in M2 can be automatic in the
  same way for four of its five keys.

### 1.3 What is not a conflict after all

An earlier reading of this project's notes claimed display CS collides with the console's TX
on PA9. **It does not.** CS is on **PC7**; PA9/PA10 sit on CN10-2 and CN10-4, which the
shield leaves unconnected. The console, the ST-LINK VCP and the whole OTT reset flow are
untouched by the shield, and no CubeMX rework is needed for them.

Checked against the current configuration, the only collision with anything already in use
is PA5/LD2 above. PC13 (user button) and PA13/PA14 (SWD) are clear, and PC0, PC6, PC7, PC9,
PB0, PB4, PB10, PA0, PA1, PA6, PA7 are all currently unused.

### 1.4 Still to be measured

The map above is derived from two authoritative ST documents rather than from a neighbour's
column, so the confidence is far higher than before — but it is still paper. **Confirm the
display lines on the board with a logic analyzer before trusting a driver that misbehaves,**
and note that the joystick lines need no instrument at all: configure them as inputs with
pull-ups, press a direction, and the firmware can report which pin went low. That check is
cheap enough to be an OTT in its own right.

## 2. Clock and core configuration (as configured)

Owned by the STM32CubeMX `.ioc` and applied by its generated `SystemClock_Config()` before
the firmware entry point is reached.

| Item | Value | Note |
|---|---|---|
| SYSCLK / HCLK | 160 MHz | The STM32U545's maximum |
| Source | PLL1 from MSIS at 48 MHz | `PLLM 3 → 16 MHz × PLLN 10 → 160 MHz, PLLR 1` |
| Time base | SysTick at 1 kHz | Owned by the HAL; `Bsp/systick_bsp` reads it and hangs the 1 ms input-debounce hook off it |
| USART1 kernel clock | 160 MHz | Console rate is pinned to 115200 in firmware, not in the `.ioc` |
| Security | TrustZone off | Single non-secure domain (`CORTEX_M33_NS`) |
| Instruction cache | ICACHE enabled | Generated by CubeMX; wanted on this part |
| Display SPI bit rate | open | Set once the pin map (§1) is confirmed. This rate, not the CPU, decides the frame budget — see §3 |

CubeMX resolved the PLL source to MSIS rather than the HSI16 that was requested. It is
functionally equivalent here and the console runs correctly at 115200, so it stands as
configured.

Consequence for all firmware: never express a delay as a spin count. Use `Services/delay`
or `Services/sw_timer`, which are clock-independent.

## 3. Frame budget — why NFR-002 needs partial updates

NFR-002 asks for at least 30 frames per second. That cannot be met by transmitting whole
frames, and the margin is not close.

A 240 × 320 frame at 16 bits per pixel is **153,600 bytes**, or 1.23 Mbit over SPI per frame:

| SPI bit rate | One full frame | Achievable full-frame rate |
|---|---|---|
| 40 Mbit/s | ≈31 ms | ≈32 FPS, with the entire budget inside the transfer |
| 32 Mbit/s | ≈38 ms | ≈26 FPS |
| 20 Mbit/s | ≈61 ms | ≈16 FPS |

The larger colour display therefore made this budget *harder* than the previous 2 kB
monochrome frame did, even though the CPU is faster. The transfer dominates; the CPU is
not the constraint.

What makes 30 FPS comfortable rather than impossible is that **almost nothing changes
between two Pacman frames.** The maze is static; only Pacman, four ghosts and the
occasional eaten pellet move. Redrawing six 8 × 8 cells is 768 bytes — under 0.2 ms at any
of the rates above.

So the rendering path must transmit only what changed. The design consequence: the display
driver needs a way to push a sub-rectangle, and the view needs to know which cells are
dirty. Measure the achieved rate once the display runs, and only then revisit NFR-002.

## 4. Flashing and debugging

| Tool | Role |
|---|---|
| STM32CubeProgrammer 2.23.0 | Flashing: `STM32_Programmer_CLI -c port=SWD -w build/pacman.elf -v -rst` |
| openocd 0.12.0 | gdb server, and the source of the ST-LINK udev rules |

openocd cannot program this part. It attaches and detects the Cortex-M33, but its flash
driver knows only `STM32U57/U58xx` (device ID 0x482) while this board reports **0x455**
(STM32U535/U545), so `program` fails with `auto_probe failed`. Support for U535/U545 landed
after 0.12.0, which is the newest version the development host packages.

openocd is still worth having for more than gdb: it can halt the target and read registers,
which settles hardware questions without anyone at the board. Reading `GPIOC->IDR` this way
is how the user button's active-HIGH polarity was confirmed. GPIO sits on AHB2 at
`0x42020000` with `0x400` per port and IDR at `+0x10`. Always take a control reading from a
pin whose level is already known — an all-zero register is indistinguishable from a
disabled clock.

## 5. Open questions for M2

1. **Display SPI bit rate** — what the ST7789V and the shield's routing actually sustain,
   which sets the frame budget in §3. The shield is specified to 32 MHz.
2. **Colour format** — RGB565 is assumed throughout; confirm the ST7789V is driven in 16-bit
   pixel mode and that the byte order matches.
3. **Frame buffer placement** — 153.6 kB of the 256 kB contiguous SRAM. Decide whether a
   single buffer suffices or the render path needs two, which would not fit.
4. **Joystick debouncing** — `Bsp/switch` already debounces a GPIO over a 32-sample history;
   confirm it serves five keys without change.
5. **Confirming the display lines on the board** (§1.4) — paper is not silicon.

Settled since this document was written: the pin map (§1), the SB10 decision (§1.2), the
non-existence of the supposed PA9 conflict (§1.3), and the display controller (§6).

## 6. Display controller: ST7789V

The GFX01M2 shipped with two different LCDs, so this had to be read off the board rather
than assumed. The sticker on our shield says **`XNGFX01M2$AZ1`**, which UM2750 §6.3.1 maps to
board `MB1642-TCXD022IB5-D01`, LCD `TCXD022IBLON-5`, driver IC **ST7789V**.

| Product identification | Board | LCD | Driver IC |
|---|---|---|---|
| **`XNGFX01M2$AZ1`** — ours | MB1642-TCXD022IB5-D01 | TCXD022IBLON-5 | **ST7789V** |
| `XNGFX01M2$AZ2` | MB1642-DT022CTFT-D01 | DT022CTFT | ILI9341V |

So the driver targets the **ST7789V**, not the ILI9341 this project previously assumed. The
two are similar in shape but differ in initialisation sequence and in some register
semantics, so the distinction is not academic. The controller can also report its ID over
SPI, which gives the driver a way to verify what it is talking to at run time rather than
trusting the sticker.
