# Services — Reusable Middleware

Hardware-independent, reusable building blocks used across layers (no direct
register access). Pure C, unit-testable on the host.

**What goes here** — one folder per service, `Services/<service>/<service>.c`/`.h`.
Empty for now; candidates as the project grows:
- the pub-sub message broker ([03 Architecture §3.2](../../Docu/PrePlanning/03-Architecture.md)),
- the Active-Object base ([§3.5](../../Docu/PrePlanning/03-Architecture.md)),
- generic helpers (software timers, FSM, delays — as in the reference project).

**Depends on:** nothing hardware-specific. Safe to include from any layer.
