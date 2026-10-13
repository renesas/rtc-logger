# RTC Internal Timestamp Event Logger – User Manual

**Short ID:** `rs_rtc_logger`
**Version:** 1.0.0
**FSP Baseline:** FSP v6.5.0
**Primary Target:** FPB-RA4E2
**Secondary Target:** FPB-RA6E2
**License:** BSD-3-Clause

---

## About This Manual

This manual covers the core use case, capabilities, and reusability model of the
`rs_rtc_logger`. It is the starting point for evaluating and adapting the
module. Detailed, task-specific documentation has been split into companion
guides — see the [Documentation Map](#documentation-map) below.

## Documentation Map

| Document | What it covers | Read this when you are... |
|---|---|---|
| **RTC_EVENT_LOGGER-USER_MANUAL.md** *(this file)* | Use case, core capabilities, dual operating modes, reusability model | Evaluating the module for the first time |
| [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md) | Importing/building the example project, expected VCOM output | Running the reference demo for the first time |
| [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) | Reference board assumptions, required non-default FSP settings, adapting to a new board, dependencies | Porting to your own board or peripherals |
| [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md) | Every function — public **and** private/internal, each clearly tagged — naming conventions, state gating, hot-safe rules | Writing code against, or debugging inside, `rs_rtc_logger` |
| [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) | Initialization order, task/RTOS integration, warm-reset ordering, multi-instance considerations | Wiring the RTC Event Logger into your application |
| [FILE_GUIDE.md](FILE_GUIDE.md) | Core vs. demo vs. FSP-generated files, naming conventions, what is safe to modify | Deciding what to copy, keep, or replace |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Lifecycle state machine, function state gating, event/telemetry flow | Understanding internal design or debugging state issues |
| [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md) | Demonstrated behavior, ISR/UART timing, RAM budget, clock accuracy | Validating the module meets your timing/memory budget |
| [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md) | Adapting the core logic to other event sources and use cases | Reusing the module for a different use case |
| [RELEASE_NOTES.md](RELEASE_NOTES.md) | What changes between releases, merge guidance | Adopting a newer release of this module |
---

## Table of Contents

1. [What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate)
2. [What Does the Core Code Do?](#2-what-does-the-core-code-do)
   - [2.1 Dual Operating Modes](#21-dual-operating-modes)
3. [Core Code vs. Reference Application Code](#3-core-code-vs-reference-application-code)
4. [Where to Go Next](#4-where-to-go-next)

---

## 1. What Use Case Does This Module Demonstrate?

This reference code demonstrates **timestamped serial event logging using the MCU's on-chip Real-Time Clock (RTC)**. It addresses the recurring embedded development challenge of reliably capturing a human-readable, time-anchored record of discrete events and transmitting those records over a UART debug/logging channel.

The core challenge the module solves is two-fold. First, RTC timestamp capture must happen at the exact moment an interrupt fires, without blocking or delaying the ISR. Second, that timestamp must survive system resets — specifically, the RTC configuration and event sequence count must be preserved across warm resets (software reset, pin reset, watchdog reset) while power remains active, so logging can resume seamlessly without losing event ordering.

### Representative Application Verticals

| Vertical | Representative Events |
|---|---|
| Smart Utility & Energy Metering | Fault detection, tamper events, power-grid failures |
| Industrial Automation & PLCs | Equipment power-downs, over-temperature faults, packet losses |
| Remote IoT & Environment Sensors | Anomaly detection, battery depletion events |
| Medical Devices (Wearables) | Anomaly detection, power-on status retention |
| Cold Chain & Asset Tracking | Temperature threshold crossings, packet drops |

The module encapsulates the RTC HAL driver (`r_rtc` / `rtc_instance_t`) directly, and reaches UART output through a shared, vendored Buffered I/O UART driver rather than opening a `uart_instance_t` itself (see [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update)). The External IRQ driver (`r_icu`) is owned and managed by the application layer; any interrupt source calls `rs_rtc_logger_EventTrigger()` to trigger a timestamped log entry. Developers do not need to manually orchestrate RTC reads, timestamp formatting, string composition, and UART transmission in every project.

---

## 2. What Does the Core Code Do?

The core module provides the following capabilities:

**RTC Timestamp Capture**
On every `rs_rtc_logger_EventTrigger()` call, the module immediately reads the current calendar time from the MCU's on-chip RTC via `R_RTC_CalendarTimeGet()`. The timestamp is captured at ISR entry time — before any queuing or formatting occurs — so the logged time reflects the exact moment of the event. The `rtc_time_t` struct fields are converted to display format using integer arithmetic only (no floating-point).

**Event Buffering via Circular Buffer**
Captured events (timestamp + sequence count + payload) are stored in a static circular buffer inside `rs_rtc_logger_ctrl_t`. The buffer decouples ISR-time capture from UART transmit time. `EventTrigger()` returns immediately after enqueuing; it never blocks the calling ISR context. Buffer depth defaults to 8 entries and is configurable at build time.

**UART Transmission via Shared Buffered I/O**
A FreeRTOS consumer task (module-owned in Example Project mode, application-owned in Expert mode) drains the circular buffer and calls `rs_rtc_logger_Write()` to format and transmit each entry over UART. UART writes go through `fwrite()`/`fflush(stdout)` on a shared stdio stream owned by a vendored Buffered I/O UART driver, rather than through a UART instance this module opens and tracks itself. The call returns once the write has been safely submitted (the driver briefly serializes internally if a previous transmission is still in flight). See [ARCHITECTURE.md](ARCHITECTURE.md#3-event-capture-and-telemetry-flow) for the full flow and [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update) for what changed.

**Formatted Output**
The default output format is:

```
[YYYY-MM-DD HH:MM:SS] Event Detected #<count>
```

For example: `[2024-07-15 14:30:05] Event Detected #3`

Additional formats are available: time-only `[HH:MM:SS]`, compact `[YYYYMMDD-HHMMSS]`, and syslog-style `[Mon DD HH:MM:SS]`. Format is selectable at build time or changeable at runtime via `rs_rtc_logger_TimestampFormatSet()`.

**Warm Reset Preservation**
On a warm reset (any reset other than power-on reset), the RTC continues running and retains its calendar configuration. The module relies on the FSP `r_rtc` build-time property *Set Source Clock in Open* being set to `Disabled`, so `R_RTC_ClockSourceSet()` is called only when a power-on reset is detected. The event sequence counter is preserved by the reference application using a `BSP_SECTION_NOINIT` magic-sentinel pattern and restored via `rs_rtc_logger_SeqCountSet()` after `Open()`. Power-on-reset detection reads the sticky `R_SYSTEM->RSTSR0_b.PORF` flag and then explicitly clears it, so the flag accurately reflects only the reset that just occurred and every subsequent reset — including genuine warm resets — is correctly identified rather than being re-treated as another power-on reset. See [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update).

**Debounced Event Source (Reference Application)**
The reference application's user-button ISR callback (`demo_button_s1_callback()`) applies a software debounce, in addition to the r_icu hardware digital filter, before calling `rs_rtc_logger_EventTrigger()`. This is demo-layer behavior, not core module behavior — the core module accepts every `EventTrigger()` call it receives regardless of source. See [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application) for tuning details.

**LOCO Clock Source**
The RTC is clocked from the LOCO (Low-speed On-Chip Oscillator), which is the FSP default for the RA4E2. LOCO is less accurate than a 32.768 kHz sub-clock crystal (which is fitted on the FPB-RA4E2 as Y2 but is not used by default). For applications requiring higher time accuracy, the sub-clock alternative is documented in [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md). A runtime LOCO drift calibration function `rs_rtc_logger_LocoCalibrationSet()` is provided to adjust the RFRL register — approximately 3906 ppm per RFRL unit, which is roughly 3.5 seconds per 15 minutes per unit.

**Power-Down Retention Limitation (FPB-RA4E2)**
The RA4E2 in its 64-pin QFN package (R7FA4E2B93CFM) does not expose a VBATT pin, and the FPB-RA4E2 board has no battery connector or VBATT supply circuit. RTC time is therefore **not preserved on power-down** on this board. RTC state is preserved across warm resets while USB power is maintained. Battery-backed power-down retention requires a target board with VBATT supply; the FPB-RA6E2 VBATT support status should be confirmed against its schematic.

---

## 2.1 Dual Operating Modes

The module supports two operating modes selected at compile time via `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` in `rs_rtc_logger_cfg.h`. Both modes share the same core source files.

| Aspect | Example Project Mode (`=1`, default) | Expert Mode (`=0`) |
|---|---|---|
| Purpose | Onboarding, demos, evaluation | Production use requiring precise control |
| Application entry point | `rs_rtc_logger_EventTrigger()` only | Full lifecycle: `rs_rtc_logger_Open()`, `rs_rtc_logger_Start()`, `rs_rtc_logger_Stop()`, `rs_rtc_logger_Close()`, `rs_rtc_logger_EventTrigger()`, `rs_rtc_logger_EventDequeue()`, `rs_rtc_logger_Write()`, `rs_rtc_logger_WriteStr()` |
| Lifecycle management | Auto-managed — triggered on first `rs_rtc_logger_EventTrigger()` call | Application owns all lifecycle transitions |
| Consumer task | Module-owned internal static FreeRTOS task | Application-owned consumer task |
| `rs_rtc_logger_Write()` caller | Called internally by module consumer task | Application consumer task calls explicitly |
| Firmware-update pattern | Not applicable | `rs_rtc_logger_Stop()` before write, `rs_rtc_logger_Start()` after verify |

**Example Project Mode** is intended for new users evaluating the module. The application need only call `rs_rtc_logger_EP_PrepareConsumerTask()` once from task context before enabling the external IRQ, then call `rs_rtc_logger_EventTrigger()` from the ISR. All other behavior — opening HAL drivers, starting the UART output path, draining the circular buffer, and transmitting over VCOM — is managed internally.

**Expert Mode** gives the application full control over the module lifecycle and is appropriate for production firmware where the application needs to sequence initialization precisely, integrate with firmware update workflows (Stop/Start pattern), or manage RTOS task priorities explicitly.

---

## 3. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference or demo application layer, at a glance. The full file-by-file breakdown — including which files are safe to modify — is in [FILE_GUIDE.md](FILE_GUIDE.md).

| Layer | Location | Reuse Expectation |
|---|---|---|
| **Core** | `src/rs_rtc_logger/` (`rs_rtc_logger.c/.h`, `_types.h`, `_cfg.h`, `_format.c/.h`, `_hal.c/.h`, `_stdio_console.c/.h`) | Copy into your project; core logic. `_stdio_console.c/.h` is new — a required adapter to the vendored UART driver below, not optional demo glue (see [FILE_GUIDE.md](FILE_GUIDE.md)) |
| **Vendored** | `src/rs_stdio_uart_buffer_io/` | Buffered I/O UART driver; use verbatim, do not edit — see [FILE_GUIDE.md](FILE_GUIDE.md#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_<board>.c/.h` | RTC instance only. Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) |
| **Reference App** | `src/rs_rtc_logger_demo.c`, `src/blinky_thread_entry_ra4e2.c` / `_ra6e2.c`, `src/hal_warmstart.c`, `src/fault_handler.c`, `src/hal_entry.c` | Adapt or replace entirely for your application |
| **FSP Generated** | `ra/`, `ra_gen/` directories (example projects) / `firmware/generated/` (Meson build) | Do not edit manually; regenerate via e2studio FSP tooling or `rasc_generate` Meson target |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it.

---

## 4. Where to Go Next

- **Running the demo for the first time?** → [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md)
- **Wiring the RTC Event Logger into your application?** → [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)
- **Reusing this for a different event source?** → [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md)
- **Porting to your own board?** → [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)
- **Deciding what to copy vs. replace?** → [FILE_GUIDE.md](FILE_GUIDE.md)
- **Writing code against the module?** → [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md)
- **Understanding internal design?** → [ARCHITECTURE.md](ARCHITECTURE.md)
- **Checking timing/RAM budget?** → [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md)
- **Upgrading from an older release?** → [RELEASE_NOTES.md](RELEASE_NOTES.md)

---

*End of User Manual — `rs_rtc_logger` v1.0.0*
