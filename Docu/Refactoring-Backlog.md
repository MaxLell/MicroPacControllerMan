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
| [RF-003](#rf-003) | Application is not yet separable from the hardware — input + NVM seams left | **High** | M3 |
| [RF-005](#rf-005) | Two hand-applied edits are lost on every CubeMX re-generation | Medium | — |
| [RF-006](#rf-006) | Display chip-select delay is a spin count, not a duration | Medium | — |
| [RF-007](#rf-007) | Display always pushes the full frame | Deferred — measure first | — |
| [RF-008](#rf-008) | LPUART RX FIFO disabled; characters can still be dropped | Low | — |
| [RF-009](#rf-009) | OTT console keeps partial input across a scenario | Low | — |
| [RF-010](#rf-010) | `spi_bsp_write()` cannot report an error | Low | — |
| [RF-011](#rf-011) | No `ASSERT` handler is registered on the target | Low | — |
| [RF-014](#rf-014) | The 32 ms debounce window is the whole of the NFR-003 input budget | Medium | — |

---

### RF-003

**Application is not yet separable from the hardware.** **High.** *Timing and display
seams cut; input and NVM remain.*

The goal is that the game logic runs on the host with no hardware dependency.

**Done — the pattern.** A platform port is **one shared header, one `.c` per platform,
selected in `CMakeLists.txt`** — never `#ifdef`s inside a module. Two ports exist:

- **Timing.** `systick_bsp.h` with `systick_bsp.c` (SysTick) and `systick_bsp_host.c`
  (`clock_gettime`). `Services/delay` and `Services/sw_timer` build and run on both.
- **Display.** `display.h` is now a display *sink* that shows a `framebuffer_t`, with
  `display.c` (LS013B7DH03 over SPI) and `display_host.c` (headless: keeps the last
  frame, which is what an SDL window will blit). The frame buffer itself moved out into
  `Services/framebuffer` and `gfx` moved to `Services/gfx`, so all the drawing is pure
  logic and unit-tested on the host.

Note what that bought: the panel's inverted bit sense is now confined to `display.c`,
and `test_display.c` pins it down byte-for-byte with the BSP mocked — a polarity
mistake there would otherwise only show up as a photographic negative on the panel,
which no automated check would catch.

**Still to do:**

- **Input.** `Bsp/joystick` reports five named keys, which is a board fact, not the
  semantic level the game wants — a *direction* and a button. That level is what makes a
  keyboard a drop-in host implementation, and it does not exist yet: an M2 test reads
  `joystick_take_press()` directly, which is fine for a test and wrong for the game.
- **NVM.** No module yet; the high score will need the same treatment
  ([FR-005](PrePlanning/02-Requirements.md), NFR-004).
- The message broker and Model/Control do not exist yet; they should be written
  host-first so the dependency never forms.

[§3.8](PrePlanning/03-Architecture.md#38-build--toolchain) anticipates exactly this —
"the host/target split behind Input, Display, NVM and timing is realised as small
platform ports" — and leaves the interfaces to be defined during this phase.

*Done when* Model/Control and the message broker compile and run with no `Bsp/` and no
HAL in the include path, against ports that have a target and a host implementation.

### RF-005

**Two hand-applied edits are lost on every CubeMX re-generation.** Medium.

1. The `.noinit` section in the linker script of `ThirdParty/STM32_U545RE_HAL/`, marked
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
([M2 Board Bring-Up §2](Design/M2-Board-Bring-Up.md))
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
`Drivers/st7789` and `Drivers/display` — a flush "succeeds" either way. That matters more
now than when this was written: the display is the only device on the bus, and a panel
that has stopped answering is indistinguishable from one showing a black screen on
purpose. "Error handling / return conventions" was one of the areas raised for review in
PR #6, but no direction was given, so the existing `void` signature was kept.

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

**It is not only the target.** A unit test whose `setUp` forgets `assert_probe_begin()`
hits the same default: the test binary spins at 100 % CPU with no output, Ceedling never
returns, and nothing anywhere names the cause. That happened while writing
`test_sprite.c` and cost more time than the test did. Severity stays Low because the fix
is one line in a test — but the *diagnosis* is expensive every time, and it will be
expensive again.

*Done when* `app_main()` registers a handler that says what happened — the expression,
file and line over the console, then halt or reset — and the choice is recorded as a
`DEC-xxx`. A reset with a logged reason is the usual pick for a game. Worth considering
alongside it: a library default that prints before it hangs would have turned both
symptoms into a one-line answer.

### RF-014

**The 32 ms debounce window is the whole of the NFR-003 input budget.** Medium.

`switch_get_debounced_state()` reports a key only after `SWITCH_DEBOUNCE_SAMPLES` = **32**
consecutive agreeing samples, and it is sampled from the 1 ms tick, so a settled contact
takes 32 ms to reach the application. NFR-003 allows **30 ms** from press to movement, and
`joystick_dot` measures the drawing half at 2.08 ms — so the path is 34 ms and the display
is not what puts it over ([M2 Board Bring-Up §3.3](Design/M2-Board-Bring-Up.md)).

The window is 32 because that is the width of the `uint32_t` the history shift register
lives in, not because a contact needs it; the `_Static_assert` in `switch.c` ties the two
together. Eight samples is the conventional figure and would bring the whole path to about
10 ms.

Left alone for now deliberately: the primitive is shared with `user_button`, where 32 ms is
harmless, and the number should be chosen against a game loop that can be judged rather than
against an OTT.

*Done when* the debounce length is a per-instance parameter (history in a `uint8_t`, or a
count carried in `switch_t`) and the joystick uses a window that leaves NFR-003 some room —
with the input latency re-measured against the game loop to prove it.
