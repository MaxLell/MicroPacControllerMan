# Test — Verification

Mirrors the reference project's test layout.

## `Target/` — On-Target Tests (OTT)

Tests that run on the STM32G431 itself and report PASS/FAIL over the serial console,
with no debugger attached ([doc 09](../../Docu/PrePlanning/09-OTT-Mechanism-and-Reset-Flow.md)).

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
(sampling, frame rate, heartbeat lines, VCOM service) — no tick arithmetic. Waiting
for the operator is `user_button_take_press()`, which consumes one latched press edge,
so no scenario carries its own debounce or arming logic.

## Host side

- `run_ott.py` — harness that drives an OTT over the VCP and reports PASS/FAIL.
  Stdlib Python only, no pyserial. `--suite` runs the checks a machine can judge on
  its own (enumeration VT-INT-001, boot banner VT-INT-002); a named test streams live
  with a long timeout. Exit 0 = PASS, 1 = FAIL, 2 = timeout.
- `console.py` — a plain interactive terminal on the VCP, for poking at the CLI by
  hand.
- `Host/` — host unit tests under Ceedling/Unity (added from Milestone 3);
  `support/` — the vendored Unity framework.
