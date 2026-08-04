# M2 Board Bring-Up — Design

Design document for **Milestone 2: Board Bring-Up** — bringing the X-NUCLEO-GFX01M2
display and joystick to life on the STM32U545RE-Q.

This is where the *how* lives: pin assignments, clock settings, transfer budgets, tool
choices and the questions still to be answered. The
[requirements](../PrePlanning/02-Requirements.md) state only *what* the system must do
and deliberately carry none of this detail.

Requirements realised here: CON-002, CON-003, CON-004 (display, joystick, carrier),
FR-004 (directional control), FR-005 / FR-028 (colour rendering), and the two timing budgets
this milestone measured — a 60 FPS frame rate and a 30 ms input latency. Those two were
requirements (NFR-002, NFR-003) while this document was written and are **withdrawn**
now ([DEC-036](../PrePlanning/11-Decisions-and-As-Built.md)); the measurements below stand,
and what they no longer do is decide whether the system passes.

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
| Display CS (NCS) | CN3-21 | CN10-21 | **PC7** | GPIO, **active LOW** — measured; see §1.5 |
| Display DCX (WR) | CN3-25 | CN10-25 | **PB10** | GPIO, data/command select |
| Display RESET | CN2-30 | CN7-30 | **PA1** | GPIO, active low |
| Display TE (FMARK) | CN2-28 | CN7-28 | **PA0** | GPIO in, optional |
| Joystick WEST | CN3-17 | CN10-17 | **PC9** | GPIO in, active low. UM2750 calls it LEFT |
| Joystick CENTER | CN3-19 | CN10-19 | **PC6** | GPIO in, active low |
| Joystick SOUTH | CN3-27 | CN10-27 | **PB4** | GPIO in, active low. UM2750 calls it DOWN |
| Joystick EAST | CN2-34 | CN7-34 | **PB0** | GPIO in, active low. UM2750 calls it RIGHT |
| Joystick NORTH | CN2-38 | CN7-38 | **PC0** | GPIO in, active low. UM2750 calls it UP |

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
| Joystick WEST | CN10-17 | PB6 | **PC9** |
| Joystick CENTER | CN10-19 | PC7 | **PC6** |

The other nine agree, which is exactly what makes the mistake tempting.

### 1.2 SCK shares PA5 with LD2 — and no soldering is needed

Display SCK is **PA5**, which also drives **LD2**, the green user LED. That is a property of
the Nucleo-64 boards, not of the shield.

Two things follow, and they are independent of each other.

**The LED cannot be driven any more, whatever the board's solder bridges say.** Once PA5 is
configured as `SPI1_SCK` in its alternate function, it is no longer a GPIO output and
`dio_bsp` cannot write it. So:

- `DIO_BSP_PIN_LED_GREEN` and the CubeMX `LED_GREEN` output go away.
- The **`blinky` OTT is retired.** It verifies PA5 by driving it and reading it back, which an
  alternate-function pin does not permit. It served its purpose as the first self-judging OTT;
  the joystick test that replaces it can be automatic in the same way for four of its five keys.

**Solder bridge SB10 is a separate, deferrable question, and the default is fine.** UM3062
table 19 offers SB10 to disconnect the LED from PA5, for the reason the manual states — "in
case of a signal issue on SPI-SCK depending on the ARDUINO shield". The obvious worry is that
the LED loads a clock line meant to run at tens of MHz.

That worry is smaller than it looks: UM3062 §7.6 says LD2 is driven **through a transistor**,
so PA5 sees a base resistor and a little capacitance rather than an LED with a series
resistor. ST also ships SB10 **ON** by default, and their own demonstration firmware for this
shield runs on boards wired exactly this way.

**Decision: leave SB10 ON.** The only cost is cosmetic — LD2 flickers with display traffic. If
the display misbehaves at a high SPI clock, opening SB10 is the first thing to try, and the
symptom will then have told us it mattered. Opening it pre-emptively means soldering on
speculation.

### 1.3 What is not a conflict after all

An earlier reading of this project's notes claimed display CS collides with the console's TX
on PA9. **It does not.** CS is on **PC7**; PA9/PA10 sit on CN10-2 and CN10-4, which the
shield leaves unconnected. The console, the ST-LINK VCP and the whole OTT reset flow are
untouched by the shield, and no CubeMX rework is needed for them.

Checked against the current configuration, the only collision with anything already in use
is PA5/LD2 above. PC13 (user button) and PA13/PA14 (SWD) are clear, and PC0, PC6, PC7, PC9,
PB0, PB4, PB10, PA0, PA1, PA6, PA7 are all currently unused.

### 1.4 Measured: the joystick half is confirmed, the display half is not

**Joystick — confirmed on hardware.** The `joystick` OTT names each key as it is pressed and
passes once all five have been seen. Run at the board, all five keys registered *and the name
the firmware printed matched the key that was pressed* — which is the part that matters. A
test that merely passes would also pass with two directions swapped; what closes the question
is the operator seeing NORTH appear when the north key goes down.

So the position cross-reference of §1 holds for these five pins, and with it the method: the
map derived from UM2750 positions against UM3062's assignment is right where it has been
checked.

That run also settles the pull-up question properly. A pin that reads high when released and
low when pressed, with `GPIO_NOPULL` configured, *is* being pulled up externally. An earlier
attempt to show this by reading the pins high over SWD proved nothing on its own — a floating
input reads high too — so this test, not that measurement, is the evidence.

**Display — confirmed on hardware.** The `display_id` OTT resets the controller and reads its
identification registers. It got an answer, which is the only available proof that these pins
are the ones the map claims: a wrong SCK, MOSI, MISO, CS or DCX produces no reply at all.
SCK, MOSI and MISO must be right because data went out and came back; CS because the
controller responded only at one polarity; DCX because the command and data phases were
distinguished. RESET is exercised but not proven — a stuck-high reset line would also let the
controller work. TE stays unused and untested.

**R-009 is closed.** Both halves of the pin map are now measured rather than derived.

### 1.6 The panel works

The `display_test` OTT was run at the board and confirmed by the operator: full red,
green and blue, the eight colour bars in the announced order, and the red border with
the white origin square. So the whole path is proven end to end — SPI, the control pins,
the initialisation sequence, RGB565 pixel format and byte order, and the 240x320 extent.

`MADCTL = 0x00`, the native portrait orientation, is right.

**Display inversion is off, and getting there is worth recording.** It started *on*,
because that is what most ST7789 modules need. The first pass through the flat fills and
the colour bars was confirmed as "the colours all come" — and it was wrong. Every colour
was its own complement, and nobody caught it, because a set of colours in reverse order
still looks like a set of colours.

What caught it was drawing something with a *named expectation*: a yellow disc came out
blue. In RGB565 that is exact — `~0xFFE0 = 0x001F`, yellow's complement is precisely blue
— which turned a vague "looks odd" into a one-line diagnosis. Inversion off, and the disc
is yellow.

The lesson is in the test, not the driver: `display_test` now announces what each screen
*must* look like ("the whole screen must be RED (not cyan)") instead of merely naming what
it drew. A confirmation is only worth as much as the expectation it was checked against.

Note that this settles the *panel's* orientation, not the game's. Which way is "up" for
Pacman is settled separately, by §1.7.

### 1.7 Input and display agree — measured

Both halves were confirmed separately: `joystick` proved the keys, `display_test` proved
the panel. Neither could say whether the two agree, and a swapped axis or an inverted row
order would have passed both.

`joystick_dot` puts them together — a yellow square that moves one cell per key press,
drawn with partial updates only. Run at the board and confirmed by the operator: **the
north key moves the square towards the top of the panel.** With `MADCTL = 0x00` and row 0
at the top, the naive mapping is the correct one, and no rotation is needed anywhere:

| Key | Frame buffer | On the panel |
|---|---|---|
| NORTH | row − 1 | up |
| SOUTH | row + 1 | down |
| WEST | column − 1 | left |
| EAST | column + 1 | right |

So "up" for Pacman needs no translation layer. The keys keep their compass names and the
key-to-direction mapping stays an explicit table, because the *game's* notion of up is a
game decision — but the hardware imposes nothing on it.

### 1.5 Measured: chip select is active LOW, and the controller is an ST7789V

`display_id` reads the identification registers at both chip-select polarities:

```
--- chip select active LOW ---
  RDDID (0x04) -> 42 C2 A9 00
  RDID1 (0xDA) -> 85 FF 00 00
  RDID2 (0xDB) -> 85 FF 00 00
  RDID3 (0xDC) -> 52 00 00 00
--- chip select active HIGH ---
  RDDID (0x04) -> 00 00 00 00        (and likewise for the rest)
```

**Chip select is active LOW.** UM2750 states "SPI chip select active high" in five places, but
it also says that for a NOR flash whose pin is named **CS#** — and the `#` means active low,
which the MX25L6433F datasheet confirms. The statement is a copy-paste error repeated through
the document. The ST7789V datasheet has CSX active low, and the board agrees: at active high
every register reads as zero.

**The controller identifies as an ST7789V.** RDID1/RDID2/RDID3 give **0x85 / 0x85 / 0x52**,
the ST7789 identity, which independently backs up the `XNGFX01M2$AZ1` sticker rather than
relying on it. The driver can make this check at start-up.

**Reads need a one-*bit* dummy, not a one-byte dummy.** The `RDDID` result looks wrong until
you shift it: `42 C2 A9` left by a single bit is exactly `85 85 52`. So the controller emits
one dummy clock cycle before the data, and a byte-aligned read lands off by one bit. That the
shift reproduces the ID exactly is itself confirmation the bus is sound — and it is the kind of
detail that otherwise costs an afternoon, because the data looks like plausible garbage rather
than like nothing.

**A note on tooling, learned the hard way.** Reading pins over SWD while the firmware runs is
not available here: openocd drives this ST-LINK in HLA mode, which offers no memory access on
a running target, and the adapter does not support `dapdirect_swd`. Polling by halt/resume in
a loop breaks the connection. Measurements of this kind belong in firmware, reported over the
console — which is what the OTT mechanism is for.

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

## 3. Frame budget — why 60 FPS needs partial updates

The frame-rate target is at least 60 frames per second — a figure §3.2 arrives at by measurement;
it read 30 while this section was first written, and the conclusion below holds either way.
Neither can be met by transmitting whole frames, and the margin is not close.

A 240 × 320 frame at 16 bits per pixel is **153,600 bytes**, or 1.23 Mbit over SPI per frame:

| SPI bit rate | One full frame | Achievable full-frame rate |
|---|---|---|
| 40 Mbit/s | ≈31 ms | ≈32 FPS, with the entire budget inside the transfer |
| 32 Mbit/s | ≈38 ms | ≈26 FPS |
| 20 Mbit/s | ≈61 ms | ≈16 FPS |

The larger colour display therefore made this budget *harder* than the previous 2 kB
monochrome frame did, even though the CPU is faster. The transfer dominates; the CPU is
not the constraint.

What makes such a rate comfortable rather than impossible is that **almost nothing changes
between two Pacman frames.** The maze is static; only Pacman, four ghosts and the
occasional eaten pellet move. Redrawing six 8 × 8 cells is 768 bytes — under 0.2 ms at any
of the rates above.

So the rendering path must transmit only what changed. The design consequence: the display
driver needs a way to push a sub-rectangle, and the view needs to know which cells are
dirty.

### 3.1 Measured

With the frame buffer and the display port in place, `display_test` times five full-frame
presents through the real path — `framebuffer` -> `gfx` -> `display` -> `st7789`:

```
5 full frames in 1264 ms -> 252 ms/frame, 3 fps
```

At the configured **5 Mbit/s** the table above predicted ~250 ms. The measurement is 252 ms,
so the arithmetic holds and the transfer really is the whole cost — there is no hidden
overhead to hunt for, and no point optimising the drawing side.

**Partial updates settle it, and the SPI clock does not have to move.** The same path,
sending only what a moving game changes — five actors each vacating one 8x8 cell and
entering another, plus a couple of eaten pellets, so twelve cells:

```
5 full frames    in 1264 ms -> 252 ms/frame,  3 fps
100 partial frames in 344 ms -> 3.43 ms/frame, 290 fps
```

Twelve 8x8 cells are 1,536 bytes against 153,600 — **one percent of the data, and 73
times faster.** At 30 FPS that uses **10 % of the frame budget and leaves 90 %** for the
game; at the 60 FPS eventually chosen, 21 % and 79 %. Either rate is reachable at the bit rate
already configured.

Worth noting where the remaining time goes: pure transfer of 1,536 bytes at 5 Mbit/s is
2.46 ms, and the measurement is 3.43 ms. The extra ~0.97 ms is the per-region overhead —
about **81 µs to set a window**, since each region costs a CASET, a RASET and a RAMWR,
each its own short SPI transaction with chip-select and DCX toggling around it. At 290 FPS
that is irrelevant, but it is the thing to attack first if a future frame ever needs
hundreds of small regions: merging adjacent dirty cells into one rectangle removes it.

So neither lever below is needed for now. They are recorded because the arithmetic is
useful, not because there is work outstanding:

- **Raise the SPI clock.** The kernel clock is 160 MHz and the prescaler is a power of two,
  so the choices are 5, 10, 20 or 40 Mbit/s. The shield is specified to 32 MHz, while the
  ST7789V's own write-cycle minimum of 66 ns works out at about 15 MHz — the shield's figure
  and the controller's disagree, so anything above 10 Mbit/s is worth confirming on the panel
  rather than assuming. Following the `uart_bsp` precedent the rate is pinned in firmware
  rather than in the `.ioc`, so changing it is one constant.
- **Send only what changed.** Six 8x8 cells is 768 bytes against 153,600 — under 0.2 ms at
  any of those rates. This is the lever that actually reaches the required rate; the clock
  alone does not, since even 40 Mbit/s leaves a full frame at ~31 ms with nothing left for
  the game.

RAM now reads **157,296 bytes, exactly 60 % of the 256 kB**, which is the frame buffer plus
the 3.5 kB the firmware used before. A second buffer would not fit, so the double-buffered
snapshot assumed in [03 §3.2](../PrePlanning/03-Architecture.md) has to be reconsidered when
M3 resumes. The on-target tests already live under that constraint: they share one buffer
through `ott_framebuffer`, because the second scenario that declared its own would not link.

### 3.2 Smooth is not the same as fast

290 FPS answers how much the panel can take. It does not answer whether Pacman will *look*
smooth, and that is the question the rate was chosen to answer. Smoothness is not a function of the
frame rate on its own but of how far a sprite jumps between two frames — a sprite at 10 FPS
covering nine pixels a frame and one at 60 FPS covering one and a half are the same speed
and look nothing alike.

So `animation` holds the speed constant at **90 px/s** and varies only the rate, five actors
running over a field of pellets, each frame repainting the field behind a sprite and drawing
it at its new place — the work a real frame does, not a square on a blank screen:

| Requested | Held | Step per frame | Drawing time | Of the frame budget |
|---|---|---|---|---|
| 10 FPS | 10 FPS | 9.00 px | 7.09 ms | 7 % of 100 ms |
| 15 FPS | 15 FPS | 6.00 px | 6.55 ms | 9 % of 66.7 ms |
| 30 FPS | 30 FPS | 3.00 px | 5.66 ms | 17 % of 33.3 ms |
| **60 FPS** | **60 FPS** | **1.50 px** | **5.26 ms** | **31 % of 16.7 ms** |

Every pass held the pace it asked for, which is what makes the visual comparison worth
anything. Note that the cost per frame barely falls as the rate doubles — the sprite
dominates the dirty rectangle and the step contributes only a few columns — so **doubling
the rate nearly doubles the data rate.** That is the real ceiling. Unpaced, drawing the same
frames as fast as the path allows, it sits at **175 FPS**.

**The target was therefore raised from 30 FPS to 60.** Three things say 60 rather than more:

- **It costs little.** 5.26 ms of a 16.7 ms frame, so 69 % of every frame still belongs to
  the game — and the measured ceiling is nearly three times the target.
- **The panel cannot show more.** The ST7789V's own refresh runs at its reset default of
  60 Hz, which `st7789_init` does not change (`FRCTRL2` is left alone). Frames sent faster
  than that are overwritten in GRAM before they are ever scanned out.
- **A rate is only worth requiring if it can be judged.** The test hands the rate to the
  joystick after the ladder, so the same motion can be walked up and down while watching it.

One thing to expect at 60 FPS that did not matter at 30 is **tearing**: we write GRAM
unsynchronised to the panel's scan, and at a frame rate equal to the scan rate the two beat
against each other rather than drifting quickly past each other.

**It was looked for at the board and is not there** — the operator saw no seam through the
moving sprites at 60 FPS. That is a negative result at one workload, not a proof: five
16 × 16 sprites in separate rows write little of the panel and finish long before the scan
reaches most of it. A full-width update — a level transition, a score bar spanning the
screen — is where a seam would first show. The tearing-effect output is already wired to
**PA0** and named in `dio_bsp_pin_e`, unused so far, and it is the fix if one ever does;
a lower frame rate is not.

### 3.3 Input latency — the debounce is the whole budget

The budget is 30 ms from a key press to the movement appearing. `joystick_dot` measures the
drawing half through the real path, one move being the cell vacated plus the cell entered:

```
100 moves in 208 ms -> 2.08 ms per move
```

Two 16 × 16 regions are 1,024 bytes, about 1.7 ms of transfer plus the two window
set-ups — so this too is transfer-bound and holds no surprise.

The surprise is the other half. `Bsp/switch` reports a key only after
**`SWITCH_DEBOUNCE_SAMPLES` = 32 consecutive agreeing samples**, and it is sampled from the
1 ms tick, so a settled contact takes **32 ms to be reported** — before anything is drawn.

> **32 ms of debounce + 2.08 ms of drawing = 34 ms, against a 30 ms budget.** The budget is
> spent on debouncing a switch, and the display is not the problem.

Nothing about that is measured-and-therefore-fixed: 32 ms is an unusually long window, chosen
because it happens to be the width of the `uint32_t` the history lives in, not because a
contact needs it. Eight samples is the conventional figure and would put the whole path at
about 10 ms. The window is not changed here — it is shared with `user_button`, where 32 ms is
harmless, and shortening it belongs with the game loop that will actually be judged.
Recorded as [RF-014](../Refactoring-Backlog.md#rf-014). *(The 30 ms budget was withdrawn as a
requirement in [DEC-036](../PrePlanning/11-Decisions-and-As-Built.md); RF-014 stays, because
32 ms was never what a contact needs.)*

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
   confirm it serves five keys without change. *Answered: five instances of the primitive,
   polled from the same 1 ms tick as `user_button`, need no change to it — `Bsp/joystick` is
   the instance module. But the window costs 32 ms, which is the whole input budget; see §3.3.*
5. **Confirming the display lines on the board** (§1.4) — paper is not silicon.

Settled since this document was written: the pin map (§1), the non-existence of the supposed
PA9 conflict (§1.3), the display controller (§6), the frame budget and the smoothness of
motion (§3.1–3.2). SB10 (§1.2) is deliberately left at its default until a measurement says
otherwise.

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
