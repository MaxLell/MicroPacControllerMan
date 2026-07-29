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

## 1. Pin mapping — the shield side is known, the board side is not

Two tables are needed and only one of them is in hand.

### 1.1 What the shield puts where (known)

From **UM2750 Rev 3**, tables 12 and 13 — the shield's own ST-morpho *positions*, which are
independent of any board's pin naming. The shield's CN2 mates with the board's CN7, its CN3
with the board's CN10.

| Signal | Shield position | Board connector | Notes |
|---|---|---|---|
| Display SCK | CN3-11 | CN10-11 | |
| Display MISO (SDO) | CN3-13 | CN10-13 | |
| Display MOSI (SDI) | CN3-15 | CN10-15 | |
| Display CS (NCS) | CN3-21 | CN10-21 | **active HIGH** — the manual is explicit |
| Display DCX (WR) | CN3-25 | CN10-25 | data/command select |
| Display RESET | CN2-30 | CN7-30 | active low |
| Display TE (FMARK) | CN2-28 | CN7-28 | tearing effect, optional |
| Joystick LEFT | CN3-17 | CN10-17 | active low |
| Joystick CENTER | CN3-19 | CN10-19 | active low |
| Joystick DOWN | CN3-27 | CN10-27 | active low |
| Joystick RIGHT | CN2-34 | CN7-34 | active low |
| Joystick UP | CN2-38 | CN7-38 | active low |

Interface mode is strapped on the shield by IM0..IM3: **4-line, 8-bit data SPI**.

The unused NOR flash occupies further positions — CN3-16/23/26/28/29/30/31 and
CN2-2/32/35/36/37 — which matters only in that they must be left alone. CN3-3/5 and CN3-33
are reserved for a touch panel the shield does not populate.

### 1.2 What the board has at those positions (NOT known)

**This is the missing half, and it is the one that decides everything.** UM2750's "STM32
GPIO" columns are per-board-family footnotes, and **the document lists no STM32U5 board at
all** — its newest families are G4, WB and WL, and it is dated March 2022.

So UM2750 can say what pin a *NUCLEO-G431RB* has at CN10-21. It cannot say what a
NUCLEO-U545RE-Q has there. Nucleo-64 boards are largely consistent by position, but "largely"
is not a basis for a driver — that assumption is precisely what cost the previous bring-up
its schedule.

The authoritative table is **UM3062, "STM32U3/U5 Nucleo-64 boards (MB1841)"**, which carries
the CN7/CN10 pin assignment for this board. It is not yet in hand; two download attempts
from st.com failed.

For reference only, this is what UM2750 gives for the neighbouring **G431RB/G474RE/G491RE**
group at the same positions. **Treat it as a hypothesis to be checked against UM3062, not as
this project's pin map:**

| Signal | Position | G4-family pin (reference only) |
|---|---|---|
| Display SCK | CN10-11 | PA5 |
| Display MISO | CN10-13 | PA6 |
| Display MOSI | CN10-15 | PA7 |
| Display CS | CN10-21 | PA9 |
| Display DCX | CN10-25 | PB10 |
| Display RESET | CN7-30 | PA1 |
| Display TE | CN7-28 | PA0 |
| Joystick LEFT | CN10-17 | PB6 |
| Joystick CENTER | CN10-19 | PC7 |
| Joystick DOWN | CN10-27 | PB4 |
| Joystick RIGHT | CN7-34 | PB0 |
| Joystick UP | CN7-38 | PC0 |

### 1.3 How it gets settled

1. Obtain **UM3062** and read the CN7/CN10 tables for the NUCLEO-U545RE-Q.
2. **Confirm the result on the board** before writing a display driver. The joystick lines
   can be measured without any instrument: configure candidate pins as inputs with pull-ups,
   press a direction, and see which pin goes low. The display lines need a logic analyzer.

Measuring first is not bureaucracy. On the previous hardware an assumed pin map was the root
cause of a display that stayed blank, and the mistake survived several days of looking for
software faults.

### 1.4 Two conflicts that are likely, pending §1.2

Both follow from the G4 reference mapping. **If** the U545RE-Q carries the same pins at those
positions, both are real; if it does not, both evaporate. They are recorded because each one
would change a decision already taken, so they must be resolved early either way.

**Display CS against the console.** The G4 reference puts CS at CN10-21 on **PA9** — and on
our board the console demonstrably runs on **USART1, PA9/PA10**. CS is hardwired on the
shield and cannot move, and it is active *high*, so a UART transmit line on that pin would
select and deselect the display mid-transfer. Were this to hold, the console would have to
move: either to another USART if the board's solder bridges allow re-routing the ST-LINK VCP,
or off-board to a USB-serial adapter on free pins.

**Display SCK against the LED.** The G4 reference puts SCK at CN10-11 on **PA5**, which on
this board is **LD2**, the green user LED. Nucleo-64 boards share LD2 with the Arduino-header
SPI clock, so this one cannot be designed away if the position holds. Consequence: LD2 would
become an activity indicator, and the `blinky` OTT — which verifies PA5 by driving it and
reading it back — would stop being valid and need reworking or retiring.

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

1. **Which STM32U545RE pin sits at each morpho position** (§1.2) — obtain UM3062. Everything
   else waits on this; the shield side (§1.1) is already settled.
2. **The PA9 conflict** (§1.4) — only if the G4 reference mapping holds for this board. If it
   does: can the ST-LINK VCP be re-routed off USART1 by solder bridge? Also UM3062. This
   would block all display work, so it is checked immediately after item 1.
3. **Display SPI bit rate** — what the controller and the shield's routing actually sustain,
   which sets the frame budget in §3.
4. **Which display controller is on our board.** UM2750 §6.3 shows the GFX01M2 shipped
   with **two different LCDs**, and the driver has to match:

   | Product identification | Board | LCD | Driver IC |
   |---|---|---|---|
   | `XNGFX01M2$AZ1` | MB1642-TCXD022IB5-D01 | TCXD022IBLON-5 | **ST7789V** |
   | `XNGFX01M2$AZ2` | MB1642-DT022CTFT-D01 | DT022CTFT | **ILI9341V** |

   Read it off the stickers on the PCB — the first sticker's second line is the product
   identification, the second sticker's first line the board reference. Both controllers can
   also report an ID over SPI, so the driver can verify at run time once it talks at all.
   Note that earlier project notes claimed ILI9341 outright; that was one of the two
   possibilities, not established fact.

5. **Colour format** — RGB565 is assumed throughout; confirm the controller is driven in
   16-bit pixel mode and that the byte order matches.
6. **Frame buffer placement** — 153.6 kB of the 256 kB contiguous SRAM. Decide whether a
   single buffer suffices or the render path needs two, which would not fit.
7. **Joystick debouncing** — the existing `Bsp/switch` primitive already debounces a GPIO
   over a 32-sample history; confirm it serves five keys without change.
