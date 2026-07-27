# embedded_utils (vendored)

Verbatim copy of `utils/` from [MaxLell/embedded_utils](https://github.com/MaxLell/embedded_utils)
(MIT — the licence text is in the file headers). The reference project
[BareMetalHollowClockFw](https://github.com/MaxLell/BareMetalHollowClockFw) pulls the
same code in as a git submodule; this project vendors it as plain files, matching how
`EmbeddedCli/` is handled here.

- `custom_assert.h` / `.c` — the project's `ASSERT`. Fatal-error handling for
  programming mistakes (FR-111): a failed assertion calls `custom_assert_failed()`,
  which dispatches to a handler registered with `custom_assert_init()` and otherwise
  halts. Compiles away entirely under `NDEBUG`.
- `test_support.h` — `STATIC`, which is `static` normally and empty under `TEST`, so
  unit tests can reach a module's internals.

Do not edit these files. Fixes belong upstream, then get re-vendored — see the
project's git workflow in `CLAUDE.md`.
