# Test — Verification

Mirrors the reference project's test layout.

- `Target/` — **On-Target Tests (OTT)** that run on the STM32G431 and report
  PASS/FAIL over the serial console (doc 09):
  - `ott.c`/`ott.h` — the OTT core (the `ott` CLI command + the boot-time
    `ott_execute_pending()` runner/reporter).
  - `ott_scenarios.c`/`ott_scenarios.h` — the scenario registry (one row per test).
  - `scripts/` — one module per OTT scenario, `scripts/ott_<name>.c`/`.h`
    (e.g. `ott_blinky` for VT-INT-005). **Add a test here + one registry row.**
- `run_ott.py` — host-side harness that drives an OTT over the VCP and reports
  PASS/FAIL (stdlib Python).
- `Host/` — host unit tests under Ceedling/Unity (added from Milestone 3);
  `support/` — the vendored Unity framework.
