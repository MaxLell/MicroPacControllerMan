# Test — Verification

Two mechanisms with different jobs. Keeping them apart is the point:

| | Runs on | Verifies | Mocks |
|---|---|---|---|
| **`Host/`** — unit tests | the host, under Ceedling | logic **above the BSP** | yes, the BSP boundary |
| **`Target/`** — on-target tests (OTT) | the STM32 | **the hardware itself** | never |

**What gets a unit test: everything above the BSP.** The BSP is the mocking boundary —
a `Bsp/` header is mocked so the module above it can be tested, but the BSP itself is
not a unit-test subject. Whether a pin is really wired to what we think is not a
question a host test can answer; that is what the OTTs are for, and they are
deliberately *not* unit-tested.

## `Host/` — unit tests (Ceedling + Unity + CMock)

```
ceedling test:all         # every unit test
ceedling test:sw_timer    # one module
ceedling clobber          # throw away build-test/
```

Configuration is `Firmware/project.yml` ([CON-102](../../Docu/PrePlanning/02-Requirements.md)).
A module under test is compiled natively and its dependencies are replaced by
CMock-generated mocks, so a test can drive cases real hardware and real time cannot
reach on demand — the 32-bit tick wrapping around, for instance.

Current coverage: `Services/delay`, `Services/sw_timer`, `Services/framebuffer`,
`Services/gfx`, `Services/message_queue`, `Services/message_broker` and
`Drivers/display` (with `spi_bsp` and `dio_bsp` mocked). The game's
Model/Control tests
([VT-UNIT-001..005](../../Docu/PrePlanning/06-Verification-and-Validation.md)) arrive
with the game itself in M3.

`test_message_broker.c` is the example of what unit tests buy that hardware cannot:
backpressure and slow-consumer isolation are *load* conditions. On the board they happen
rarely, at the worst moment, and leave nothing behind but a dropped input or a frame that
never rendered. Here they are arithmetic. Note it deliberately uses the *real*
`message_queue` rather than mocking it — the broker's logic mostly is queue
orchestration, so a mock would turn the tests into assertions about which functions were
called instead of about what a subscriber receives.

`test_display.c` is worth reading as the example of why the BSP is the mocking boundary:
it captures every byte the panel driver would clock out and asserts on the wire format,
including the inverted bit sense. A polarity mistake there shows up on hardware only as
a photographic negative — no build, and no OTT PASS/FAIL, would catch it.

**Mocking a dependency** — include `mock_<module>.h` instead of `<module>.h` and set
expectations:

```c
#include "mock_systick_bsp.h"

systick_bsp_get_tick_ExpectAndReturn(10U);   /* first call returns 10 */
systick_bsp_get_tick_ExpectAndReturn(15U);   /* second returns 15     */
```

Strict ordering is on, so the calls must happen in exactly that sequence.

**Asserting that an assertion fires** — `Test/support/assert_probe.h` registers itself
as the `custom_assert` handler and aborts the test body when an `ASSERT` trips, so a
violated precondition does not run on into undefined behaviour:

```c
void setUp(void)    { assert_probe_begin(); }
void tearDown(void) { assert_probe_end(); }

ASSERT_PROBE_EXPECT(sw_timer_create(NULL), "in_timer != NULL");
```

**Two gotchas when adding a test:**

- Every test file needs an explicit `#include "custom_assert.h"`. Ceedling picks the
  sources to link from the includes it sees in the *test* file, not transitively, and
  every test executable links the assert probe — without it the link fails on
  `custom_assert_failed`.
- A module with file-scope state needs a reset hook for per-test isolation, since all
  tests in a file share one executable. See `sw_timer_test_reset()`, compiled only
  under `TEST`, called from `setUp()`. `STATIC` from `test_support.h` opens statics up
  the same way.

## `Target/` — On-Target Tests (OTT)

Tests that run on the STM32 itself and report PASS/FAIL over the serial console with no
debugger attached ([doc 09](../../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)).

| File | What |
|---|---|
| `ott.c` / `ott.h` | The OTT core: the `ott` and `reset` console commands, and the boot-time `ott_execute_pending()` runner and reporter. **Owns the layout of the retained request** (magic word, checksum, parameter blob) — `Bsp/retain_ram` only provides the bytes. |
| `ott_scenarios.c` / `.h` | The scenario registry: one row per test, plus the setup/run function types. |
| `scripts/` | One module per scenario, `scripts/ott_<name>.c`/`.h`. |

Current scenarios, all interactive (the firmware renders or prints, you confirm with
the USER button B1, and a safety cap returns the board to nominal mode regardless):

- `ott_user_button` — live button state plus every debounced press; passes after three.
- `ott_display` — geometric GFX patterns on the LCD (VT-INT-006).
- `ott_touchpad` — live x/y and touch-present on the console (VT-INT-007).
- `ott_touchdot` — a dot on the LCD tracks your finger; display and touchpad together.

**Adding a scenario:** create `scripts/ott_<name>.c`/`.h` with a setup and a run
function, add one row to the table in `ott_scenarios.c`, and add the source to
`CMakeLists.txt`. Nothing in the OTT core or the CLI changes.

Scenarios use `Services/sw_timer` for their safety caps and for anything periodic
(sampling, frame rate, heartbeat lines, VCOM service) — no tick arithmetic. Waiting for
the operator is `user_button_take_press()`, which consumes one latched press edge, so no
scenario carries its own debounce or arming logic.

## Host side

- `run_ott.py` — harness that drives an OTT over the VCP and reports PASS/FAIL. Stdlib
  Python only, no pyserial. `--suite` runs the checks a machine can judge on its own
  (enumeration VT-INT-001, boot banner VT-INT-002); a named test streams live with a
  long timeout. Exit 0 = PASS, 1 = FAIL, 2 = timeout.
- `console.py` — a plain interactive terminal on the VCP, for poking at the CLI by hand.
- `support/` — helpers shared by the unit tests (`assert_probe`), linked into every test
  executable by Ceedling.
