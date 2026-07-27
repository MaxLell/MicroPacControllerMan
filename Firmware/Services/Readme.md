# Services — Reusable Middleware

Hardware-independent, reusable building blocks used across layers (no direct register
access, no HAL calls). Pure C, unit-testable on the host — and in practice *this is
where the host-testable code lives*, so prefer putting logic here over pushing it down
into a driver.

**What goes here** — one folder per service, `Services/<service>/<service>.c`/`.h`.

| Service | What |
|---|---|
| `delay/` | The blocking wait — `delay_ms()`. The single place the firmware is allowed to burn time, so it is also the single place to change once FreeRTOS arrives (the body becomes a task delay). |
| `sw_timer/` | The non-blocking half: arm a timer, keep working, and `sw_timer_process()` fires whatever came due. Timers are one-shot; a callback that re-arms its own timer is periodic. |
| `framebuffer/` | A 1-bpp frame buffer: memory plus the bit arithmetic to address it. An object, not a hidden global, so several can exist — the render path is specified to hand on a double-buffered snapshot ([03 §3.2](../../Docu/PrePlanning/03-Architecture.md), R-007). Colours are *logical*: a set bit means ink, and whatever polarity a panel wants is that driver's problem. |
| `gfx/` | Geometric primitives drawn into a frame buffer: lines, rectangles, circles, triangles, filled and outlined. No text or logo. Shapes may hang over the edges; the frame buffer clips them. |

`delay` and `sw_timer` between them replace open-coded tick arithmetic. **There is no
`millis()` in this firmware** — if you find yourself writing `(tick - start) >= timeout`,
you want a `sw_timer`.

`framebuffer` and `gfx` are why a display can be driven without any of the drawing code
knowing what a panel is. Everything above them is testable with no hardware at all.

Candidates as the project grows:

- the pub-sub message broker ([03 §3.2](../../Docu/PrePlanning/03-Architecture.md)),
- the Active-Object base ([§3.5](../../Docu/PrePlanning/03-Architecture.md)),
- an FSM helper, as in the reference project.

**Depends on:** nothing hardware-specific — with one deliberate exception, `delay` and
`sw_timer` read the tick from `Bsp/systick_bsp`, whose header is HAL-free and has a host
implementation. Safe to include from any layer.
