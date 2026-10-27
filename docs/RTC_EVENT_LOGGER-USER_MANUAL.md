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
module, and it also contains the complete task-specific documentation (running the demo,
configuration, function reference, integration, file layout, architecture,
performance, reusability, and release notes) in a single file. Use the
[Table of Contents](#table-of-contents) to jump to the section you need.

---

## Table of Contents

### Find What You Need by Goal

| If you are... | Go to |
|---|---|
| Evaluating the module for the first time | [1. What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate), [2. What Does the Core Code Do?](#2-what-does-the-core-code-do) and [2.1 Dual Operating Modes](#21-dual-operating-modes) |
| Running the reference demo for the first time | [Running the Project and Expected Output](#running-the-project-and-expected-output) |
| Wiring the RTC Event Logger into your application | [Integration Guide](#integration-guide) |
| Reusing the module for a different event source or use case | [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases) |
| Porting to your own board or peripherals | [Configuration Guide](#configuration-guide) |
| Deciding what to copy, keep, or replace | [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) |
| Writing code against, or debugging inside, `rs_rtc_logger` | [Functions Guide](#functions-guide--public-and-private-functions) |
| Understanding internal design or debugging state issues | [Design Flow and Architecture](#design-flow-and-architecture) |
| Validating the module meets your timing/memory budget | [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance) |
| Adopting a newer release of this module | [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release) |

### Full Table of Contents

- [1. What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate)
  - [Representative Application Verticals](#representative-application-verticals)
- [2. What Does the Core Code Do?](#2-what-does-the-core-code-do)
  - [2.1 Dual Operating Modes](#21-dual-operating-modes)
- [3. Core Code vs. Reference Application Code](#3-core-code-vs-reference-application-code)
- [4. Where to Go Next](#4-where-to-go-next)
- [Running the Project and Expected Output](#running-the-project-and-expected-output)
  - [1. Prerequisites](#1-prerequisites)
  - [2. Importing and Building](#2-importing-and-building)
  - [3. Expected Output — Example Project Mode (cold start)](#3-expected-output--example-project-mode-cold-start)
  - [4. Expected Output — Expert Mode (cold start)](#4-expected-output--expert-mode-cold-start)
  - [5. Expected Output — Warm Reset (both modes)](#5-expected-output--warm-reset-both-modes)
  - [6. Interaction Notes](#6-interaction-notes)
- [Configuration Guide](#configuration-guide)
  - [1. Configuration Overview](#1-configuration-overview)
  - [2. Configuration Assumptions for the Reference Board](#2-configuration-assumptions-for-the-reference-board)
  - [3. Required Non-Default FSP Settings](#3-required-non-default-fsp-settings)
  - [4. `rtc_time_t` Field Conventions](#4-rtc_time_t-field-conventions)
  - [5. Adapting to Your Board](#5-adapting-to-your-board)
    - [Configuration Path Options](#configuration-path-options)
    - [Key Adaptation Points](#key-adaptation-points)
    - [FPB-RA6E2](#fpb-ra6e2)
  - [6. Debounce Configuration (Reference Application)](#6-debounce-configuration-reference-application)
  - [7. Dependencies](#7-dependencies)
    - [Hardware Dependencies](#hardware-dependencies)
    - [Software Dependencies](#software-dependencies)
    - [Driver Dependency Details](#driver-dependency-details)
    - [External Module Dependencies](#external-module-dependencies)
- [Functions Guide — Public and Private Functions](#functions-guide--public-and-private-functions)
  - [Public Functions](#public-functions)
    - [1. Start Here: The Entry Point](#1-start-here-the-entry-point)
    - [2. Configuration Bridge (call before Open)](#2-configuration-bridge-call-before-open)
      - [🟢 `rs_rtc_logger_ConfigGen()`](#-rs_rtc_logger_configgen)
      - [🟢 `rs_rtc_logger_ConfigCheck()`](#-rs_rtc_logger_configcheck)
      - [🟢 `rs_rtc_logger_ConfigGetSize()`](#-rs_rtc_logger_configgetsize)
    - [3. Lifecycle Functions (Expert Mode / also used internally in Example Project Mode)](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode)
    - [4. Operational Functions](#4-operational-functions)
    - [5. Configuration and Query Functions](#5-configuration-and-query-functions)
    - [6. Example Project Mode Setup Helper](#6-example-project-mode-setup-helper)
    - [7. Complete Public Function Reference Table](#7-complete-public-function-reference-table)
  - [Internal Functions](#internal-functions)
    - [8. Internal Functions — `rs_rtc_logger.c`](#8-internal-functions--rs_rtc_loggerc)
      - [⚫ `rs_rl_is_open()`](#-rs_rl_is_open)
      - [⚫ `rs_rl_enqueue()`](#-rs_rl_enqueue)
      - [⚫ `rs_rl_dequeue()`](#-rs_rl_dequeue)
      - [⚫ `rs_rl_auto_init()`](#-rs_rl_auto_init)
      - [⚫ `rs_rtc_logger_internal_consumer_task()`](#-rs_rtc_logger_internal_consumer_task)
    - [9. Internal Functions — `rs_rtc_logger_format.c` / `.h`](#9-internal-functions--rs_rtc_logger_formatc--h)
      - [🔒 `rs_rtc_logger_format_timestamp()` — INTERNAL](#-rs_rtc_logger_format_timestamp--internal)
      - [🔒 `rs_rtc_logger_format_line()` — INTERNAL](#-rs_rtc_logger_format_line--internal)
      - [⚫ `rs_rtc_logger_format_terminator()` — PRIVATE](#-rs_rtc_logger_format_terminator--private)
      - [⚫ `rm_rlf_write_u2()` / `rm_rlf_write_d2()` / `rm_rlf_write_year()` / `rm_rlf_write_uint()` / `rm_rlf_write_str()` — PRIVATE](#-rm_rlf_write_u2--rm_rlf_write_d2--rm_rlf_write_year--rm_rlf_write_uint--rm_rlf_write_str--private)
    - [10. Internal Functions — `rs_rtc_logger_hal.c` / `.h`](#10-internal-functions--rs_rtc_logger_halc--h)
      - [🔒 RTC wrappers — INTERNAL](#-rtc-wrappers--internal)
      - [🔒 UART wrappers — INTERNAL](#-uart-wrappers--internal)
      - [⚫ `rs_rl_bcd_to_dec()` — PRIVATE](#-rs_rl_bcd_to_dec--private)
    - [11. Internal Functions — `rs_rtc_logger_stdio_console.c` / `.h`](#11-internal-functions--rs_rtc_logger_stdio_consolec--h)
    - [12. Complete Internal Function Reference Table](#12-complete-internal-function-reference-table)
- [Integration Guide](#integration-guide)
  - [1. Simple Path Integration (Example Project Mode)](#1-simple-path-integration-example-project-mode)
  - [2. Expert Path Integration (Full Lifecycle Control)](#2-expert-path-integration-full-lifecycle-control)
  - [3. Warm-Reset Integration Ordering](#3-warm-reset-integration-ordering)
  - [4. Task and RTOS Integration](#4-task-and-rtos-integration)
  - [5. Shared UART and Multi-Instance Considerations](#5-shared-uart-and-multi-instance-considerations)
  - [6. Firmware-Update Integration Pattern (Expert Mode)](#6-firmware-update-integration-pattern-expert-mode)
- [File Guide — Naming Conventions and What Is Safe to Modify](#file-guide--naming-conventions-and-what-is-safe-to-modify)
  - [1. Core Code vs. Reference Application Code](#1-core-code-vs-reference-application-code)
  - [2. Naming Conventions Used in This Module](#2-naming-conventions-used-in-this-module)
  - [3. Core Files — Copy and Keep; Modify with Care](#3-core-files--copy-and-keep-modify-with-care)
  - [4. Preset Configuration Files — Reference Only; Do Not Edit](#4-preset-configuration-files--reference-only-do-not-edit)
  - [4a. Vendored Buffered I/O UART Driver — Reference Only; Do Not Edit](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit)
  - [5. Reference Application — Adapt Freely](#5-reference-application--adapt-freely)
  - [6. FSP-Generated Files — Do Not Edit Manually](#6-fsp-generated-files--do-not-edit-manually)
- [Design Flow and Architecture](#design-flow-and-architecture)
  - [1. Lifecycle State Machine](#1-lifecycle-state-machine)
  - [2. Public Function State Gating](#2-public-function-state-gating)
  - [3. Event Capture and Telemetry Flow](#3-event-capture-and-telemetry-flow)
  - [4. Module File Responsibilities](#4-module-file-responsibilities)
- [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance)
  - [1. Functional Behavior Demonstrated](#1-functional-behavior-demonstrated)
  - [2. Performance Characteristics](#2-performance-characteristics)
  - [3. Clock Accuracy Note](#3-clock-accuracy-note)
- [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases)
  - [1. Core Function Reusability](#1-core-function-reusability)
  - [2. Portions Applicable Without Modification](#2-portions-applicable-without-modification)
  - [3. Portions Requiring Modification](#3-portions-requiring-modification)
  - [4. Example: Adapting for an Accelerometer Event Source](#4-example-adapting-for-an-accelerometer-event-source)
- [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release)
  - [1. Current Version](#1-current-version)
  - [2. Buffered I/O over UART](#2-buffered-io-over-uart)
    - [2.1 New components](#21-new-components)
    - [2.2 Per-Board Differences](#22-per-board-differences)
  - [3. Adopting a Newer Release](#3-adopting-a-newer-release)
  - [4. What Typically Changes Between Releases](#4-what-typically-changes-between-releases)
  - [5. Recommended Merge Process](#5-recommended-merge-process)

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

The module encapsulates the RTC HAL driver (`r_rtc` / `rtc_instance_t`) directly, and reaches UART output through a shared, vendored Buffered I/O UART driver rather than opening a `uart_instance_t` itself (see [Release Notes](#2-buffered-io-over-uart)). The External IRQ driver (`r_icu`) is owned and managed by the application layer; any interrupt source calls `rs_rtc_logger_EventTrigger()` to trigger a timestamped log entry. Developers do not need to manually orchestrate RTC reads, timestamp formatting, string composition, and UART transmission in every project.

---

## 2. What Does the Core Code Do?

The core module provides the following capabilities:

**RTC Timestamp Capture**
On every `rs_rtc_logger_EventTrigger()` call, the module immediately reads the current calendar time from the MCU's on-chip RTC via `R_RTC_CalendarTimeGet()`. The timestamp is captured at ISR entry time — before any queuing or formatting occurs — so the logged time reflects the exact moment of the event. The `rtc_time_t` struct fields are converted to display format using integer arithmetic only (no floating-point).

**Event Buffering via Circular Buffer**
Captured events (timestamp + sequence count + payload) are stored in a static circular buffer inside `rs_rtc_logger_ctrl_t`. The buffer decouples ISR-time capture from UART transmit time. `EventTrigger()` returns immediately after enqueuing; it never blocks the calling ISR context. Buffer depth defaults to 8 entries and is configurable at build time.

**UART Transmission via Shared Buffered I/O**
A FreeRTOS consumer task (module-owned in Example Project mode, application-owned in Expert mode) drains the circular buffer and calls `rs_rtc_logger_Write()` to format and transmit each entry over UART. UART writes go through `fwrite()`/`fflush(stdout)` on a shared stdio stream owned by a vendored Buffered I/O UART driver, rather than through a UART instance this module opens and tracks itself. The call returns once the write has been safely submitted (the driver briefly serializes internally if a previous transmission is still in flight). See [Architecture](#3-event-capture-and-telemetry-flow) for the full flow and [Release Notes](#2-buffered-io-over-uart) for what changed.

**Formatted Output**
The default output format is:

```
[YYYY-MM-DD HH:MM:SS] Event Detected #<count>
```

For example: `[2024-07-15 14:30:05] Event Detected #3`

Additional formats are available: time-only `[HH:MM:SS]`, compact `[YYYYMMDD-HHMMSS]`, and syslog-style `[Mon DD HH:MM:SS]`. Format is selectable at build time or changeable at runtime via `rs_rtc_logger_TimestampFormatSet()`.

**Warm Reset Preservation**
On a warm reset (any reset other than power-on reset), the RTC continues running and retains its calendar configuration. The module relies on the FSP `r_rtc` build-time property *Set Source Clock in Open* being set to `Disabled`, so `R_RTC_ClockSourceSet()` is called only when a power-on reset is detected. The event sequence counter is preserved by the reference application using a `BSP_SECTION_NOINIT` magic-sentinel pattern and restored via `rs_rtc_logger_SeqCountSet()` after `Open()`. Power-on-reset detection reads the sticky `R_SYSTEM->RSTSR0_b.PORF` flag and then explicitly clears it, so the flag accurately reflects only the reset that just occurred and every subsequent reset — including genuine warm resets — is correctly identified rather than being re-treated as another power-on reset. See [Release Notes](#2-buffered-io-over-uart).

**Debounced Event Source (Reference Application)**
The reference application's user-button ISR callback (`demo_button_s1_callback()`) applies a software debounce, in addition to the r_icu hardware digital filter, before calling `rs_rtc_logger_EventTrigger()`. This is demo-layer behavior, not core module behavior — the core module accepts every `EventTrigger()` call it receives regardless of source. See [Configuration Guide](#6-debounce-configuration-reference-application) for tuning details.

**LOCO Clock Source**
The RTC is clocked from the LOCO (Low-speed On-Chip Oscillator), which is the FSP default for the RA4E2. LOCO is less accurate than a 32.768 kHz sub-clock crystal (which is fitted on the FPB-RA4E2 as Y2 but is not used by default). For applications requiring higher time accuracy, the sub-clock alternative is documented in [Reusability Guide](#reusability-and-adaptation-for-similar-use-cases). A runtime LOCO drift calibration function `rs_rtc_logger_LocoCalibrationSet()` is provided to adjust the RFRL register — approximately 3906 ppm per RFRL unit, which is roughly 3.5 seconds per 15 minutes per unit.

**Power-Down Retention Limitation (FPB-RA4E2)**
The RA4E2 in its 64-pin QFN package (R7FA4E2B93CFM) does not expose a VBATT pin, and the FPB-RA4E2 board has no battery connector or VBATT supply circuit. RTC time is therefore **not preserved on power-down** on this board. RTC state is preserved across warm resets while USB power is maintained. Battery-backed power-down retention requires a target board with VBATT supply; the FPB-RA6E2 VBATT support status should be confirmed against its schematic.

---

### 2.1 Dual Operating Modes

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

The following table identifies which files are reusable core code and which belong only to the reference or demo application layer, at a glance. The full file-by-file breakdown — including which files are safe to modify — is in [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify).

| Layer | Location | Reuse Expectation |
|---|---|---|
| **Core** | `src/rs_rtc_logger/` (`rs_rtc_logger.c/.h`, `_types.h`, `_cfg.h`, `_format.c/.h`, `_hal.c/.h`, `_stdio_console.c/.h`) | Copy into your project; core logic. `_stdio_console.c/.h` is new — a required adapter to the vendored UART driver below, not optional demo glue (see [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify)) |
| **Vendored** | `src/rs_stdio_uart_buffer_io/` | Buffered I/O UART driver; use verbatim, do not edit — see [File Guide](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_<board>.c/.h` | RTC instance only. Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see [Configuration Guide](#configuration-guide)) |
| **Reference App** | `src/rs_rtc_logger_demo.c`, `src/blinky_thread_entry_ra4e2.c` / `_ra6e2.c`, `src/hal_warmstart.c`, `src/fault_handler.c`, `src/hal_entry.c` | Adapt or replace entirely for your application |
| **FSP Generated** | `ra/`, `ra_gen/` directories (example projects) / `firmware/generated/` (Meson build) | Do not edit manually; regenerate via e2studio FSP tooling or `rasc_generate` Meson target |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it.

---

## 4. Where to Go Next

- **Running the demo for the first time?** → [Running the Project and Expected Output](#running-the-project-and-expected-output)
- **Wiring the RTC Event Logger into your application?** → [Integration Guide](#integration-guide)
- **Reusing this for a different event source?** → [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases)
- **Porting to your own board?** → [Configuration Guide](#configuration-guide)
- **Deciding what to copy vs. replace?** → [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify)
- **Writing code against the module?** → [Functions Guide](#functions-guide--public-and-private-functions)
- **Understanding internal design?** → [Design Flow and Architecture](#design-flow-and-architecture)
- **Checking timing/RAM budget?** → [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance)
- **Upgrading from an older release?** → [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release)

---

## Running the Project and Expected Output

*Read this when you are: Running the reference demo for the first time.*

### 1. Prerequisites

- FPB-RA4E2 board connected via USB (provides both power and J-Link virtual COM)
- e2studio IDE with FSP v6.5.0 installed
- A serial terminal connected to the J-Link virtual COM port at **115200 baud, 8N1**

### 2. Importing and Building

1. Import the project into e2studio using **File → Import → Existing Projects into Workspace** and select the `RTC_Event_Logger_FPB_RA4E2` project directory (or `RTC_Event_Logger_FPB_RA6E2` for the secondary target).
2. The project is fully functional upon import. The `configuration.xml` is pre-configured; no FSP Stack Configurator re-run is required for the default preset path.
3. Build the project (**Project → Build All**). The default build target is **Example Project mode** (`RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1`).
4. Flash and debug using **Run → Debug As → Renesas GDB Hardware Debugging**.
5. Open a serial terminal on the J-Link virtual COM port at 115200 8N1.
6. Power cycle the target board by disconnecting and re-connecting the USB cable.

To switch to **Expert mode**, set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` to `0` in `rs_rtc_logger_cfg.h` (or as a project-level compiler define `-DRS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0`) and rebuild.

> **Note:** If you download firmware via the e2studio debugger, be aware that the default e2studio debugger reset behavior freezes the RTC counter, which affects timestamps shown below.

### 3. Expected Output — Example Project Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Example Project
RTC Event Logger control block: 992 bytes.
Creating RTC Event Logger consumer task...
Consumer task ready.
Triggering RTC Event Logger auto-init...
Waiting for RTC Event Logger auto-init to complete...
[0000-00-00 00:00:00] INIT #1
RTC Event Logger auto-init complete.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2026-07-30 10:38:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: 3
LOCO calibration set: trim +3, RFRL = 258.
Ready. Press S1 to log an event.
[2026-07-30 10:38:06] Event Detected #2
[2026-07-30 10:38:07] Event Detected #3
```

### 4. Expected Output — Expert Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Expert
Opening RTC Event Logger...
RTC Event Logger control block: 984 bytes.
RTC Event Logger open OK.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2026-07-30 10:54:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: 3
LOCO calibration set: trim +3, RFRL = 258.
Starting RTC Event Logger...
RTC Event Logger started.
Ready. Press S1 to log an event.
[2026-07-30 10:54:05] Event Detected #1
[2026-07-30 10:54:07] Event Detected #2
```

### 5. Expected Output — Warm Reset (both modes)

After pressing the reset button while USB power remains:

```
=========================================
  RTC Event Logger - FPB-RA4E2
  FSP v6.5.0 / FreeRTOS
=========================================
UART OK. Starting ...

*** RTC Event Logger Demo ***
Mode: Example Project
RTC Event Logger control block: 992 bytes.
Creating RTC Event Logger consumer task...
Consumer task ready.
Triggering RTC Event Logger auto-init...
Waiting for RTC Event Logger auto-init to complete...
[0000-00-00 00:00:00] INIT #1
RTC Event Logger auto-init complete.
Warm reset: seq counter restored.
Warm reset — RTC time retained.
Ready. Press S1 to log an event.
[2026-07-30 10:49:37] Event Detected #3
```

> Note: "Warm reset: seq counter restored." appears only when a valid sequence-counter
> backup exists from the previous session (i.e. at least one event was logged before the reset).

The sequence counter resumes from its pre-reset value and the RTC continues from its running time with no re-prompting for date/time.

### 6. Interaction Notes

- **Date/time prompt:** Enter in `YYYY-MM-DD HH:MM:SS` format. Press Enter with no input to skip; events will display `[0000-00-00 00:00:00]` and you will be re-prompted after each event.
- **LOCO trim prompt:** Enter a signed integer (e.g. `+17`, `-8`, `0`) to adjust the RFRL register. Press Enter with no input to skip and use the default RFRL = 255.
- **S1 button:** Press User Switch S1 on the FPB-RA4E2 board to trigger an event. A software debounce (default 150 ms, `RS_RTC_LOGGER_CFG_DEBOUNCE_MS`) suppresses spurious re-triggers from mechanical switch bounce — see [Configuration Guide](#6-debounce-configuration-reference-application). Presses spaced closer than the debounce window will not produce a second logged event.

---

## Configuration Guide

*Read this when you are: Porting to your own board or peripherals.*

### 1. Configuration Overview

The `rs_rtc_logger` relies on a combination of:

- FSP-generated or preset RTC instance configuration
- Build-time configuration constants in `rs_rtc_logger_cfg.h`
- A runtime configuration structure (`rs_rtc_logger_cfg_t`) bound via `rs_rtc_logger_ConfigGen()`

The reference implementation targets the **FPB-RA4E2** and **FPB-RA6E2**.

---

### 2. Configuration Assumptions for the Reference Board

The reference implementation targets the **FPB-RA4E2** with the following board-specific assumptions. These are reference assumptions for the demo hardware, not requirements for your application.

| Resource | Reference Assumption | Notes |
|---|---|---|
| MCU | RA4E2 (Cortex-M33, 64-pin QFN, R7FA4E2B93CFM) | Primary development target |
| UART channel | SCI0 (USB-Serial VCOM bridge via J-Link), owned by the vendored Buffered I/O UART driver | Schematic page 7: P410(RX)/P411(TX); see [Section 3](#3-required-non-default-fsp-settings) |
| UART baud rate | 115200 baud, async, 8N1 | VCOM USB-Serial bridge default |
| RTC clock source | LOCO (Low-speed On-Chip Oscillator) | FSP default; see clock accuracy note in [Behavior and Performance](#demonstrated-behavior-and-performance) |
| Sub-clock crystal | Y2 (32.768 kHz) populated but not used by default | Available as a higher-accuracy alternative |
| External IRQ | P304 / IRQ9 / User Switch S1 | Schematic page 2; application-owned |
| Power supply | USB 5V → 3.3V linear regulator (no VBATT) | No battery backup; see power-down limitation in [Power-Down Retention Limitation](#2-what-does-the-core-code-do) |
| FreeRTOS heap | heap_4 | Required for `xTaskCreateStatic()` |
| FreeRTOS static allocation | `configSUPPORT_STATIC_ALLOCATION = 1` | Required for static task creation |

---

### 3. Required Non-Default FSP Settings

The following FSP Stack Configurator settings must be configured correctly for the module to operate. These are values that differ from FSP defaults and must be set in `configuration.xml` when not using the preset configuration path (see [Section 5](#5-adapting-to-your-board)).

| Driver | Property | Required Value | Reason |
|---|---|---|---|
| `r_rtc` | Clock source | LOCO | Specifies the LOCO oscillator as the RTC clock source |
| `r_rtc` | Calendar mode | Enabled | Required for `R_RTC_CalendarTimeGet()` and `R_RTC_CalendarTimeSet()` |
| `r_rtc` | Set Source Clock in Open | **Disabled** | Required for warm-reset RTC preservation — prevents `R_RTC_ClockSourceSet()` from being called on every open, which would reset the running clock |
| `r_rtc` | RTC interrupt priority | Non-zero (≥ 2) | Required; must be lower than the FreeRTOS max syscall priority |
| `r_rtc` | Carry interrupt priority (`carry_ipl`) | ≤ 4 (e.g. 2) | `R_RTC_CalendarTimeGet()` is called from `EventTrigger()` in ISR context and spins waiting for the carry ISR. The carry ISR MUST have higher preemption (lower IPL number) than every ISR that calls `EventTrigger()`. The demo button ISR is at IPL 5; carry must be < 5. The FSP-generated `hal_data.c` default of 12 will cause an **infinite loop/deadlock** when `EventTrigger()` is called from an ISR. The preset config uses `carry_ipl = 2` (safe). `rs_rtc_logger_Open()` checks this at runtime and returns `FSP_ERR_INVALID_HW_CONDITION` if the limit is exceeded. |
| `r_icu` | IRQ channel | IRQ9 (P304) | On-board user button S1; configured and owned by the application |
| FreeRTOS | Heap scheme | heap_4 (or heap_3) | Required for `xTaskCreateStatic()` |
| FreeRTOS | `configSUPPORT_STATIC_ALLOCATION` | 1 | Required for static task creation (no heap used for task stack or control block) |

> **The Buffered I/O UART is not configured by this component.** The RTC Event Logger does not open, own, or configure the UART instance. The shared VCOM UART is opened and owned by the vendored **Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`), bound to the FSP Stack Configurator-generated `g_uart_jlob_vcom` instance. The table below lists the UART-related settings that driver requires — set them on `g_uart_jlob_vcom` in `configuration.xml`. The vendored driver installs its own (`uart_jlob_vcom_callback`) the first time any stdio call (`printf`/`getchar`/`fflush`) runs.

| Driver | Property | Required Value | Reason |
|---|---|---|---|
| `r_sci_uart` (`g_uart_jlob_vcom`) | Callback | Left as generated / driver-installed (`uart_jlob_vcom_callback`) | Owned by the Buffered I/O UART driver, not by this component. |
| `r_sci_uart` (`g_uart_jlob_vcom`) | Operating mode | Asynchronous | Required by the Buffered I/O UART driver |
| `r_sci_uart` (`g_uart_jlob_vcom`) | Baud rate | 115200 | Matches the VCOM USB-Serial bridge default on FPB boards |
| `r_sci_uart` (`g_uart_jlob_vcom`) | Flow control | RTS (SCI-internal; no external flow-control pin) | Matches the reference configuration validated by `RS_STDIO_UART_CheckApplicationConfiguration()` |
| `r_sci_uart` (`g_uart_jlob_vcom`) | DMA/DTC transfer instances (`p_transfer_tx`/`p_transfer_rx`) | Not set (NULL) | The vendored driver's config check explicitly rejects a DMA/DTC-backed transfer instance |
| `r_sci_uart` (`g_uart_jlob_vcom`) | rxi/txi/tei/eri interrupt priority and vector | Enabled, valid NVIC vector allocated | Same failure mode as the old `r_sci_uart` requirement: if the stack is removed, IRQs fall back to `FSP_INVALID_VECTOR`/`BSP_IRQ_DISABLED` and the Buffered I/O UART driver's own validation (`RS_STDIO_UART_CheckApplicationConfiguration()`, invoked via `rs_rtc_logger_console_init()` during `rs_rtc_logger_Open()`) will reject it |
| Clock tree | HOCO | 20 MHz (both boards) | Required by the reference baud-rate register values baked into the board config file |
| Clock tree | PCLKA | **100 MHz on FPB-RA4E2**, **50 MHz on FPB-RA6E2** | Different PLL configuration per board (see [Release Notes](#22-per-board-differences)); the driver's config check validates against the value for the board it was built for |

---

### 4. `rtc_time_t` Field Conventions

The FSP `rtc_time_t` type is an alias for the C Standard `struct tm`. Two fields require an offset conversion before display:

| Field | Meaning | Display conversion for `[YYYY-MM-DD HH:MM:SS]` |
|---|---|---|
| `tm_year` | Years since 1900 | Add 1900: `tm_year + 1900` |
| `tm_mon` | Months since January (0–11) | Add 1: `tm_mon + 1` |
| `tm_mday` | Day of month (1–31) | Direct use |
| `tm_hour` | Hours (0–23) | Direct use — 24-hour mode only |
| `tm_min` | Minutes (0–59) | Direct use |
| `tm_sec` | Seconds (0–60) | Direct use |

The FSP RTC operates exclusively in 24-hour mode. The timestamp formatter inside the module handles all of these conversions automatically.

---

### 5. Adapting to Your Board

#### Configuration Path Options

The module supports two ways to supply RTC hardware configuration, selected by `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` in `rs_rtc_logger_cfg.h`. This selection covers the **RTC** instance only — see the Buffered I/O UART note below for how the shared UART is configured.

**Preset Path (default, `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 1`):**
Pre-built RTC HAL instance definitions for the FPB-RA4E2 (or FPB-RA6E2) are provided in `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.c/.h` (or `_ra6e2.c/.h`). The project builds and runs without opening the FSP Stack Configurator. This path is recommended for initial evaluation.

```c
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
#include "rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h"
#endif

rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
```

**FSP Stack Configurator Path (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`):**
Configure the `r_rtc` stack in the e2studio FSP Stack Configurator for your target board. FSP tooling generates a `g_rtc0` symbol in `ra_gen/`. Pass it to `rs_rtc_logger_ConfigGen()` instead of the preset instance.

```c
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0);
```

Use this path when targeting a board other than FPB-RA4E2/FPB-RA6E2, or when you need to customize the RTC clock source or any other FSP-configurable RTC property. Validate your binding with `rs_rtc_logger_ConfigCheck()` — see [Functions Guide](#2-configuration-bridge-call-before-open).

> `rs_rtc_logger_ConfigGen()` takes an RTC instance. The shared UART is configured entirely separately, through the vendored Buffered I/O UART driver's own per-board config file (`src/rs_stdio_uart_buffer_io/stdio_uart_config_ra4e2_fpb.c` or `_ra6e2_fpb.c`), which binds to the FSP Stack Configurator-generated `g_uart_jlob_vcom` instance and is validated independently via `RS_STDIO_UART_CheckApplicationConfiguration()` (wired in through `rs_rtc_logger_console_init()`, called from `rs_rtc_logger_hal_uart_open()` during `Open()`). There is no equivalent "preset vs. FSP Stack Configurator" choice for the UART side — the driver always binds to `g_uart_jlob_vcom`. See [Release Notes](#2-buffered-io-over-uart) for the full change and [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) for which files are safe to modify when porting the UART side to a new board.

#### Key Adaptation Points

| Adaptation | Location | What to Change |
|---|---|---|
| Target board / RTC instance | `src/rs_rtc_logger/config/` | Create a new preset config file for your board, or switch to the FSP Stack Configurator path |
| Target board / shared UART | `src/rs_stdio_uart_buffer_io/` | Add a new `stdio_uart_config_<board>.c` file following the pattern in `stdio_uart_config_ra4e2_fpb.c` / `_ra6e2_fpb.c` — reference pins, baud registers, and `RS_STDIO_UART_CheckApplicationConfiguration()` reference values for your board |
| External interrupt source | Application ISR callback | `r_icu` is application-owned; replace with any interrupt source (accelerometer callback, timer, GPIO) |
| RTC clock source | `configuration.xml` r_rtc properties | Change from LOCO if the sub-clock or another source is preferred |
| Event payload string | Call site of `rs_rtc_logger_EventTrigger()` | Replace `"Event Detected"` with your application-specific event description |
| Timestamp format | `rs_rtc_logger_cfg_t.ts_format` or `rs_rtc_logger_TimestampFormatSet()` | Select ISO8601 (default), time-only, compact, or syslog format |
| Buffer depth | `RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH` in `rs_rtc_logger_cfg.h` | Increase for higher burst event rates; decrease to save RAM |
| Consumer task priority | `RS_RTC_LOGGER_CFG_TASK_PRIORITY` | Tune relative to other application tasks |
| Consumer task stack size | `RS_RTC_LOGGER_CFG_TASK_STACK_SIZE` | Default 2048 bytes; increase if deeper call stacks are needed |
| Operating mode | `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` | Set to `0` (Expert mode) to own the full lifecycle |
| Overflow policy | `rs_rtc_logger_cfg_t.overflow_policy` | Switch to `DROP_NEWEST` if strict event ordering must be preserved |
| Sequence counter on warm reset | Application: call `rs_rtc_logger_SeqCountSet()` after `Open()` | Restore from application-managed non-volatile storage |
| LOCO drift compensation | `rs_rtc_logger_LocoCalibrationSet()` at runtime, or `RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT` at build time | Adjust RFRL register to compensate for observed oscillator error |
| RTC carry interrupt priority (`carry_ipl`) | `configuration.xml` r_rtc properties → Carry Interrupt Priority | Must be strictly lower number (higher preemption) than the IPL of every ISR that calls `rs_rtc_logger_EventTrigger()`. The FSP Stack Configurator default is `carry_ipl = 12`, which is lower preemption than the demo button ISR at `ipl = 5`. Set `carry_ipl` to a value ≤ `RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT` (default 4). `rs_rtc_logger_Open()` returns `FSP_ERR_INVALID_HW_CONDITION` if this is violated. |
| Button debounce window (reference app only) | `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` in `rs_rtc_logger_cfg.h` | Default 150 ms; see [Section 6](#6-debounce-configuration-reference-application) |
| RX/TX byte queue size (Buffered I/O UART driver) | `src/rs_stdio_uart_buffer_io/syscalls_picolib_stdio_to_uart.c` | Fixed at 256 bytes each, hard-coded in the vendored driver — not exposed via `rs_rtc_logger_cfg.h`. Edit only if you accept diverging from the vendored driver's "used unmodified" delivery model. |

#### FPB-RA6E2

The FPB-RA6E2 port includes the full Buffered I/O over UART update and is symmetric with FPB-RA4E2 — same core module, same `rs_rtc_logger_stdio_console.c/h` adapter, same vendored driver files, differing only in the per-board files below. Board-specific differences to address when adapting further:

| Item | FPB-RA4E2 | FPB-RA6E2 |
|---|---|---|
| RTC preset config | `rs_rtc_logger_data_fpb_ra4e2.c/.h` | `rs_rtc_logger_data_fpb_ra6e2.c/.h` |
| Buffered I/O UART board config | `stdio_uart_config_ra4e2_fpb.c` (guard: `BOARD_RA4E2_FPB`) | `stdio_uart_config_ra6e2_fpb.c` (guard: `BOARD_RA6E2_FPB`) |
| UART baud register (`brr`) | 53 | 26 |
| PCLKA | 100 MHz | 50 MHz |
| HOCO | 20 MHz | 20 MHz |
| UART pins | P410 (RX) / P411 (TX) | P410 (RX) / P411 (TX) |
| RTC `carry_ipl` | 2 | 2 |

Additional items to confirm independently of this update:
- The RA6E2 includes TrustZone. The module targets the non-secure partition by default. The reference application must use the BSP TrustZone template configured as a non-secure project. This has not been specifically re-assessed against the Buffered I/O UART driver as part of this update.
- Confirm VBATT schematic support on FPB-RA6E2 if power-down RTC retention is required.

---

### 6. Debounce Configuration (Reference Application)

This is **reference application behavior**, not core module behavior. The core module (`rs_rtc_logger_EventTrigger()`) has no concept of debouncing and accepts every call it receives from any source.

The FPB boards' `r_icu` hardware digital filter is set to `PCLKB_div_1` (120 ns at 25 MHz), which is far shorter than the 5–20 ms bounce period of a mechanical button. Every bounce edge triggers the ISR, producing 7–13 spurious `EventTrigger()` calls per physical button press if left unfiltered.

The reference application implements a software debounce inside `demo_button_s1_callback()` (in `rs_rtc_logger_demo.c`), using the FreeRTOS tick count (`xTaskGetTickCountFromISR()`) to reject ISR firings that arrive sooner than `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` after the last accepted event. This covers typical button bounce while allowing rapid intentional presses spaced apart.

| Constant | Default | Location | Effect |
|---|---|---|---|
| `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` | `150U` | `rs_rtc_logger_cfg.h` | Minimum milliseconds between accepted button-triggered events. Set to `0` to disable the software debounce and rely on the hardware filter only. |

If you replace the button ISR with a different event source (accelerometer, timer, GPIO), decide independently whether that source needs its own debounce/deglitch strategy — the `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` constant and its consuming code are specific to the demo's button callback and are not part of the reusable core.

---

### 7. Dependencies

#### Hardware Dependencies

| Resource | Usage | Owned By |
|---|---|---|
| On-chip RTC (`r_rtc`) | Calendar time read/set; clock source initialization | This module |
| UART — SCI0 (`r_sci_uart`, `g_uart_jlob_vcom`) | Shared VCOM serial I/O at 115200 baud (both log output and CLI console) | **Vendored Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`) — not this module |
| External IRQ (`r_icu`) — P304/IRQ9 | User button S1 interrupt detection | **Application** |
| USB-Serial bridge (J-Link virtual COM) | VCOM physical layer | Board hardware |

#### Software Dependencies

| Component | Version | Notes |
|---|---|---|
| Renesas FSP | v6.5.0 | Provides `r_rtc`, `r_sci_uart`, `r_icu`, BSP, startup |
| FreeRTOS | Bundled with FSP v6.5.0 | Required for time-critical event synchronization |
| **Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`) | Vendored used unmodified | hard dependency on the the Buffered I/O driver since  — `rs_rtc_logger_hal_uart_open()` calls into it (via `rs_rtc_logger_console_init()`) on every `Open()`. See [Section 3](#3-required-non-default-fsp-settings) and [Release Notes](#2-buffered-io-over-uart). |
| picolibc | Bundled with the LLVM ARM Embedded toolchain | Required by the vendored driver's stdio stream redirection (`FDEV_SETUP_STREAM`) |
| e2studio IDE | Compatible with FSP v6.5.0 | Required to build and flash the project |
| J-Link On-Board | — | Used for debug/flash on FPB boards |

#### Driver Dependency Details

The module references the RTC HAL driver exclusively through the FSP abstract interface type `rtc_instance_t *`. This means the RTC-facing core source is identical across both target boards — board-specific RTC differences are confined to `configuration.xml` and the RTC preset config files.

The module functions `rs_rtc_logger_hal_uart_open()`/`_close()`/`_write()` goes through `rs_rtc_logger_stdio_console.c` to the vendored Buffered I/O UART driver's stdio stream. That driver owns `g_uart_jlob_vcom` (opening it on first stdio call, or deterministically via `rs_rtc_logger_console_init()`) and never closes it for the life of the application.

The External IRQ driver (`r_icu`) is **not** initialized, enabled, disabled, or closed by this module. The module has no dependency on any specific interrupt source. When the `r_icu` ISR fires, the application calls `rs_rtc_logger_EventTrigger()` to pass the event to the module core.

#### External Module Dependencies

**Buffered I/O over UART.** This module depends on one other component: the vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`), used verbatim/unmodified, for all UART I/O. If you replace or remove the vendored driver, `rs_rtc_logger_hal_uart_open()`/`_close()`/`_write()` (via `rs_rtc_logger_stdio_console.c`) will need to be re-pointed at whatever UART I/O mechanism you substitute.

---

## Functions Guide — Public and Private Functions

*Read this when you are: Writing code against, or debugging inside, `rs_rtc_logger`.*

> For call sequences and how these functions fit together in an application, see [Integration Guide](#integration-guide).

This guide documents every function in the `rs_rtc_logger` — both the **public** functions an application calls directly, and the **private/internal** functions used only inside the module. Each function is explicitly tagged:

| Tag | Meaning |
|---|---|
| 🟢 **PUBLIC** | Declared in `rs_rtc_logger.h` or a board config header (`rs_rtc_logger_data_fpb_<board>.h`). Part of the supported application-facing interface. Naming: `rs_rtc_logger_OperationName()`. |
| 🔒 **INTERNAL** | Declared in an internal header (`rs_rtc_logger_format.h`, `rs_rtc_logger_hal.h`) marked "not part of the public API." Has external (non-`static`) linkage so it can be called from other files inside the module, but is not intended for application use. |
| ⚫ **PRIVATE** | `static` — file-scoped, callable only from within the `.c` file that defines it. Never referenced outside the module. |

Only 🟢 **PUBLIC** functions are safe to call from application code. 🔒 **INTERNAL** and ⚫ **PRIVATE** functions are documented here for engineers reading, debugging, or extending the module's source — calling them directly from application code voids the safety guarantees (state gating, ISR-safety, hot-safe semantics) described for the public functions.

---

### Public Functions

> **What "hot-safe" means:** A function marked hot-safe can be called while the module is in the STARTED state (actively transmitting) without disrupting the current transmission. The change takes effect on the next `Write()` call. Functions that are not hot-safe must only be called before the module is started or after it is stopped.

#### 1. Start Here: The Entry Point

```c
// ISR-safe — the only function the application ISR needs to call:
rs_rtc_logger_EventTrigger(&g_logger_ctrl, (uint8_t const *)"Event Detected", 14U);
```

In **Example Project mode**, this is the only function the application needs to call after initial setup. In **Expert mode**, the application also manages the lifecycle functions described below.

---

#### 2. Configuration Bridge (call before Open)

```c
// Bind the RTC HAL instance — call once before rs_rtc_logger_Open()
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
```

`rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, and `rs_rtc_logger_ConfigGetSize()` are declared in the **board-specific preset configuration header** (`rs_rtc_logger_data_fpb_ra4e2.h` / `rs_rtc_logger_data_fpb_ra6e2.h`), alongside the `*_preset` instance data they are most commonly paired with. They are declared and defined **unconditionally** (regardless of `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE`), so the same three functions remain available whether the caller binds the `*_preset` RTC instance or the FSP Stack Configurator-generated `g_rtc0`.

>  Boards are using a Buffered I/O UART driver (FPB-RA4E2 and FPB-RA6E2 — see [Configuration Guide](#configuration-guide)), this component never opens, writes, or validates a `uart_instance_t` at all. `rs_rtc_logger_ConfigGen()` takes **only** an RTC instance parameter. The shared UART is owned and independently validated by the vendored Buffered I/O UART driver.

##### 🟢 `rs_rtc_logger_ConfigGen()`

Assigns the RTC HAL instance pointer to the RTC Event Logger configuration structure. Called once during application initialization, before `rs_rtc_logger_Open()`.

```c
fsp_err_t rs_rtc_logger_ConfigGen(rs_rtc_logger_cfg_t   * p_cfg,
                                   rtc_instance_t  const * p_rtc);
```

`p_rtc` accepts either the pre-set `g_rtc0_preset` (when `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1`) or the FSP Stack Configurator-generated `g_rtc0` (when `== 0`). Non-instance fields (`ts_format`, `ts_brackets`, `line_term`, `overflow_policy`, `p_callback`, `p_context`) must already be set in `p_cfg` by the caller; this function does not modify them.

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Instance pointer assigned |
| `FSP_ERR_ASSERTION` | `p_cfg` or `p_rtc` is NULL |

*Valid in any RTC Event Logger lifecycle state. Not ISR-safe.*

##### 🟢 `rs_rtc_logger_ConfigCheck()`

Validates a configuration structure before `rs_rtc_logger_Open()`. Verifies that the required RTC HAL instance (and its configuration block) is present, that the RTC carry interrupt priority satisfies the constraint documented in [Configuration Guide](#3-required-non-default-fsp-settings), and that the enumerated configuration fields (`ts_format`, `line_term`, `overflow_policy`) are in range.

```c
fsp_err_t rs_rtc_logger_ConfigCheck(rs_rtc_logger_cfg_t const * p_cfg);
```

Call any time after `rs_rtc_logger_ConfigGen()` and before `rs_rtc_logger_Open()`, so misconfiguration is caught early. This function validates the **RTC instance only** — UART configuration is validated independently by the Buffered I/O UART driver's own `RS_STDIO_UART_CheckApplicationConfiguration()` (wired in through `rs_rtc_logger_console_init()`, called from `rs_rtc_logger_hal_uart_open()` during `Open()` — see [Section 10](#10-internal-functions--rs_rtc_logger_halc--h)).

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Configuration is valid |
| `FSP_ERR_ASSERTION` | `p_cfg` is NULL |
| `FSP_ERR_NOT_INITIALIZED` | Required RTC instance/config is missing |
| `FSP_ERR_INVALID_HW_CONDITION` | RTC `carry_ipl`/`carry_irq` is disabled or has no NVIC vector allocated |
| `FSP_ERR_INVALID_ARGUMENT` | `ts_format`, `line_term`, or `overflow_policy` is out of range |

*Valid in any lifecycle state. Not ISR-safe.*

##### 🟢 `rs_rtc_logger_ConfigGetSize()`

Returns the size, in bytes, of the RTC Event Logger control block structure. Useful for confirming control-block sizing (e.g. against `RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES`) without requiring `rs_rtc_logger_Open()` to have been called first.

```c
fsp_err_t rs_rtc_logger_ConfigGetSize(rs_rtc_logger_ctrl_t const * p_ctrl,
                                       size_t                      * p_size);
```

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Size returned in `*p_size` |
| `FSP_ERR_ASSERTION` | `p_ctrl` or `p_size` is NULL |

*Valid in any lifecycle state. Not ISR-safe. `p_ctrl` may point to caller-allocated storage that has not yet been opened — it is only used for `sizeof(*p_ctrl)`.*

Example combining all three:

```c
#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>
...
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
fsp_err_t err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
if (FSP_SUCCESS != err) { /* handle bad configuration */ }
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
```

---

#### 3. Lifecycle Functions (Expert Mode / also used internally in Example Project Mode)

| Function | Valid States | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_Open(p_ctrl, p_cfg)` | CLOSED | Initialize; open RTC + UART HAL drivers in order |
| 🟢 `rs_rtc_logger_Start(p_ctrl)` | OPEN, STOPPED | Enable UART output path; activate consumer task |
| 🟢 `rs_rtc_logger_Stop(p_ctrl)` | STARTED | Suspend TX; buffer continues accepting events |
| 🟢 `rs_rtc_logger_Close(p_ctrl)` | OPEN, STOPPED | Release all resources; close UART then RTC |

---

#### 4. Operational Functions

| Function | Valid States | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_EventTrigger(p_ctrl, p_data, length)` | STARTED, STOPPED (Example Project: also CLOSED on first call) | ISR-safe capture; enqueues to circular buffer |
| 🟢 `rs_rtc_logger_EventDequeue(p_ctrl, p_entry_out)` | OPEN, STARTED, STOPPED | Dequeue oldest buffer entry into caller's struct (Expert mode consumer task; safe canonical path) |
| 🟢 `rs_rtc_logger_Write(p_ctrl, p_entry)` | STARTED | Format and transmit one buffer entry (called internally in Example Project mode) |
| 🟢 `rs_rtc_logger_WriteStr(p_ctrl, p_str)` | STARTED | Convenience: write a string with live RTC timestamp, bypassing the circular buffer |

---

#### 5. Configuration and Query Functions

| Function | Hot-Safe | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_TimestampFormatSet(p_ctrl, format)` | Yes | Switch timestamp format at runtime |
| 🟢 `rs_rtc_logger_TimeSet(p_ctrl, p_time)` | Yes | Set RTC calendar date and time |
| 🟢 `rs_rtc_logger_ClockValidate(p_ctrl, time_is_valid)` | Yes | Mark RTC time as valid or invalid (used on warm reset) |
| 🟢 `rs_rtc_logger_LocoCalibrationSet(p_ctrl, rfrl_value)` | Yes | Adjust LOCO oscillator drift via RFRL register (valid range 8–510) |
| 🟢 `rs_rtc_logger_SeqCountSet(p_ctrl, seq_count)` | No | Restore sequence counter after warm reset (OPEN or STOPPED only) |
| 🟢 `rs_rtc_logger_StatusGet(p_ctrl, p_status)` | — | Snapshot of state, counters, and last timestamp |
| 🟢 `rs_rtc_logger_TimeGet(p_ctrl, p_time)` | — | Read current RTC calendar time |
| 🟢 `rs_rtc_logger_VersionGet(p_version)` | — | Module version (valid in any state including CLOSED) |

---

#### 6. Example Project Mode Setup Helper

```c
// Call from task context BEFORE enabling the external IRQ:
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);
```

🟢 This creates the internal static FreeRTOS consumer task. It must be called from task context (not ISR context) because `xTaskCreateStatic()` must not be called from within an ISR. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

---

#### 7. Complete Public Function Reference Table

| Function | Category | State Gating | See |
|---|---|---|---|
| `rs_rtc_logger_ConfigGen()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_ConfigCheck()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_ConfigGetSize()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_Open()` | Lifecycle | CLOSED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Start()` | Lifecycle | OPEN, STOPPED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Stop()` | Lifecycle | STARTED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Close()` | Lifecycle | OPEN, STOPPED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_EventTrigger()` | Operational | STARTED, STOPPED (+CLOSED in EP mode) | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_EventDequeue()` | Operational | OPEN, STARTED, STOPPED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_Write()` | Operational | STARTED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_WriteStr()` | Operational | STARTED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_TimestampFormatSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_TimeSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_ClockValidate()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_LocoCalibrationSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_SeqCountSet()` | Config/Query | OPEN, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_StatusGet()` | Config/Query | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_TimeGet()` | Config/Query | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_VersionGet()` | Config/Query | Any state | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_EP_PrepareConsumerTask()` | Setup helper | Any state, task context only | [Section 6](#6-example-project-mode-setup-helper) |

For the full state-gating matrix (which states return which error codes), see [Architecture](#2-public-function-state-gating).

---

### Internal Functions

The functions below are **not part of the supported application interface**. They are documented so that engineers debugging, reviewing, or extending `rs_rtc_logger` can understand the internal call graph without re-deriving it from source. Signatures may change between releases without notice, unlike the public functions above.

#### 8. Internal Functions — `rs_rtc_logger.c`

All five are ⚫ **PRIVATE** (`static`, file-scoped).

##### ⚫ `rs_rl_is_open()`

```c
static bool rs_rl_is_open(rs_rtc_logger_ctrl_t const * p_ctrl);
```

Returns `true` if the RTC Event Logger is in any non-CLOSED state (OPEN, STARTED, or STOPPED); `false` if CLOSED or `p_ctrl` is NULL. Used internally by the public lifecycle functions to check state before transitioning.

##### ⚫ `rs_rl_enqueue()`

```c
static fsp_err_t rs_rl_enqueue(rs_rtc_logger_ctrl_t * p_ctrl,
                                rtc_time_t     const * p_timestamp,
                                uint8_t        const * p_data,
                                uint32_t               length);
```

Enqueues a timestamped event entry into the circular buffer. ISR-safe — uses a critical section (`__disable_irq()`/`__enable_irq()`, or the FreeRTOS-from-ISR equivalent) to atomically update buffer indices and count, and applies the configured overflow policy (`OVERWRITE_OLDEST` or `DROP_NEWEST`). This is the function that actually implements the buffering behavior the public `rs_rtc_logger_EventTrigger()` exposes.

Returns `FSP_SUCCESS` on enqueue, or `FSP_ERR_OVERFLOW` if the buffer is full and the policy is `DROP_NEWEST`.

##### ⚫ `rs_rl_dequeue()`

```c
static bool rs_rl_dequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                           rs_rtc_logger_event_entry_t * p_entry_out);
```

Dequeues the oldest entry from the circular buffer. Called from task context only (not ISR-safe by design — no ISR-side dequeue path exists). Updates `buf_tail` and `buf_count`. Compiled in for **both** Example Project mode and Expert mode so the public `rs_rtc_logger_EventDequeue()` wrapper can call it in either configuration. Returns `true` if an entry was dequeued, `false` if the buffer was empty.

##### ⚫ `rs_rl_auto_init()`

```c
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static fsp_err_t rs_rl_auto_init(rs_rtc_logger_ctrl_t * p_ctrl);
#endif
```

Performs the deferred auto-`Open()`/`Start()` sequence in Example Project mode. Called from the internal consumer task after it detects `auto_init_pending`. Guarantees the first `EventTrigger()` event (already enqueued before `Open()`/`Start()` ran) is not lost. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

##### ⚫ `rs_rtc_logger_internal_consumer_task()`

```c
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static void rs_rtc_logger_internal_consumer_task(void * p_arg);
#endif
```

The RTC Event Logger-owned FreeRTOS task body used in Example Project mode. Waits for task notifications from `EventTrigger()` (or the UART TX-complete callback), then drains the circular buffer by calling the public `rs_rtc_logger_Write()` for each pending entry, guarded by a TX timeout (`RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS`). This is the task created by the public `rs_rtc_logger_EP_PrepareConsumerTask()`. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

---

#### 9. Internal Functions — `rs_rtc_logger_format.c` / `.h`

`rs_rtc_logger_format.h` is explicitly marked in its own file header as *"an internal RTC Event Logger interface — it is not part of the public API."* It declares two 🔒 **INTERNAL** functions with external linkage (callable from `rs_rtc_logger.c`), backed by six ⚫ **PRIVATE** helpers local to `rs_rtc_logger_format.c`.

##### 🔒 `rs_rtc_logger_format_timestamp()` — INTERNAL

```c
uint32_t rs_rtc_logger_format_timestamp(rtc_time_t              const * p_time,
                                         rs_rtc_logger_ts_format_t       format,
                                         bool                            ts_brackets,
                                         char                          * p_buf,
                                         uint32_t                        buf_size);
```

Formats an `rtc_time_t` timestamp into a caller-supplied ASCII buffer, per the selected format (ISO 8601, time-only, compact, or syslog) and bracket option. Output is always NUL-terminated; integer arithmetic only; no heap allocation. Returns the number of characters written (not including NUL), or `0` on error (buffer too small or invalid format). Called by `rs_rtc_logger_format_line()` and, indirectly, by `rs_rtc_logger_Write()` / `rs_rtc_logger_WriteStr()`.

##### 🔒 `rs_rtc_logger_format_line()` — INTERNAL

```c
uint32_t rs_rtc_logger_format_line(rs_rtc_logger_event_entry_t const * p_entry,
                                    rs_rtc_logger_ts_format_t           format,
                                    bool                                ts_brackets,
                                    rs_rtc_logger_term_t                term,
                                    char                              * p_buf,
                                    uint32_t                            buf_size);
```

Composes a complete formatted log line (timestamp + event payload + sequence number + line terminator) from a circular buffer entry into a single NUL-terminated ASCII line, ready for UART TX. Returns total characters written, or `0` on error.

##### ⚫ `rs_rtc_logger_format_terminator()` — PRIVATE

```c
static uint32_t rs_rtc_logger_format_terminator(rs_rtc_logger_term_t term,
                                                 char               * p_buf,
                                                 uint32_t             offset,
                                                 uint32_t             buf_size);
```

Appends the line terminator string (`\r\n`, `\n`, or `\r`) to a caller-supplied buffer at the given offset. Returns the number of terminator characters written (1 or 2), or `0` if insufficient space. Deliberately kept out of the public/internal header — it was removed from there as part of a prior cleanup so it cannot be called from outside this file.

##### ⚫ `rm_rlf_write_u2()` / `rm_rlf_write_d2()` / `rm_rlf_write_year()` / `rm_rlf_write_uint()` / `rm_rlf_write_str()` — PRIVATE

Low-level ASCII-writing primitives shared by `rs_rtc_logger_format_timestamp()` and `rs_rtc_logger_format_line()`. All are pure, integer-only, and named with the `rm_rlf_` private prefix (module-local formatting helpers, distinct from the `rs_rtc_logger_` / `rs_rl_` prefixes used elsewhere):

| Function | Purpose |
|---|---|
| `rm_rlf_write_u2()` | Writes a 2-digit, zero-padded decimal integer (e.g. month, hour) |
| `rm_rlf_write_d2()` | Writes a 2-character, **space**-padded decimal integer — used for the syslog day field, since RFC 3164 requires a space-padded single-digit day (` 1`, not `01`) |
| `rm_rlf_write_year()` | Writes a 4-digit year, converting `tm_year` (years since 1900) to the full calendar year |
| `rm_rlf_write_uint()` | Writes a `uint32_t` as a variable-width decimal string (used for the sequence number) |
| `rm_rlf_write_str()` | Copies a fixed-length string (need not be NUL-terminated) into the output buffer |

---

#### 10. Internal Functions — `rs_rtc_logger_hal.c` / `.h`

`rs_rtc_logger_hal.h` is explicitly marked *"Internal header — not part of the public API."* It declares seven 🔒 **INTERNAL** HAL-wrapper functions, backed by two ⚫ **PRIVATE** helpers local to `rs_rtc_logger_hal.c`.

##### 🔒 RTC wrappers — INTERNAL

| Function | Purpose |
|---|---|
| `rs_rtc_logger_hal_rtc_open(p_ctrl)` | Wraps `p_rtc->p_api->open()`. Validates the RTC oscillator is running by probing with a `calendarTimeGet()` call after open. Returns `FSP_ERR_NOT_INITIALIZED` if the oscillator is not running. |
| `rs_rtc_logger_hal_rtc_close(p_ctrl)` | Wraps `p_rtc->p_api->close()`. |
| `rs_rtc_logger_hal_rtc_time_get(p_ctrl, p_time)` | Direct calendar-time read — see the "seconds-sandwich" implementation note in the source; deliberately does **not** call `R_RTC_CalendarTimeGet()` (the carry-interrupt method) to avoid the ISR-priority deadlock risk documented in [Configuration Guide](#3-required-non-default-fsp-settings). |
| `rs_rtc_logger_hal_rtc_time_set(p_ctrl, p_time)` | Wraps `p_rtc->p_api->calendarTimeSet()`. |
| `rs_rtc_logger_hal_loco_freq_set(p_ctrl, rfrl_value)` | Stops the RTC counter, writes a new RFRL value, reads back the retained calendar time, and restarts the counter — no seconds are lost. Backs the public `rs_rtc_logger_LocoCalibrationSet()`. Valid RFRL range: 8–510. |

##### 🔒 UART wrappers — INTERNAL

| Function | Purpose |
|---|---|
| `rs_rtc_logger_hal_uart_open(p_ctrl)` | Calls `rs_rtc_logger_console_init()` (see [Section 11](#11-internal-functions--rs_rtc_logger_stdio_consolec--h)), which ensures the shared Buffered I/O UART driver is initialized and strictly validated against the board reference configuration. It is safe to call even if the driver already opened by the UART earlier (e.g. via the reference application's sign-on banner's first `printf()`). |
| `rs_rtc_logger_hal_uart_close(p_ctrl)` | A no-op that always returns `FSP_SUCCESS`. The shared UART is owned by the Buffered I/O UART driver and is never closed for the life of the application. |
| `rs_rtc_logger_hal_uart_write(p_ctrl, p_data, length)` | Writes via `fwrite()` + `fflush(stdout)` on the same stdio stream the Buffered I/O UART driver owns. Returns once the write has been safely submitted/serialized (the driver's own `uart_flush()` spin-waits briefly on its internal busy flag if a previous transmission is still in flight). Increments `p_ctrl->tx_errors` and returns `FSP_ERR_WRITE_FAILED` if `fwrite()`/`fflush()` do not complete cleanly. |

##### ⚫ `rs_rl_bcd_to_dec()` — PRIVATE

```c
static uint8_t rs_rl_bcd_to_dec(uint8_t bcd);
```

Converts a packed BCD byte to a decimal integer (e.g. `0x21` → `21`). Used exclusively by `rs_rtc_logger_hal_rtc_time_get()` to decode RTC calendar registers. Integer arithmetic only, no division.

---

#### 11. Internal Functions — `rs_rtc_logger_stdio_console.c` / `.h`

Uses Buffered I/O over UART (see [Release Notes](#2-buffered-io-over-uart)). This file physically ships inside the `rs_rtc_logger/` core folder, but its three functions split across the public/private line differently than the name suggests — see the table below. It is a thin, non-blocking bridge between this component (and the reference CLI) and the vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`, used verbatim/unmodified).

| Function | Tag | Caller | Purpose |
|---|---|---|---|
| `rs_rtc_logger_console_init()` | 🔒 INTERNAL (core dependency) | `rs_rtc_logger_hal_uart_open()` — i.e. every `rs_rtc_logger_Open()` call | Forces deterministic initialization of the vendored driver (the driver otherwise initializes lazily on first `printf()`/`getchar()`/`fflush()`) and runs the driver's own public `RS_STDIO_UART_CheckApplicationConfiguration()` exactly once, caching the result. A validation failure is returned on every subsequent call too, since misconfiguration cannot be fixed by retrying — it propagates through `rs_rtc_logger_hal_uart_open()` and `rs_rtc_logger_Open()` the same way any other `Open()` failure does. |
| `rs_rtc_logger_console_getc_nb(p_c)` | Demo-only convenience | Reference CLI task (`demo_rx_getc()`) | Non-blocking single-character read. Peeks the vendored driver's RX byte queue depth via `R_BYTEQ_Used()` — a non-public but externally-linkable symbol (`uart_rx_q_ctrl`) — before ever calling the driver's own `getchar()`, which otherwise blocks indefinitely when nothing is queued (a documented limitation of the vendored driver). Returns `false` immediately if nothing is available; never blocks. |
| `rs_rtc_logger_console_puts(p_str)` | Demo-only convenience | Reference CLI task (`demo_uart_write()`) | Writes a NUL-terminated string via `fwrite()` then `fflush(stdout)`. Not used by `rs_rtc_logger_hal_uart_write()` itself, which calls `fwrite()`/`fflush()` directly (see [Section 10](#10-internal-functions--rs_rtc_logger_halc--h)) — this function exists purely for the reference CLI's convenience. |

`rs_rtc_logger_console_init()` is the one function in this file the core module actually depends on for `Open()` to succeed — despite the file's own header comment describing it as a "demo application" adapter, it sits on the core's call path. Keep this in mind if you ever consider dropping the file as pure demo scaffolding: doing so breaks `rs_rtc_logger_hal_uart_open()`. See [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) for the full reuse/modify guidance on this file.

---

#### 12. Complete Internal Function Reference Table

| Function | Tag | File | Compiled When |
|---|---|---|---|
| `rs_rl_is_open()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always |
| `rs_rl_enqueue()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always |
| `rs_rl_dequeue()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always (both modes) |
| `rs_rl_auto_init()` | ⚫ PRIVATE | `rs_rtc_logger.c` | `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1` |
| `rs_rtc_logger_internal_consumer_task()` | ⚫ PRIVATE | `rs_rtc_logger.c` | `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1` |
| `rs_rtc_logger_format_timestamp()` | 🔒 INTERNAL | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_format_line()` | 🔒 INTERNAL | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_format_terminator()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_u2()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_d2()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_year()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_uint()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_str()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_hal_rtc_open()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_close()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_time_get()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_time_set()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_loco_freq_set()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_open()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_close()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_write()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rl_bcd_to_dec()` | ⚫ PRIVATE | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_console_init()` | 🔒 INTERNAL (core dependency) | `rs_rtc_logger_stdio_console.c` | Always |
| `rs_rtc_logger_console_getc_nb()` | Demo-only convenience | `rs_rtc_logger_stdio_console.c` | Always (unused unless the demo calls it) |
| `rs_rtc_logger_console_puts()` | Demo-only convenience | `rs_rtc_logger_stdio_console.c` | Always (unused unless the demo calls it) |

---

## Integration Guide

*Read this when you are: Wiring the RTC Event Logger into your application.*

> This guide covers **how to wire the PES into your application** — initialization order, task/RTOS integration, and multi-instance considerations. For **adapting the core logic to a different event source or use case**, see [Reusability Guide](#reusability-and-adaptation-for-similar-use-cases). For the functions referenced below, see [Functions Guide](#functions-guide--public-and-private-functions).

### 1. Simple Path Integration (Example Project Mode)

This is the recommended starting point for most integrations. Set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1` (the default) in `rs_rtc_logger_cfg.h`.

Recommended call order, from your FreeRTOS thread entry:

```c
/* 1. Bind the RTC HAL instance (preset or FSP Stack Configurator) */
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);

/* 2. Optional but recommended: validate configuration early */
fsp_err_t err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
if (FSP_SUCCESS != err) { /* handle bad configuration */ }

/* 3. Create the PES-owned consumer task — task context only, before enabling any IRQ */
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);

/* 4. Enable your event source's interrupt (e.g. r_icu external IRQ) */
R_ICU_ExternalIrqOpen(&g_external_irq_ctrl, &g_external_irq_cfg);
R_ICU_ExternalIrqEnable(&g_external_irq_ctrl);

/* 5. From here on, your ISR calls only: */
rs_rtc_logger_EventTrigger(&g_logger_ctrl, p_data, length);
```

> `rs_rtc_logger_ConfigGen()` takes an RTC instance. The shared UART is opened automatically the first time your application makes any stdio call (`printf`/`getchar`/`fflush`), or deterministically inside `rs_rtc_logger_Open()` itself via `rs_rtc_logger_hal_uart_open()`. You do not need to open it yourself. See [Release Notes](#2-buffered-io-over-uart).

`rs_rtc_logger_Open()` and `rs_rtc_logger_Start()` are called automatically on the first `EventTrigger()` — you do not call them yourself in this mode. See [Architecture](#2-public-function-state-gating) footnote 1 for exactly what happens on that first call.

**Ordering constraint:** Step 3 (`EP_PrepareConsumerTask`) must complete *before* step 4 (enabling the interrupt), because `xTaskCreateStatic()` must not be called from ISR context and the PES has no way to defer task creation if the ISR fires first.

---

### 2. Expert Path Integration (Full Lifecycle Control)

Set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 0`. The application owns every transition and must provide its own consumer task.

```c
/* 1. Bind and validate configuration */
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
rs_rtc_logger_ConfigCheck(&g_logger_cfg);

/* 2. Open — validates RTC oscillator; ensures the shared Buffered I/O UART
 *    driver is initialized and validated (see rs_rtc_logger_hal_uart_open()) */
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);

/* 3. Restore warm-reset state, if applicable — see Section 3 below */

/* 4. Start — enables the UART output path */
rs_rtc_logger_Start(&g_logger_ctrl);

/* 5. Create your own consumer task, which loops on: */
rs_rtc_logger_event_entry_t entry;
if (FSP_SUCCESS == rs_rtc_logger_EventDequeue(&g_logger_ctrl, &entry))
{
    rs_rtc_logger_Write(&g_logger_ctrl, &entry);
}

/* 6. Enable your event source's interrupt — safe any time after Start() */

/* 7. Application-driven shutdown, if needed */
rs_rtc_logger_Stop(&g_logger_ctrl);
rs_rtc_logger_Close(&g_logger_ctrl);
```

Your consumer task can use whatever wait strategy fits your application (polling `EventDequeue()`, a task notification triggered from your own ISR path, a queue, etc.) — the PES does not prescribe one in Expert mode, unlike the internal consumer task used in Example Project mode.

---

### 3. Warm-Reset Integration Ordering

On a warm reset (detected via `R_SYSTEM->RSTSR0` POR flag), the RTC keeps running and its configuration is retained, but the PES's own state (sequence counter, "is the clock trustworthy" flag) resets with the rest of RAM unless your application restores it from non-volatile/`BSP_SECTION_NOINIT` storage. Integrate the restore calls **after `Open()`, before enabling any interrupt source that calls `EventTrigger()`**:

```c
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);

if (warm_reset_detected)
{
    rs_rtc_logger_SeqCountSet(&g_logger_ctrl, restored_seq_count);
    rs_rtc_logger_ClockValidate(&g_logger_ctrl, true);
}

rs_rtc_logger_Start(&g_logger_ctrl);
/* enable interrupt source only after this point */
```

**Why ordering matters:** if an `EventTrigger()` ISR fires between the moment your application chooses the restore value and the `SeqCountSet()` store executing, the ISR's `seq_count++` is overwritten by the restore, silently creating a duplicate sequence number in the output. `rs_rtc_logger_Open()` always initializes the "clock is trustworthy" flag to `false`; skipping `ClockValidate(true)` on a warm reset causes the first few events to display `[0000-00-00 00:00:00]` even though the RTC actually holds a valid, previously-set time.

In Example Project mode, `Open()`/`Start()` happen automatically inside the deferred auto-init path — if you need `SeqCountSet()`/`ClockValidate()` restored before the first event is transmitted, call them from your own warm-reset detection code *before* the first `EventTrigger()` call, so they land before the auto-init sequence starts.

---

### 4. Task and RTOS Integration

| Consideration | Example Project Mode | Expert Mode |
|---|---|---|
| Consumer task ownership | PES-owned (internal static task, created by `rs_rtc_logger_EP_PrepareConsumerTask()`) | Application-owned |
| Task stack size | `RS_RTC_LOGGER_CFG_TASK_STACK_SIZE` (default 2048 bytes) | Sized by your own task creation |
| Task priority | `RS_RTC_LOGGER_CFG_TASK_PRIORITY` (default `tskIDLE_PRIORITY + 1`) | Set by your own task creation |
| Consumer task wake interval | `RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS` (default 1000 ms) — periodic wake-up bound on `ulTaskNotifyTake()`, not a completion timeout (writes are synchronous — see [Architecture](#3-event-capture-and-telemetry-flow)) | Your responsibility to guard if desired |
| FreeRTOS heap scheme | `heap_4` (or `heap_3`) required for `xTaskCreateStatic()` | Same requirement applies if you use static allocation |
| `configSUPPORT_STATIC_ALLOCATION` | Must be `1` | Only required if your own task uses static allocation |

Tune `RS_RTC_LOGGER_CFG_TASK_PRIORITY` relative to your other application tasks — see the adaptation table in [Configuration Guide](#key-adaptation-points).

---

### 5. Shared UART and Multi-Instance Considerations

**The RTC Event Logger uses a Buffered I/O over UART driver** (see [Release Notes](#2-buffered-io-over-uart)). The RTC Event Logger component does not open, own, or register a callback for a UART instance.

The shared physical UART is owned by the **vendored Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`), and both the RTC Event Logger's log output and the reference application's CLI console write through the *same* stdio stream (`stdout`), via `fwrite()`/`fflush()`. This has two practical implications for integration:

1. **Single shared UART, by design.** The vendored driver is written around one physical UART instance (`g_uart_jlob_vcom`) and one pair of RX/TX byte queues. If your application needs the RTC Event Logger to log to a *different* UART than your CLI console uses, you would need to either: (a) run two independent copies of the vendored driver bound to different UART instances (not a supported out-of-the-box configuration — the driver's global byte-queue state, described in `r_byteq.c`, is written for a single instance), or (b) have the RTC Event Logger's log lines share the same stream as your console, as the reference application does.
2. **Multiple `rs_rtc_logger_ctrl_t` instances within one application** are possible in principle (the core module itself has no static state that assumes a single control block — see [Functions Guide](#functions-guide--public-and-private-functions)), but since all instances would write through the same shared stdio stream, their log lines would interleave on the same physical UART. This has not been validated by the reference implementation; treat it as a case-by-case adaptation.

If you need genuinely independent UART channels for different purposes (e.g. one for logging, one for a separate protocol), do not attempt to share the vendored driver across them — give the second channel its own `uart_instance_t` opened directly through the FSP HAL, bypassing the Buffered I/O UART driver and `rs_rtc_logger_stdio_console.c` entirely for that channel.

---

### 6. Firmware-Update Integration Pattern (Expert Mode)

In Expert mode, the `rs_rtc_logger_Stop()`/`rs_rtc_logger_Start()` pair can be used to silence UART activity during a firmware write operation while continuing to buffer events:

```c
// Before firmware write — silence UART output
rs_rtc_logger_Stop(&g_logger_ctrl);

// ... firmware write executes here ...
// rs_rtc_logger_EventTrigger() events continue to buffer silently

// After firmware verify — resume logging from same seq_count
rs_rtc_logger_Start(&g_logger_ctrl);
// Buffered events are transmitted in order
```

The `seq_count` and circular buffer are preserved across `rs_rtc_logger_Stop()`/`rs_rtc_logger_Start()`. Events that arrived during the STOPPED window are transmitted in order once STARTED. This pattern is not applicable in Example Project mode, since the PES owns the lifecycle transitions automatically in that mode.

---

## File Guide — Naming Conventions and What Is Safe to Modify

*Read this when you are: Deciding what to copy, keep, or replace.*

### 1. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference or demo application layer. The configuration path column refers to two options explained fully in [Configuration Guide](#5-adapting-to-your-board): the **Preset path** (default, uses pre-built HAL instances that work out of the box) and the **FSP Stack Configurator path** (for custom boards or modified hardware settings).

| Layer | Files | Purpose | Reuse Expectation |
|---|---|---|---|
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; circular buffer management; `EventTrigger()` ISR path; Example Project auto-lifecycle and internal consumer task | Copy into your project; core logic |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.h` | Public interface header — all public function prototypes | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_types.h` | Public type definitions and enumerations | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_cfg.h` | Build-time configuration constants with `#ifndef` guards | Override individual constants as needed for your project |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_format.c/.h` | Pure timestamp formatting — converts `rtc_time_t` to ASCII string; no I/O; integer arithmetic only | Copy into your project; no board-specific content |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_hal.c/.h` | Thin HAL shim: RTC open/close/get/set wrappers (direct raw-driver calls). UART open/close/write wrappers — they route through `rs_rtc_logger_stdio_console.c` to the shared Buffered I/O UART driver's stdio stream. (see [Release Notes](#2-buffered-io-over-uart)) | Copy into your project; board-agnostic |
| **Core (new dependency)** | `src/rs_rtc_logger/rs_rtc_logger_stdio_console.c/.h` | Thin adapter bridging this module to the vendored Buffered I/O UART driver. Physically lives in the core folder; its `rs_rtc_logger_console_init()` is called from `rs_rtc_logger_hal_uart_open()`, making it a genuine core dependency, not just demo glue — see [Functions Guide](#11-internal-functions--rs_rtc_logger_stdio_consolec--h) | Copy into your project alongside the rest of core; board-agnostic itself, but requires the vendored driver below |
| **Vendored (new, used unmodified)** | `src/rs_stdio_uart_buffer_io/` (`r_byteq.c/.h`, `rs_stdio_uart.c/.h`, `syscalls_picolib_stdio_to_uart.c`, plus one `stdio_uart_config_<board>_fpb.c` per board) | Buffered I/O UART driver: byte-queue-backed picolibc stdio-to-UART bridge. Owns the shared physical UART (`g_uart_jlob_vcom`) for the whole application — both log output and CLI console | Use verbatim; do not modify (see [Section 4a](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) below). Add a new `stdio_uart_config_<board>_fpb.c` when porting to a new board. |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.c/.h`<br>`src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.c/.h` | Pre-built **RTC-only** HAL instance definitions per board (no UART preset any more — see [Configuration Guide](#5-adapting-to-your-board)). Also declares/defines `rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, and `rs_rtc_logger_ConfigGetSize()` (see [Functions Guide](#2-configuration-bridge-call-before-open)) — these three functions are declared unconditionally regardless of `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` | Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see [Configuration Guide](#configuration-guide)) |
| **Reference App** | `src/rs_rtc_logger_demo.c` | Full demonstration of both modes; CLI task, consumer task (Expert mode), button ISR (including software debounce — see [Configuration Guide](#6-debounce-configuration-reference-application)), LOCO calibration. CLI I/O routes through `rs_rtc_logger_console_getc_nb()`/`rs_rtc_logger_console_puts()`. Shared, unmodified, by both boards in the Meson build — board selection is via `RS_RTC_LOGGER_TARGET_RA4E2` / `RS_RTC_LOGGER_TARGET_RA6E2` | Adapt or replace entirely for your application |
| **Reference App** | `src/blinky_thread_entry.c` (one copy per e2studio board project; `blinky_thread_entry_ra4e2.c` / `_ra6e2.c` in the Meson `src/` tree) | FreeRTOS thread entry; sign-on banner; `r_icu` External IRQ open/enable; LED blink. The banner calls `printf()`, which transparently triggers the vendored driver's init | Adapt for your board; not reusable as-is |
| **Reference App** | `src/hal_entry.c` | FSP v6.5.0 application hook, called after `g_hal_init()` in Meson firmware builds; intentionally empty (FreeRTOS scheduler owns application logic via `blinky_thread_entry_*.c`). Not present in the e2studio example projects, where the generated `main.c` does not call `hal_entry()` | Meson build only; adapt if your `main()` flow calls `hal_entry()` |
| **Reference App** | `src/hal_warmstart.c` / `.h` | Warm-start detection helper used by demo (Meson build only — RASC generates its own copy for e2studio example projects) | Adapt for your application |
| **Reference App** | `src/fault_handler.c` | Fault handler for debug | Optional; adapt or replace |
| **Reference App** | `src/crt0_stub.c` | Weak `_start` stub required by the Meson/ATfE toolchain link step (picolibc compatibility); not applicable to e2studio-built projects | Meson build only; leave as-is unless changing toolchains |
| **FSP Generated** | `ra/` and `ra_gen/` directories (e2studio example projects) / `firmware/generated/` (Meson build) | FSP-generated configuration, drivers, middleware. `g_uart_jlob_vcom` (the shared VCOM UART instance) is generated here — the FSP naming convention renamed what earlier development iterations called `g_uart0` | Do not edit manually; regenerate via e2studio FSP tooling or the `rasc_generate` Meson target |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it. The CLI and the core module's log writes go through the vendored Buffered I/O UART driver, shared by both — see the new rows above.

---

### 2. Naming Conventions Used in This Module

All identifiers follow a consistent scheme using the `rs_` / `RS_` prefix, which identifies Renesas reference software and avoids conflicts with FSP middleware names (`rm_` / `RM_`). The short name for this module is `rtc_logger`.

| Category | Pattern | Examples |
|---|---|---|
| **Public functions** | `rs_rtc_logger_OperationName()` — lowercase prefix, UpperCamelCase operation | `rs_rtc_logger_EventTrigger()`, `rs_rtc_logger_TimeSet()`, `rs_rtc_logger_StatusGet()` |
| **Internal helper functions** | `rs_rtc_logger_*` or abbreviated `rs_rl_*` — all lowercase | `rs_rtc_logger_hal_uart_write()`, `rs_rl_enqueue()`, `rs_rl_dequeue()` |
| **Build-time macros and config symbols** | `RS_RTC_LOGGER_CFG_*` — all uppercase | `RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH`, `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE`, `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` |
| **Version macros** | `RS_RTC_LOGGER_VERSION*` — all uppercase | `RS_RTC_LOGGER_VERSION_MAJOR`, `RS_RTC_LOGGER_VERSION` |
| **Enumeration type names** | `rs_rtc_logger_*_t` — lowercase with `_t` suffix | `rs_rtc_logger_state_t`, `rs_rtc_logger_ts_format_t` |
| **Enumeration values** | `RS_RTC_LOGGER_GROUP_VALUE` — all uppercase | `RS_RTC_LOGGER_STATE_STARTED`, `RS_RTC_LOGGER_TS_FORMAT_ISO8601`, `RS_RTC_LOGGER_TERM_CRLF` |
| **Struct type names** | `rs_rtc_logger_*_t` — lowercase with `_t` suffix | `rs_rtc_logger_cfg_t`, `rs_rtc_logger_ctrl_t`, `rs_rtc_logger_status_t` |
| **Source and header files** | `rs_rtc_logger_*.c / .h` — lowercase | `rs_rtc_logger.c`, `rs_rtc_logger_cfg.h`, `rs_rtc_logger_types.h` |
| **Core source folder** | `src/rs_rtc_logger/` | All core module files live here |
| **Board config files** | `rs_rtc_logger_data_<board>.c/.h` | `rs_rtc_logger_data_fpb_ra4e2.c/.h` |
| **Project name** | `UseCaseName_<board>` — descriptive, not prefixed with `rs_` | `RTC_Event_Logger_FPB_RA4E2` |

The key rule to remember: **public functions use lowercase `rs_rtc_logger_` prefix** with an UpperCamelCase operation name (e.g. `rs_rtc_logger_EventTrigger`). Everything that is a macro, constant, or enumeration value uses the **all-uppercase `RS_RTC_LOGGER_`** form. Type names and internal helpers are always all-lowercase.

---

### 3. Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_rtc_logger.c` | With care | Core logic; add capability at defined extension points only |
| `rs_rtc_logger.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_rtc_logger_types.h` | No | Public types; changing structures may break binary compatibility |
| `rs_rtc_logger_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_rtc_logger_format.c/.h` | With care | Pure formatting; portable; can be unit-tested in isolation |
| `rs_rtc_logger_hal.c/.h` | With care | Thin HAL shim; RTC wrappers call the raw driver directly, UART wrappers route through `rs_rtc_logger_stdio_console.c` — modify only if switching UART driver variant or RTC driver |
| `rs_rtc_logger_stdio_console.c/.h` | With care | Depends on Vendored Buffered I/O UART driver. `rs_rtc_logger_hal_uart_open()` depends on `rs_rtc_logger_console_init()` here — do not remove this file without re-pointing that dependency |

---

### 4. Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for the FPB-RA4E2 and FPB-RA6E2 and are marked "do not edit." They are reference-quality reproductions of FSP-generated output for the specified board's **RTC instance only** — there is no UART preset (see [Section 4a](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) below for the UART side). To customize the RTC configuration for a different board, switch to the FSP Stack Configurator path as described in [Configuration Guide](#5-adapting-to-your-board) (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`).

| File | Notes |
|---|---|
| `config/rs_rtc_logger_data_fpb_ra4e2.c` | Preset RTC instance definition for FPB-RA4E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra4e2.h` | Preset RTC instance declaration; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.c` | Preset RTC instance definition for FPB-RA6E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.h` | Preset RTC instance declaration; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |

---

### 4a. Vendored Buffered I/O UART Driver — Reference Only; Do Not Edit

The Buffered I/O over UART driver (see [Release Notes](#2-buffered-io-over-uart)) is used **verbatim, unmodified** — it is a separate component, not merged into the RTC Event Logger's own source tree, and is documented here only so you know what it is and where its board-specific boundary is.

| File | Notes |
|---|---|
| `rs_stdio_uart_buffer_io/r_byteq.c/.h` | Generic byte-queue implementation backing the RX/TX streams; board-agnostic; do not edit |
| `rs_stdio_uart_buffer_io/rs_stdio_uart.c/.h` | Generic UART instance config/validation helpers, supporting `r_sci_uart`, `r_sci_b_uart`, or `r_uarta` depending on what the target's FSP HAL provides; do not edit |
| `rs_stdio_uart_buffer_io/syscalls_picolib_stdio_to_uart.c` | picolibc stdio stream redirection (`printf`/`getchar`/`fflush` → UART), lazy driver init, ISR callback; do not edit |
| `rs_stdio_uart_buffer_io/stdio_uart_config_ra4e2_fpb.c` | **Board-specific** — the one file in this directory you copy and adapt when porting to a new board. Guarded by `BOARD_RA4E2_FPB`; provides the reference pin mapping, baud registers, and `RS_STDIO_UART_CheckApplicationConfiguration()` reference values for FPB-RA4E2 |
| `rs_stdio_uart_buffer_io/stdio_uart_config_ra6e2_fpb.c` | Same as above, for FPB-RA6E2, guarded by `BOARD_RA6E2_FPB` |

Porting to a new board means adding a new `stdio_uart_config_<board>.c` file following the pattern in the two files above — see [Configuration Guide](#5-adapting-to-your-board).

---

### 5. Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_rtc_logger_demo.c` | Starting point for your CLI task and ISR callback; replace or heavily adapt. Board-agnostic (selects target via `RS_RTC_LOGGER_TARGET_RA4E2`/`_RA6E2`). CLI I/O goes through `rs_rtc_logger_console_getc_nb()`/`rs_rtc_logger_console_puts()` |
| `blinky_thread_entry_ra4e2.c` / `blinky_thread_entry_ra6e2.c` (Meson `src/`)<br>`blinky_thread_entry.c` (each e2studio example project) | FreeRTOS thread entry; adapt for your board's LED/button layout and initialization order. The sign-on banner is `printf()` calls |
| `hal_entry.c` | FSP v6.5.0 application hook (Meson build only); adapt if your application needs post-`g_hal_init()` logic that does not belong in the FreeRTOS thread entry |
| `hal_warmstart.c` | Warm-start detection; adapt if your system uses a different reset detection mechanism |
| `fault_handler.c` | Debug fault handler; replace with your production fault handling |

---

### 6. FSP-Generated Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` (e2studio example projects) | FSP-generated drivers, middleware, CMSIS, and board support; regenerate via e2studio FSP tooling when changing `configuration.xml` |
| `ra_gen/` (e2studio example projects) | Auto-generated by FSP Stack Configurator; overwritten on every re-generation |
| `firmware/generated/` (Meson build) | RASC `custom_target` outputs copied by `run_rasc.py` / `run_rasc_bundle.py`; overwritten by the `rasc_generate` Meson target |

---

## Design Flow and Architecture

*Read this when you are: Understanding internal design or debugging state issues.*

### 1. Lifecycle State Machine

The module implements a four-state lifecycle. State transitions are the only way to change state; no transition occurs implicitly except for the auto-initialization on first `rs_rtc_logger_EventTrigger()` call in Example Project mode.

```
         rs_rtc_logger_Open()              rs_rtc_logger_Start()
CLOSED ─────────────────────→ OPEN ──────────────────────────→ STARTED
   ↑                           │   ↑   rs_rtc_logger_Stop()   │
   │   rs_rtc_logger_Close()   │   └────────────────────────←┘
   └───────────────────────────┘
                               │
         rs_rtc_logger_Close() │   (from STOPPED also)
                               ↓
                             CLOSED
```

| State | Description |
|---|---|
| CLOSED | Resources not allocated; both HAL drivers closed |
| OPEN | Initialized; UART output path not yet active; `rs_rtc_logger_TimeSet()`, `rs_rtc_logger_SeqCountSet()` may be called |
| STARTED | UART output path active; `rs_rtc_logger_EventTrigger()` and `rs_rtc_logger_Write()` accepted |
| STOPPED | TX suspended; buffer continues accepting events; `rs_rtc_logger_Start()` resumes transmission |

---

### 2. Public Function State Gating

| Public Function | CLOSED | OPEN | STARTED | STOPPED |
|---|---|---|---|---|
| `rs_rtc_logger_Open()` | ✓ | ERR_ALREADY_OPEN | ERR_ALREADY_OPEN | ERR_ALREADY_OPEN |
| `rs_rtc_logger_Start()` | ✗ | ✓ | ERR_INVALID_MODE | ✓ |
| `rs_rtc_logger_Stop()` | ✗ | ✗ | ✓ | ERR_INVALID_MODE |
| `rs_rtc_logger_Close()` | ✗ | ✓ | Stop first | ✓ |
| `rs_rtc_logger_EventTrigger()` | EP only¹ | ✗ | ✓ | ✓ |
| `rs_rtc_logger_Write()` | ✗ | ✗ | ✓ | ✗ |
| `rs_rtc_logger_WriteStr()` | ✗ | ✗ | ✓ | ✗ |
| `rs_rtc_logger_TimestampFormatSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_TimeSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_ClockValidate()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_LocoCalibrationSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_SeqCountSet()` | ✗ | ✓ | ✗ | ✓ |
| `rs_rtc_logger_StatusGet()` | ✗ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_TimeGet()` | ✗ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_VersionGet()` | ✓ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_ConfigGen()` / `ConfigCheck()` / `ConfigGetSize()` | ✓ | ✓ | ✓ | ✓ |

¹ EP only = Example Project mode only: `rs_rtc_logger_EventTrigger()` in CLOSED state enqueues the event immediately (no event loss) and triggers a deferred auto-`rs_rtc_logger_Open()`/`rs_rtc_logger_Start()` sequence. Expert mode returns `FSP_ERR_NOT_OPEN` in CLOSED state.

*hot = hot-safe: callable while STARTED; takes effect on the next `rs_rtc_logger_Write()` call without interrupting the current transmission. See the definition of "hot-safe" at the top of [Functions Guide](#functions-guide--public-and-private-functions).*

---

### 3. Event Capture and Telemetry Flow

```
Application ISR (r_icu button callback)
  │
  └──→ rs_rtc_logger_EventTrigger(p_ctrl, p_data, length)
            │
            ├── R_RTC_CalendarTimeGet()        ← timestamp captured at ISR time
            ├── seq_count++ (atomic)
            ├── enqueue to event_buf[buf_head]  ← inline copy, no pointers
            └── xTaskNotifyFromISR()
                    │
                    ↓
         Consumer Task (module-owned in Example Project mode /
                        app-owned in Expert mode)
            │
            ├── dequeue event_buf[buf_tail]
            └── rs_rtc_logger_Write(p_ctrl, &entry)
                    │
                    ├── format: "[YYYY-MM-DD HH:MM:SS] Event Detected #N\r\n"
                    └── rs_rtc_logger_hal_uart_write(line_buf)
                            │
                            ├── fwrite(line_buf, ..., stdout)   ← queues into the
                            │                                     Buffered I/O UART
                            │                                     driver's TX byteq
                            └── fflush(stdout)                  ← hands queued bytes
                                    │                              to the UART driver;
                                    │                              briefly spin-waits
                                    │                              on the driver's own
                                    │                              busy flag if a prior
                                    │                              transmission is still
                                    │                              in flight, then returns
                                    ↓
                    (call returns here — write is safely submitted/
                     serialized; consumer task proceeds to the next
                     entry immediately, no separate completion wait)
```

`rs_rtc_logger_hal_uart_write()` goes through `fwrite()`/`fflush(stdout)` on the shared Buffered I/O UART driver's stdio stream and does not return until the write has already been safely handed off (the driver's own `uart_flush()` serializes internally).

In the reference application, the button ISR path additionally applies a software debounce (`demo_button_s1_callback()`) before calling `rs_rtc_logger_EventTrigger()` — see [Configuration Guide](#6-debounce-configuration-reference-application). That debounce sits entirely in application code, upstream of the diagram above; the core module's flow is unchanged regardless of what triggers `EventTrigger()`.

---

### 4. Module File Responsibilities

| Module | Responsibility |
|---|---|
| `rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; state gating; `rs_rtc_logger_EventTrigger()` ISR path; circular buffer management; diagnostic counters; Example Project mode auto-lifecycle logic and internal consumer task (compiled in only when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` is enabled) |
| `rs_rtc_logger_format.c` | Pure timestamp formatting — converts `rtc_time_t` plus format selection into an ASCII string in a caller-supplied buffer. No I/O. No side effects. Integer arithmetic only. Fully unit-testable in isolation. |
| `rs_rtc_logger_hal.c` | Thin HAL shim: RTC open/close/get/set wrappers (direct raw-driver calls). UART open/close/write wrappers route through `rs_rtc_logger_stdio_console.c` to the shared Buffered I/O UART driver's stdio stream.
| `rs_rtc_logger_stdio_console.c` | Thin adapter to the vendored Buffered I/O UART driver: forces deterministic driver init + validation (`rs_rtc_logger_console_init()`, called from `rs_rtc_logger_hal_uart_open()` — a genuine core dependency), plus non-blocking char read and string write used by the reference CLI |
| `rs_rtc_logger_data_fpb_<board>.c/.h` | Board preset **RTC** instance; also hosts `rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, `rs_rtc_logger_ConfigGetSize()` (see [Functions Guide](#2-configuration-bridge-call-before-open)) |
| `rs_stdio_uart_buffer_io/*` | **Vendored, used unmodified.** Owns the shared physical UART for the whole application (log output and CLI console alike) — see [File Guide](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) |

---

## Demonstrated Behavior and Performance

*Read this when you are: Validating the module meets your timing/memory budget.*

### 1. Functional Behavior Demonstrated

- RTC calendar time set by user via VCOM CLI in `[YYYY-MM-DD HH:MM:SS]` format on power-on reset
- Warm reset detection via `R_SYSTEM->RSTSR0_b.PORF`; RTC configuration preserved; sequence counter restored from `BSP_SECTION_NOINIT` backup. `rs_rtc_logger_hal_rtc_open()` explicitly clears `PORF` after reading it on a power-on reset, so the flag correctly reflects only the reset that just occurred. Without this, the sticky flag would remain set and cause every subsequent reset — including a genuine warm reset — to be misdetected as another power-on reset, silently discarding the retained RTC time. See [Release Notes](#2-buffered-io-over-uart) for details.
- External interrupt from user button S1 (P304/IRQ9) triggers `rs_rtc_logger_EventTrigger()` from ISR context, after the reference application's software debounce (see [Configuration Guide](#6-debounce-configuration-reference-application))
- Timestamped log line transmitted over VCOM: `[YYYY-MM-DD HH:MM:SS] Event Detected #<n>`
- LOCO drift calibration via RFRL register adjustment at startup
- Events logged before the time is set display as `[0000-00-00 00:00:00]` with a per-event re-prompt for clock setting

---

### 2. Performance Characteristics

**`rs_rtc_logger_EventTrigger()` ISR overhead:**
The ISR path performs: one `R_RTC_CalendarTimeGet()` HAL call, one atomic `seq_count` increment, one inline copy of the event payload (bounded by `RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN`, default 32 bytes) into the circular buffer, and one `xTaskNotifyFromISR()` call. No UART activity, no formatting, and no heap allocation occur in the ISR path. Execution time is deterministic and bounded.

This overhead applies from the moment `rs_rtc_logger_EventTrigger()` is entered. It does **not** include the reference application's software debounce check in `demo_button_s1_callback()` (a tick-count comparison), which runs before `EventTrigger()` is called and is negligible in cost but is application code, not core module overhead.

**UART TX latency:**
`rs_rtc_logger_hal_uart_write()` submits through `fwrite()`+`fflush(stdout)` on the shared Buffered I/O UART driver's stdio stream, rather than initiating a non-blocking UART write and waiting on a task notification from a UART TX-complete callback. The call briefly blocks the calling task only if a previous transmission (from either the log path or the CLI console) is still physically in flight — the vendored driver's `uart_flush()` spin-waits on its own internal busy flag before queuing the new data — otherwise it returns immediately after queuing and flushing. The typical latency remains sub-millisecond at 115200 baud for log-line-length strings.

**Heap allocation** The core RTC Event Logger module itself performs no dynamic memory allocation: all its buffers are statically allocated or stack-resident within the control block, and no `malloc`/`free` calls appear in `rs_rtc_logger.c`, `rs_rtc_logger_format.c`, or `rs_rtc_logger_hal.c`. However, the **Buffered I/O UART driver** (`syscalls_picolib_stdio_to_uart.c`) does call `malloc()`/`free()` internally, inside its `uart_flush()` implementation, to stage the byte-queue contents into a transmit buffer before handing them to the UART HAL. This is a property of the vendored Buffered I/O driver — if your project has a no-heap requirement, account for this dependency's allocation pattern (or budget for it). See [File Guide](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit).

**RAM consumption (default configuration):**
- Circular buffer: 8 entries × ~80 bytes per entry (`rtc_time_t`/`struct tm` (9×int = 36 B) + 32-byte payload + seq_count (4 B) + length (4 B) + time_valid (4 B padded)) ≈ 640 bytes
- Line buffer: 256 bytes (inside `rs_rtc_logger_ctrl_t`)
- Total control block: verified at build time via `static_assert` against `RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES` (default 1024 bytes)
- Internal consumer task (Example Project mode): stack 2048 bytes + TCB ~88 bytes on Cortex-M33

**No dynamic memory allocation in the core module.** All of `rs_rtc_logger`'s own buffers are statically allocated or stack-resident within the control block; no `malloc`/`free` calls appear in `rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`, or `rs_rtc_logger_stdio_console.c`. As noted above, the vendored Buffered I/O UART driver does allocate/free heap memory internally.

**No floating-point.** Timestamp formatting uses integer arithmetic only throughout.

---

### 3. Clock Accuracy Note

LOCO provides lower accuracy than the sub-clock (SOSC) crystal oscillator. For the RA4E2, LOCO typically drifts in the range of several hundred to several thousand ppm, which is approximately 0.5–5 minutes per day. For applications requiring higher time accuracy, the sub-clock with the external 32.768 kHz crystal (Y2 on FPB-RA4E2) is recommended — see [Reusability Guide](#reusability-and-adaptation-for-similar-use-cases) for the adaptation path. Runtime drift compensation is available via `rs_rtc_logger_LocoCalibrationSet()`, which adjusts the RFRL register by approximately 3906 ppm per unit.

---

## Reusability and Adaptation for Similar Use Cases

*Read this when you are: Reusing the module for a different use case.*

> This guide covers **adapting the core logic to a different event source or use case**. For **wiring the RTC Event Logger into your application as-is** (initialization order, task integration, multi-instance considerations), see [Integration Guide](#integration-guide).

### 1. Core Function Reusability

The `rs_rtc_logger_EventTrigger()` function is the universal entry point and is by design **agnostic to the source of the external interrupt**. The core module has no knowledge of what generated the call. Any interrupt source — `r_icu` GPIO, accelerometer callback, timer expiry, UART RX, or a direct software call — can trigger a timestamped log entry by calling this one function. No changes to the core source are required when switching interrupt sources.

Note that the reference application's button-press path adds a software debounce (`demo_button_s1_callback()`, `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` — see [Configuration Guide](#6-debounce-configuration-reference-application)) *before* calling `EventTrigger()`. That debounce is specific to a mechanical switch and is not carried along automatically when you swap in a different event source — decide independently whether your new source needs deglitching.

---

### 2. Portions Applicable Without Modification

- All files in `src/rs_rtc_logger/` can be included directly in any FSP v6.5.0 FreeRTOS project targeting the RA4E2 or RA6E2 without modification, provided the configuration is supplied via `rs_rtc_logger_ConfigGen()`. This requires bringing along the vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`, used unmodified) and a matching `stdio_uart_config_<board>.c` file for your target board — `rs_rtc_logger_hal_uart_open()` depends on it via `rs_rtc_logger_stdio_console.c`. See [File Guide](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit).
- The timestamp formatter (`rs_rtc_logger_format.c`) is a pure function with no I/O or side effects and can be extracted for use in unit tests or other logging implementations.
- The circular buffer and ISR-safe enqueue logic inside `rs_rtc_logger.c` represents a portable, heap-free, single-producer/single-consumer event buffer pattern reusable in similar telemetry designs. See [Functions Guide](#8-internal-functions--rs_rtc_loggerc) for the internal `rs_rl_enqueue()` / `rs_rl_dequeue()` functions that implement this pattern.

---

### 3. Portions Requiring Modification

- **Board-specific RTC preset config files** (`config/rs_rtc_logger_data_fpb_<board>.c/.h`): Create new preset files for other target boards, or switch to the FSP Stack Configurator path (see [Configuration Guide](#5-adapting-to-your-board)). These now cover the RTC instance only.
- **Board-specific Buffered I/O UART config** (`src/rs_stdio_uart_buffer_io/stdio_uart_config_<board>.c`): Create a new one for other target boards, following the pattern in `stdio_uart_config_ra4e2_fpb.c` / `_ra6e2_fpb.c`. The UART channel, baud rate, and pin configuration live here now, not in the RTC preset files.
- **Reference application** (`rs_rtc_logger_demo.c`, `blinky_thread_entry_ra4e2.c` / `_ra6e2.c`): Replace the CLI task, consumer task structure, ISR callback, and debounce logic with application-specific code. The demo application is demonstration scaffolding, not production code.
- **RTC clock source**: Replace LOCO with the sub-clock (SOSC) for applications requiring higher time accuracy. Change the `r_rtc` clock source property in `configuration.xml` and remove or adapt the LOCO calibration prompt.

---

### 4. Example: Adapting for an Accelerometer Event Source

Replace the `r_icu` button ISR callback with an accelerometer driver callback:

```c
// In your accelerometer interrupt callback:
void accel_threshold_callback(accel_callback_args_t * p_args)
{
    (void)p_args;
    rs_rtc_logger_EventTrigger(&g_logger_ctrl,
                               (uint8_t const *)"Threshold Exceeded",
                               18U);
}
```

No changes to the core module source are required. The module is entirely agnostic to the interrupt source. If your accelerometer can produce rapid repeated triggers that should be coalesced (analogous to mechanical switch bounce), add your own deglitch logic in this callback — the button debounce in the reference application does not apply here.

For the initialization order, task integration, and multi-instance considerations relevant to wiring this new event source in, see [Integration Guide](#integration-guide).

---

## Release Notes and Adopting a Newer Release

*Read this when you are: Adopting a newer release of this module.*

### 1. Current Version

| Field | Value |
|---|---|
| Version | 1.0.0 |
| FSP Baseline | v6.5.0 |
| Primary Target | FPB-RA4E2 |
| Secondary Target | FPB-RA6E2 |

Both targets include the Buffered I/O over UART driver described in [Section 2](#2-buffered-io-over-uart) below.

Query the running version at any time (including CLOSED state) via `rs_rtc_logger_VersionGet()` — see [Functions Guide](#5-configuration-and-query-functions).

---

### 2. Buffered I/O over UART

This release affects how the RTC Event Logger transmits and receives console/log data over UART. It shares a single physical UART with the reference application's CLI console through a vendored **Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`), accessed through standard C stdio (`printf`/`getchar`/`fwrite`/`fflush`).

**Delivered and confirmed on both reference boards:** FPB-RA4E2 and FPB-RA6E2 (`RTC_Event_Logger_FPB_RA4E2` and `RTC_Event_Logger_FPB_RA6E2` e2studio projects). The two board ports are symmetric — same core module, same adapter, same vendored driver — differing only in the per-board files called out in [Section 2.2](#22-per-board-differences) below.

#### 2.1 New components

| Component | Nature | Purpose |
|---|---|---|
| `src/rs_stdio_uart_buffer_io/` | Vendored, used **unmodified** — not part of the RTC Event Logger's own source tree | Byte-queue-backed (`r_byteq.c/.h`) picolibc stdio-to-UART bridge (`syscalls_picolib_stdio_to_uart.c`); generic UART instance config/validation helpers (`rs_stdio_uart.c/.h`) supporting `r_sci_uart`, `r_sci_b_uart`, or `r_uarta` depending on what the target MCU's FSP HAL provides; per-board reference UART configuration (`stdio_uart_config_ra4e2_fpb.c` guarded by `BOARD_RA4E2_FPB`, and `stdio_uart_config_ra6e2_fpb.c` guarded by `BOARD_RA6E2_FPB`) |
| `src/rs_rtc_logger/rs_rtc_logger_stdio_console.c/.h` | New — thin adapter, ships inside the core folder | Bridges the RTC Event Logger to the vendored driver's stdio surface: forces deterministic init + validation (`rs_rtc_logger_console_init()`, called by `rs_rtc_logger_hal_uart_open()` — a genuine **core dependency**, not just demo glue), plus non-blocking character read (`rs_rtc_logger_console_getc_nb()`) and string write (`rs_rtc_logger_console_puts()`) used only by the reference CLI |

See [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) for the full reuse/modify guidance on these files, and [Configuration Guide](#configuration-guide) and [Integration Guide](#integration-guide) for the updated hardware and initialization details.

#### 2.2 Per-Board Differences

Both boards use the identical core module, the identical `rs_rtc_logger_stdio_console.c/h` adapter, and the identical vendored driver files (`r_byteq.c/h`, `rs_stdio_uart.c/h`, `syscalls_picolib_stdio_to_uart.c`). Only the following are per-board:

| Item | FPB-RA4E2 | FPB-RA6E2 |
|---|---|---|
| Board config file | `stdio_uart_config_ra4e2_fpb.c` (guard: `BOARD_RA4E2_FPB`) | `stdio_uart_config_ra6e2_fpb.c` (guard: `BOARD_RA6E2_FPB`) |
| RTC preset config | `rs_rtc_logger_data_fpb_ra4e2.c/.h` | `rs_rtc_logger_data_fpb_ra6e2.c/.h` |
| UART baud register (`brr`) | 53 | 26 |
| PCLKA (validated at `Open()`-adjacent config check) | 100 MHz | 50 MHz |
| HOCO (both boards) | 20 MHz | 20 MHz |
| UART pins (both boards) | P410 (RX) / P411 (TX) | P410 (RX) / P411 (TX) |
| RTC `carry_ipl` (both boards) | 2 | 2 |

The differing `brr`/PCLKA values reflect each board's own PLL configuration in this project (RA6E2: HOCO ×10 ÷1 = 200 MHz, PCLKA ÷4 = 50 MHz) — not a difference in the driver or the Buffered I/O design itself. See [Configuration Guide](#configuration-guide) for the full per-board hardware table.

---

### 3. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

---

### 4. What Typically Changes Between Releases

- Core source files (`rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`) may receive reliability improvements, new features, or FSP version compatibility updates.
- Public headers (`rs_rtc_logger.h`, `rs_rtc_logger_types.h`) may add new public functions or extend existing structures. Additions are generally straightforward to adopt; removals or signature changes — are not always accompanied by a version-number bump, so always diff the public headers rather than relying on `RS_RTC_LOGGER_VERSION` alone.
- `rs_rtc_logger_cfg.h` constants may gain new `#ifndef`-guarded defaults. Your existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases or corrected for hardware errata, or to change which peripherals the module owns directly.

---

### 5. Recommended Merge Process

1. Diff the updated core files against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications.
3. Review the updated `rs_rtc_logger.h` and `rs_rtc_logger_types.h` for interface changes; update call sites in your application accordingly.
4. Rebuild and retest on target hardware.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading. If the FSP version changes between your baseline and the update, rerun the FSP Stack Configurator to regenerate `ra_gen/` content for the new FSP version, or re-run the Meson `rasc_generate` target.

---

*End of User Manual — `rs_rtc_logger` v1.0.0*
