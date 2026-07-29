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

## 1. Pin mapping — derived from UM2750, not yet measured

Taken from **UM2750 Rev 3** (March 2022), tables 3, 6 and 9. The document's GPIO columns
are per-board-family through footnotes, and **it lists no STM32U5 board at all** — the
newest families it knows are G4, WB and WL. The assignments below are the ones it gives
for the **NUCLEO-G431RB / G474RE / G491RE** group, which is the closest documented
neighbour and, being Nucleo-64 ST-morpho, occupies the same header positions.

That inference is exactly what has to be *measured* before it is trusted (§1.1).

**Display (LCD1, "SPIA" in the manual)** — UM2750 table 3:

| Function | Shield signal | STM32 pin | Notes |
|---|---|---|---|
| SCK | SPIA_SCK | PA5 | SPI1_SCK on this part |
| MOSI | SPIA_MOSI (SDI) | PA7 | |
| MISO | SPIA_MISO (SDO) | PA6 | display can be read back |
| CS | SPIA_NCS | PA9 | **active HIGH** — the manual is explicit, and it is easy to get backwards |
| DCX | SPIA_DCX (WR) | PB10 | data/command select |
| RESET | DISP_NRESET | PA1 | active low |
| TE | DISP_TE (FMARK) | PA0 | tearing effect, optional |

Interface mode is fixed on the shield by strapping IM0..IM3: **4-line, 8-bit data SPI**.

**Joystick (B1)** — UM2750 table 9. Five discrete keys, all **active low**, common to GND:

| Key | STM32 pin |
|---|---|
| Left | PB6 |
| Right | PB0 |
| Up | PC0 |
| Down | PB4 |
| Centre | PC7 |

**SPI NOR flash (U1)** — UM2750 table 6, default dual-SPI configuration (solder bridges
SB4/SB5/SB6 fitted). Unused here, but the pins are occupied and must be left alone:
CS# **PA8**, SCK **PB13**, MISO **PB14**, MOSI **PB15**.

### 1.1 How it gets settled

**Confirm every signal on the board with a logic analyzer before a single line of display
driver is written.** The table above is an inference from a neighbouring board family in a
document that predates this microcontroller — it is a starting point, not a fact.

That order is not bureaucracy. On the previous hardware an *assumed* pin map was the root
cause of a display that stayed blank, and the mistake survived several days of looking for
software faults. Measuring first is cheaper than debugging afterwards.

### 1.2 Two confirmed pin conflicts

Both are real, and both were found by reading the manual rather than by debugging.

**Display CS collides with the console.** CS is on **PA9**, which is **USART1_TX** — the
ST-LINK virtual COM port this project's console and its whole OTT mechanism run on. This is
not a theoretical clash: the console demonstrably works on PA9/PA10 today. CS cannot move,
because the shield hardwires it to a fixed morpho position. So **the console has to move, or
the display cannot be driven.**

Worse than an ordinary conflict: CS is active *high*, so if PA9 stays a UART transmit line
its level would follow serial traffic and select/deselect the display in the middle of
transfers.

Open question for this: whether the NUCLEO-U545RE-Q lets the ST-LINK VCP be re-routed to
another USART via solder bridges. That needs the board's own user manual. If it cannot be
moved, the options narrow to giving up the on-board VCP for an external USB-serial adapter
on free pins, or cutting the shield's CS trace and jumpering CS elsewhere.

**Display SCK collides with the LED.** SCK is on **PA5**, which is also **LD2** — the green
user LED. That is a property of the Nucleo-64 boards themselves, where LD2 shares PA5 with
the Arduino-header SPI clock, so it cannot be designed away.

Consequence: once SPI1 drives PA5, LD2 stops being an independent output and becomes an
activity indicator that flickers with the display clock. The **`blinky` OTT drives PA5 and
will therefore have to change or be retired in M2** — it currently verifies the pin by
driving it and reading it back, which the SPI peripheral will no longer permit.

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

1. **The pin map** (§1) — derived from UM2750; still to be confirmed with a logic analyzer.
2. **The PA9 conflict** (§1.2) — confirmed to exist. Open: can the ST-LINK VCP be re-routed
   off USART1 by solder bridge on this board? Needs the NUCLEO-U545RE-Q user manual. This
   blocks all display work, so it goes first.
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
