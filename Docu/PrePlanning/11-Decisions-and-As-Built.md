# 11 Decisions & As-Built

[← Back to Index](Index.md)

This document records **implementation decisions actually taken** and where the
built firmware **deviates from the intended design** described elsewhere in this
set. It exists so a reader starting from zero can tell what is true *today* (as
built) from what is *planned*. Requirements ([02](02-Requirements.md)) state the
target; this log states the current reality and the reasoning.

Decisions carry a `DEC-xxx` ID. When a decision is superseded, mark it and add a
new row rather than editing history.

## 11.1 Decisions log

| ID | Decision | Milestone | Rationale | Revisit / links |
|---|---|---|---|---|
| DEC-001 | **Register-level CMSIS, no vendor HAL and no STM32CubeMX.** Peripheral init is written directly against CMSIS headers (`App/main.c`, `Bsp/*`). | M1 | Full control and minimal dependencies for a small, well-understood bring-up; matches the bare-metal style of the reference project. Supersedes the earlier "CubeMX generates init/HAL" wording in [§3.8](03-Architecture.md#38-build--toolchain) / [D-003](05-Risks-Assumptions-and-Dependencies.md#53-dependencies). | Reassess only if a peripheral becomes impractical to drive at register level. |
| DEC-002 | **FreeRTOS is not yet integrated;** M1 is a bare super-loop (`main.c`). | M1 | Bring-up needs no scheduler; adding it early would complicate the first end-to-end proof. CON-104 (FreeRTOS) still holds as a target-state constraint. | Introduce during **M4 System Integration**, per [04 Milestones](04-Implementation-Phases-and-Milestones.md). |
| DEC-003 | **Serial console on LPUART1, pins PA2/PA3, 115200 8N1.** | M1 | On the Nucleo-G431RB the ST-LINK virtual COM port is wired to PA2/PA3, and on this part those pins map to **LPUART1** (not USART2). Verified on hardware (commit `09d5f6d`). | Fixed for the console; unrelated to the mikroBUS mapping (R-001). |
| DEC-004 | **Default reset clock: HSI 16 MHz, no PLL.** | M1 | Sufficient for blink + serial; avoids clock-tree tuning during bring-up. | Revisit if the render/timing budgets ([NFR-002](02-Requirements.md)/[NFR-003](02-Requirements.md), [R-004](05-Risks-Assumptions-and-Dependencies.md#51-risks)) need more core clock. |
| DEC-005 | **OTT retained-RAM/reset mechanism brought forward from M2 into M1**, and implemented per [doc 09 §9.6](09-OTT-Mechanism-and-Reset-Flow.md): request stored in a `.noinit` buffer with **magic word `0xB007A5A5` + a multiplicative checksum** over the spec fields; validated & invalidated on boot; result printed over the console before falling through to nominal mode (no second reset). | M1 | Building the framework once lets M2's display/touchpad tests slot in as pure scenario modules. | [09](09-OTT-Mechanism-and-Reset-Flow.md), [06 VT-INT-005](06-Verification-and-Validation.md). |
| DEC-006 | **Layered source tree** (`App / Bsp / Drivers / Services / Test / ThirdParty`, one folder per module) modelled on the reference project; device/vendor code (CMSIS, startup, linker) grouped under `ThirdParty/STM32G431/`. | M1 | Navigability and parity with the reference; keeps ST/ARM code out of the architecture layers. | Binding rules: [03 Architecture §3.9](03-Architecture.md#39-firmware-source-tree-layout). |
| DEC-007 | **EmbeddedCli vendored** as the OTT console CLI, carrying the two upstream memory-safety fixes (EmbeddedCli PR #2). | M1 | Reuse a known CLI parser rather than hand-rolling one. | [D-007](05-Risks-Assumptions-and-Dependencies.md#53-dependencies), [§3.7](03-Architecture.md#37-on-target-test-ott-cli-framework). |
| DEC-008 | **mikroBUS↔GPIO pin map derived from documentation, not yet HW-confirmed.** Slot 1 = SPI1 (PA5/PA6/PA7, AF5) + CS PB6 + DISP PA6 + EXTCOMIN PB10; slot 2 = I2C1 (PB8/PB9, AF4) + RST PA4; button PC13. Recorded in [02 §2.3.3](02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001). | M2 | Unblocks the drivers now; the continuity check ([VT-INT-003/004](06-Verification-and-Validation.md)) confirms it and closes [R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks). | Re-verify SCL/SDA (PB8/PB9), the active-HIGH CS pins, and DISP-on-MISO. |
| DEC-009 | **Register-level SPI/I2C BSP + display/touchpad drivers, no HAL** (`Bsp/spi`, `Bsp/i2c`, `Bsp/button`, `Bsp/systick`; `Drivers/display`, `Drivers/gfx`, `Drivers/touchpad`). Display uses a RAM framebuffer + **software VCOM** (M1 bit toggled per flush, EXTCOMIN also pulsed) so either EXTMODE jumper setting works; I2C calls are timeout-bounded so a missing device fails cleanly. | M2 | Consistent with DEC-001; MTCH6102 needs no config writes (boots in Full mode); LS013B7DH03 is write-only. | [03 §3.9](03-Architecture.md#39-firmware-source-tree-layout). |
| DEC-010 | **M2 OTT scenarios shaped per the owner's request:** `ott touchpad` streams live X/Y + touch-present over the **console** (MTCH6102 has no Z/pressure); `ott display` renders geometric GFX patterns (lines/rects/circles/triangles, **no logo**); `ott touchdot` moves a dot on the LCD to follow the finger (combined display+touchpad). All three end on a USER-button press (with a 120 s safety cap). | M2 | Matches how the owner verifies on the bench; supersedes the display-shows-coordinates wording of [VT-INT-007](06-Verification-and-Validation.md). | [06 VT-INT-006/007](06-Verification-and-Validation.md). |
| DEC-011 | **`reset` CLI command added** and the Python harness gained an **automatic suite** (`run_ott.py --suite`: enumeration VT-INT-001, boot-banner VT-INT-002 via `reset`, blinky VT-INT-005) plus a streaming mode for the interactive tests. | M2 | Gives the harness a reproducible boot-banner trigger and a one-command regression over the machine-judgeable tests. | [06 §6.3](06-Verification-and-Validation.md#63-test-harness-python). |

## 11.2 As-built vs. intended design

Where the current firmware differs from the target design in this doc set:

| Topic | Intended (spec) | As built (today) | Reconciled by |
|---|---|---|---|
| Init code generation | CubeMX-generated init/HAL ([§3.8](03-Architecture.md#38-build--toolchain), [D-003](05-Risks-Assumptions-and-Dependencies.md#53-dependencies)) | Hand-written register-level CMSIS, no CubeMX/HAL | DEC-001 |
| RTOS | FreeRTOS ([CON-104](02-Requirements.md), [§3.4](03-Architecture.md#34-freertos-task-breakdown)) | Bare super-loop; no RTOS yet | DEC-002 (planned M4) |
| Pub-sub broker | Queue-based broker + Active-Object modules ([§3.2](03-Architecture.md#32-message-broker), [§3.5](03-Architecture.md#35-generic-software-module-template-active-object)) | Not yet implemented | Planned M3/M4 |

Everything else in the firmware matches the design as written. Items deferred **by
design** (not deviations) — the **HW-confirmed** mikroBUS pin map ([R-001](05-Risks-Assumptions-and-Dependencies.md#51-risks);
a *derived* map now exists, DEC-008/[§2.3.3](02-Requirements.md#233-mikrobus--stm32g431-pin-mapping-con-004--r-001), pending continuity check),
the NVM high-score format ([A-004](05-Risks-Assumptions-and-Dependencies.md#52-assumptions)),
and the host/SDL platform-port interfaces ([§3.8](03-Architecture.md#38-build--toolchain)) —
remain tracked in their respective docs and are not repeated here.

## 11.3 Status

Opened at the end of Milestone 1. Extend it whenever a non-obvious implementation
choice is made or the build diverges from the spec, so the "as-built" picture
stays current across sessions.
