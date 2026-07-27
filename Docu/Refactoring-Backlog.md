# Refactoring Backlog

Known work that is deliberately **not** done yet: things noticed in passing, deferred
by decision, or blocked on something else. This is a living work list, not a spec —
the [Pre-Planning set](PrePlanning/Index.md) stays the source of truth for *what the
system must do*; this file only tracks *what we owe the codebase*.

Every item says why it matters and what "done" looks like, so it can be picked up
cold. Items use `RF-xxx` IDs, and a fixed entry is **deleted** rather than struck
through — git history keeps the record, and IDs are never reused.

Seeded from the post-M2 structural review (PR #6, 2026-07-27).

## Open items

| ID | Item | Severity | Blocks |
|---|---|---|---|
| [RF-003](#rf-003) | Application is not yet separable from the hardware — first seam cut | **High** | M3 |
| [RF-005](#rf-005) | Two hand-applied edits are lost on every CubeMX re-generation | Medium | — |
| [RF-006](#rf-006) | Display chip-select delay is a spin count, not a duration | Medium | — |
| [RF-007](#rf-007) | Display always pushes the full frame | Deferred — measure first | — |
| [RF-008](#rf-008) | LPUART RX FIFO disabled; characters can still be dropped | Low | — |
| [RF-009](#rf-009) | OTT console keeps partial input across a scenario | Low | — |
| [RF-010](#rf-010) | `spi_bsp_write()` cannot report an error | Low | — |
| [RF-011](#rf-011) | No `ASSERT` handler is registered on the target | Low | — |
| [RF-012](#rf-012) | Slot-2 reset is wired to PD2, firmware drives PA4 | Cosmetic | — |

---

### RF-003

**Application is not yet separable from the hardware.** **High.** *First seam cut.*

The goal is that the game logic runs on the host with no hardware dependency.

**Done so far — the timing seam.** `systick_bsp.h` is HAL-free, so the tick now has two
implementations behind one header: `systick_bsp.c` (SysTick) and
`systick_bsp_host.c` (`clock_gettime(CLOCK_MONOTONIC)`). `Services/delay` and
`Services/sw_timer` therefore build and run on the host unchanged. This is the pattern
every remaining port should copy: shared header, one `.c` per platform, selected in
`CMakeLists.txt` — not `#ifdef`s inside a module.

**Still to do:**

- `Drivers/display` and `Drivers/touchpad` talk to `Bsp/spi_bsp`, `Bsp/i2c_bsp` and
  `Bsp/dio_bsp` directly, so nothing above them can be built without the HAL. These
  need a port at the *driver* boundary (a display sink, an input source), not at the
  bus boundary — mocking SPI on the host would be pointless work.
- There is no NVM module yet, and the high score will need the same treatment.
- The message broker and Model/Control do not exist yet; they should be written
  host-first so the dependency never forms.

[§3.8](PrePlanning/03-Architecture.md#38-build--toolchain) already anticipates this —
"the host/target split behind Input, Display, NVM and timing is realised as small
platform ports" — and explicitly leaves the interfaces to be defined during this phase.

*Done when* Model/Control and the message broker compile and run with no `Bsp/` and no
HAL in the include path, against platform ports that have a target and a host
implementation. This is M3 work, not a cleanup; it is listed here so the seam locations
are agreed before game code is written on top of them.

### RF-005

**Two hand-applied edits are lost on every CubeMX re-generation.** Medium.

1. The `.noinit` section in `ThirdParty/STM32_G431RB_HAL/STM32G431xx_FLASH.ld`, marked
   NON-GENERATED. Without it the OTT retained-RAM reset flow stops working — and
   *silently*: the build succeeds and every OTT simply looks like a normal boot.
2. The `app_main()` call in the USER CODE block of the generated `Core/Src/main.c`.
   This one fails loudly, so it is the lesser problem.

Raised as a wart in the PR #6 review notes. Item 1 deserves a guard rather than a
README note.

*Done when* a re-generation cannot silently break the reset flow — e.g. a link-time or
start-up check that `__noinit_start__`/`__noinit_end__` bracket a buffer of the
expected size, or a build step that fails if the section is absent.

(A third, purely cosmetic case: `Core/Src/gpio.c` carries hand-added explanations on
its generated comments. A re-generation drops them but emits identical code, so nothing
breaks.)

### RF-006

**Display chip-select delay is a spin count, not a duration.** Medium.

`DISPLAY_CHIP_SELECT_SETTLE_LOOPS` in `Drivers/display/display.c` is a busy loop, so it
scales inversely with the core clock. It was tuned at 16 MHz, and the move to 170 MHz
([02 §2.3.4](PrePlanning/02-Requirements.md#234-clock-configuration-as-configured))
shortened it by ~10× to an estimated 6–11 µs against the panel's ~6 µs setup
requirement — it was raised to restore margin, but the value is still empirical and
will drift again on any clock or compiler change.

*Done when* the delay is expressed in microseconds against a real time source (a DWT
cycle counter, or a µs helper in `Services/delay`) rather than an iteration count.

### RF-007

**Display always pushes the full frame.** *Deferred by decision — measure first.*

A full `display_flush()` is 2306 bytes (128 lines × [1 address + 16 data + 1 padding]
+ header + trailer). At ~664 kbit/s that is **≈28 ms, about 83 % of a 30 FPS frame
budget** ([R-004](PrePlanning/05-Risks-Assumptions-and-Dependencies.md#51-risks)).
The panel accepts a write of only the changed lines, and in Pacman most of the maze is
static between frames, so dirty-line tracking would cut the dominant cost directly.

Note the *obvious* lever does not work: the SPI prescaler is powers-of-two, so the next
step up (÷128) is 1.33 MHz and overshoots the LS013B7DH03's ~1.1 MHz ceiling.

**Explicitly deferred:** the decision is to get the finished game running first and see
whether it is smooth. Only if it is not do we come back here. Do not optimise this
speculatively.

*Done when* — if it is needed at all — `display_flush()` pushes only dirty lines, with
a measured before/after frame rate.

### RF-008

**LPUART RX FIFO disabled; characters can still be dropped.** Low.

The firmware echoes each received character over a blocking UART TX, so it is deaf for
~90 µs per character. A host that writes a command in one burst overruns the single-byte
RX register. The overrun no longer *wedges* the console — `uart_bsp_read_character()`
clears `ORE`, which previously latched and killed reception permanently — and
`run_ott.py` now paces its writes, so the path we use is sound. But a fast or
third-party client still loses bytes.

*Done when* reception tolerates a burst: enable the LPUART RX FIFO (**requires
CubeMX** — a peripheral setting, so not a hand-edit of the `.ioc`; note that
`UART_FLAG_RXNE` becomes the FIFO-not-empty flag and the read logic must be re-checked),
or move RX to interrupt-driven with a ring buffer in `uart_bsp`.

### RF-009

**OTT console keeps partial input across a scenario.** Low.

While a scenario runs, `ott_poll()` is not called, so nothing drains the RX register.
Characters that arrive during a scenario are partly lost and partly left in the CLI's
line buffer, so the *next* command can be prefixed with a fragment and fail as an
unknown command. Observed during PR #6 verification: a leftover `r` turned a later
`reset` into `rreset`.

`run_ott.py`'s `wait_until_idle()` avoids this in normal use, so this only bites
interactive or out-of-band poking.

*Done when* the CLI line buffer is cleared before a scenario's verdict is printed, or
RX is buffered across scenarios (see RF-008).

### RF-010

**`spi_bsp_write()` cannot report an error.** Low.

It returns `void` and discards the HAL status, so a stuck bus is invisible to
`Drivers/display` — a flush "succeeds" either way. `i2c_bsp` by contrast returns
`i2c_bsp_status_e`, which the touchpad driver and its OTTs propagate. "Error handling /
return conventions" was one of the areas raised for review in PR #6, but no direction
was given, so the existing `void` signature was kept.

*Done when* a project-wide return convention is decided and applied consistently across
the BSP.

### RF-011

**No `ASSERT` handler is registered on the target.** Low.

Release behaviour is settled: the vendored `custom_assert` compiles assertions away
entirely under `NDEBUG`, as the [coding standard](https://github.com/MaxLell/c-code-style)
requires. There is no Release build yet, so nothing depends on it.

What is left is the *debug* target behaviour. `custom_assert_failed()` dispatches to a
handler registered with `custom_assert_init()`, and the firmware registers none — so a
failed assertion falls back to the library default, an infinite loop. The board halts,
which is the right outcome for a detected bug, but it halts silently: nothing is printed
and, unlike the old `bkpt`, no fault is raised for a debugger to catch. Under an OTT the
symptom is a harness timeout with no explanation.

*Done when* `app_main()` registers a handler that says what happened — the expression,
file and line over the console, then halt or reset — and the choice is recorded as a
`DEC-xxx`. A reset with a logged reason is the usual pick for a game.

### RF-012

**Slot-2 reset is wired to PD2, firmware drives PA4.** Cosmetic.

The Click Shield routes slot-2 `RST` to **PD2**, but the firmware configures and drives
**PA4** ([DEC-008](PrePlanning/11-Decisions-and-As-Built.md),
[02 §2.3.3](PrePlanning/02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001)).
The MTCH6102 boots without an explicit reset, so the touchpad works regardless — the
reset pulse simply goes to an unconnected pin.

*Done when* either the pin is corrected in CubeMX, or the reset is dropped from
`touchpad_init()` and the dead pin removed from `dio_bsp_pin_e` and the `.ioc`. The
second is probably the better trade, since the controller does not need it.
