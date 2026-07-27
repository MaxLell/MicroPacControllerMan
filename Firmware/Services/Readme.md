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
| `circular_buffer/` | A generic fixed-capacity FIFO ring buffer of same-sized elements, caller-supplied storage (no heap, NFR-103). Element-type-agnostic — it moves `element_size` bytes and never looks at them. A component in its own right because the ring arithmetic is identical whatever is queued, and it is where off-by-one and wrap-around bugs live. |
| `msg/` | The shared vocabulary: topic IDs, payload types, and the fixed-size envelope. Header-only — a vocabulary has no behaviour. Transcribed from [03 §3.3](../../Docu/PrePlanning/03-Architecture.md), which stays the authority. |
| `msg_queue/` | A type-safe skin over `circular_buffer` so callers pass `msg_t` instead of `void*` and cannot get the element size wrong at a call site. |
| `msg_broker/` | The publish/subscribe bus — the only path between modules (FR-103). Instance-based, so the two brokers of FR-110 cannot interfere; subscribers register an output queue rather than a callback, so a publisher never waits on a consumer (§3.5, FR-109). Content-agnostic: it routes on the topic and never reads a payload. |

`delay` and `sw_timer` between them replace open-coded tick arithmetic. **There is no
`millis()` in this firmware** — if you find yourself writing `(tick - start) >= timeout`,
you want a `sw_timer`.

`framebuffer` and `gfx` are why a display can be driven without any of the drawing code
knowing what a panel is. Everything above them is testable with no hardware at all.

The message modules stack rather than duplicate: `msg_broker` → `msg_queue` →
`circular_buffer`. Only the bottom one does ring arithmetic, and only it needs tests for
wrap-around.

The broker deserves two notes, because both are deliberate departures from the
reference implementation it was adapted from
([MovyDesk_Prototype](https://github.com/MaxLell/MovyDesk_Prototype/tree/main/lib/MessageBroker)):

- **A queue, not a callback.** A callback runs on the publisher's stack, which makes a
  slow subscriber the publisher's problem and invites re-entrancy.
- **An object, not a singleton.** Every call takes the instance first and all state
  lives inside it. Note the one place this changes behaviour: `message_broker_init()`
  does *not* assert "not already initialized" the way the singleton did — on an
  instance that would read uninitialized memory.

**Threading:** FR-108 wants a worker task per broker. Until FreeRTOS lands in M4,
`message_broker_process()` is that worker's body and the super-loop calls it. The API
does not change when the task appears.

Candidates as the project grows:

- the Active-Object base ([§3.5](../../Docu/PrePlanning/03-Architecture.md)),
- an FSM helper, as in the reference project.

**Depends on:** nothing hardware-specific — with one deliberate exception, `delay` and
`sw_timer` read the tick from `Bsp/systick_bsp`, whose header is HAL-free and has a host
implementation. Safe to include from any layer.
