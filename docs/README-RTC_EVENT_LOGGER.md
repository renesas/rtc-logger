# RTC Internal Timestamp Event Logger – User Manual

**Short ID:** `rs_rtc_logger`  
**Version:** 1.0.0  
**FSP Baseline:** FSP v6.5.0  
**Primary Target:** FPB-RA4E2  
**Secondary Target:** FPB-RA6E2  
**License:** BSD-3-Clause  

---

## Table of Contents

1. [What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate)
2. [What Does the Core Code Do?](#2-what-does-the-core-code-do)
3. [Core Code vs. Reference Application Code](#3-core-code-vs-reference-application-code)
4. [Public Functions — Where to Start](#4-public-functions--where-to-start)
5. [Configuration Assumptions for the Reference Board](#5-configuration-assumptions-for-the-reference-board)
6. [Adapting to Your Board](#6-adapting-to-your-board)
7. [Dependencies](#7-dependencies)
8. [Demonstrated Behavior and Performance](#8-demonstrated-behavior-and-performance)
9. [File Guide — Naming Conventions and What Is Safe to Modify](#9-file-guide--naming-conventions-and-what-is-safe-to-modify)
10. [Adopting a Newer Release](#10-adopting-a-newer-release)
11. [Design Flow and Architecture](#11-design-flow-and-architecture)
12. [Running the Project and Expected Output](#12-running-the-project-and-expected-output)
13. [Reusability and Adaptation for Similar Use Cases](#13-reusability-and-adaptation-for-similar-use-cases)

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

The module encapsulates the integration of two FSP HAL drivers — RTC (`r_rtc` / `rtc_instance_t`) and UART (`r_sci_uart` / `uart_instance_t`) — into a single reusable module. The External IRQ driver (`r_icu`) is owned and managed by the application layer; any interrupt source calls `rs_rtc_logger_EventTrigger()` to trigger a timestamped log entry. Developers do not need to manually orchestrate RTC reads, timestamp formatting, string composition, and UART transmission in every project.

---

## 2. What Does the Core Code Do?

The core module provides the following capabilities:

**RTC Timestamp Capture**  
On every `rs_rtc_logger_EventTrigger()` call, the module immediately reads the current calendar time from the MCU's on-chip RTC via `R_RTC_CalendarTimeGet()`. The timestamp is captured at ISR entry time — before any queuing or formatting occurs — so the logged time reflects the exact moment of the event. The `rtc_time_t` struct fields are converted to display format using integer arithmetic only (no floating-point).

**Event Buffering via Circular Buffer**  
Captured events (timestamp + sequence count + payload) are stored in a static circular buffer inside `rs_rtc_logger_ctrl_t`. The buffer decouples ISR-time capture from UART transmit time. `EventTrigger()` returns immediately after enqueuing; it never blocks the calling ISR context. Buffer depth defaults to 8 entries and is configurable at build time.

**Asynchronous UART Transmission**  
A FreeRTOS consumer task (module-owned in Example Project mode, application-owned in Expert mode) drains the circular buffer and calls `rs_rtc_logger_Write()` to format and transmit each entry over UART. All UART writes are non-blocking: the write is initiated and the consumer task waits on a task notification from the UART TX-complete callback before proceeding to the next entry.

**Formatted Output**  
The default output format is:

```
[YYYY-MM-DD HH:MM:SS] Event Detected #<count>
```

For example: `[2024-07-15 14:30:05] Event Detected #3`

Additional formats are available: time-only `[HH:MM:SS]`, compact `[YYYYMMDD-HHMMSS]`, and syslog-style `[Mon DD HH:MM:SS]`. Format is selectable at build time or changeable at runtime via `rs_rtc_logger_TimestampFormatSet()`.

**Warm Reset Preservation**  
On a warm reset (any reset other than power-on reset), the RTC continues running and retains its calendar configuration. The module relies on the FSP `r_rtc` build-time property *Set Source Clock in Open* being set to `Disabled`, so `R_RTC_ClockSourceSet()` is called only when a power-on reset is detected. The event sequence counter is preserved by the reference application using a `BSP_SECTION_NOINIT` magic-sentinel pattern and restored via `rs_rtc_logger_SeqCountSet()` after `Open()`.

**LOCO Clock Source**  
The RTC is clocked from the LOCO (Low-speed On-Chip Oscillator), which is the FSP default for the RA4E2. LOCO is less accurate than a 32.768 kHz sub-clock crystal (which is fitted on the FPB-RA4E2 as Y2 but is not used by default). For applications requiring higher time accuracy, the sub-clock alternative is documented in Section 13. A runtime LOCO drift calibration function `rs_rtc_logger_LocoCalibrationSet()` is provided to adjust the RFRL register — approximately 3906 ppm per RFRL unit, which is roughly 3.5 seconds per 15 minutes per unit.

**Power-Down Retention Limitation (FPB-RA4E2)**  
The RA4E2 in its 64-pin QFN package (R7FA4E2B93CFM) does not expose a VBATT pin, and the FPB-RA4E2 board has no battery connector or VBATT supply circuit. RTC time is therefore **not preserved on power-down** on this board. RTC state is preserved across warm resets while USB power is maintained. Battery-backed power-down retention requires a target board with VBATT supply; the FPB-RA6E2 VBATT support status should be confirmed against its schematic.

---

## 2.1 Dual Operating Modes

The module supports two operating modes selected at compile time via `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` in `rs_rtc_logger_cfg.h`. Both modes share the same core source files.

| Aspect | Example Project Mode (`=1`, default) | Expert Mode (`=0`) |
|---|---|---|
| Purpose | Onboarding, demos, evaluation | Production use requiring precise control |
| Application entry point | `rs_rtc_logger_EventTrigger()` only | Full lifecycle: `rs_rtc_logger_Open()`, `rs_rtc_logger_Start()`, `rs_rtc_logger_Stop()`, `rs_rtc_logger_Close()`, `rs_rtc_logger_EventTrigger()`, `rs_rtc_logger_Write()` |
| Lifecycle management | Auto-managed — triggered on first `rs_rtc_logger_EventTrigger()` call | Application owns all lifecycle transitions |
| Consumer task | Module-owned internal static FreeRTOS task | Application-owned consumer task |
| `rs_rtc_logger_Write()` caller | Called internally by module consumer task | Application consumer task calls explicitly |
| Firmware-update pattern | Not applicable | `rs_rtc_logger_Stop()` before write, `rs_rtc_logger_Start()` after verify |

**Example Project Mode** is intended for new users evaluating the module. The application need only call `rs_rtc_logger_EP_PrepareConsumerTask()` once from task context before enabling the external IRQ, then call `rs_rtc_logger_EventTrigger()` from the ISR. All other behavior — opening HAL drivers, starting the UART output path, draining the circular buffer, and transmitting over VCOM — is managed internally.

**Expert Mode** gives the application full control over the module lifecycle and is appropriate for production firmware where the application needs to sequence initialization precisely, integrate with firmware update workflows (Stop/Start pattern), or manage RTOS task priorities explicitly.

---

## 3. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference or demo application layer. The configuration path column refers to two options explained fully in [Section 6](#6-adapting-to-your-board): the **Preset path** (default, uses pre-built HAL instances that work out of the box) and the **FSP Stack Configurator path** (for custom boards or modified hardware settings).

| Layer | Files | Purpose | Reuse Expectation |
|---|---|---|---|
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; circular buffer management; `EventTrigger()` ISR path; Example Project auto-lifecycle and internal consumer task | Copy into your project; core logic |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.h` | Public interface header — all public function prototypes | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_types.h` | Public type definitions and enumerations | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_cfg.h` | Build-time configuration constants with `#ifndef` guards | Override individual constants as needed for your project |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_format.c/.h` | Pure timestamp formatting — converts `rtc_time_t` to ASCII string; no I/O; integer arithmetic only | Copy into your project; no board-specific content |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_hal.c/.h` | Thin HAL shim: UART TX-complete callback registered with UART HAL; sets completion flag and signals FreeRTOS consumer task | Copy into your project; board-agnostic |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_config_fpb_ra4e2.c/.h` | Pre-built HAL instance definitions for FPB-RA4E2; aliased to FSP-generated instances where appropriate | Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see Section 6) |
| **Reference App** | `src/rs_rtc_logger_demo.c` | Full demonstration of both modes; CLI task, consumer task (Expert mode), button ISR, LOCO calibration | Adapt or replace entirely for your application |
| **Reference App** | `src/blinky_thread_entry.c` | FreeRTOS thread entry; banner output; r_icu External IRQ open/enable; LED blink | Adapt for your board; not reusable as-is |
| **Reference App** | `src/hal_warmstart.c` | Warm-start detection helper used by demo | Adapt for your application |
| **Reference App** | `src/fault_handler.c` | Fault handler for debug | Optional; adapt or replace |
| **FSP Generated** | `ra/` directory | FSP-generated configuration, drivers, middleware | Do not edit manually; regenerate via e2studio FSP tooling |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it.

---

## 4. Public Functions — Where to Start

All public functions use the naming pattern `rs_rtc_logger_OperationName()` — a lowercase `rs_rtc_logger_` prefix followed by an UpperCamelCase operation name. All functions return `fsp_err_t` and take a pointer to the control block as their first argument. Build-time configuration macros and enumeration values use the all-uppercase `RS_RTC_LOGGER_` form. See Section 9 for a complete naming conventions reference.

> **What "hot-safe" means:** A function marked hot-safe can be called while the module is in the STARTED state (actively transmitting) without disrupting the current transmission. The change takes effect on the next `Write()` call. Functions that are not hot-safe must only be called before the module is started or after it is stopped.

### Start Here: The Entry Point

```c
// ISR-safe — the only function the application ISR needs to call:
rs_rtc_logger_EventTrigger(&g_logger_ctrl, (uint8_t const *)"Event Detected", 14U);
```

In **Example Project mode**, this is the only function the application needs to call after initial setup. In **Expert mode**, the application also manages the lifecycle functions described below.

### Configuration Bridge (call before Open)

```c
// Bind HAL instances — call once before rs_rtc_logger_Open()
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
```

`rs_rtc_logger_ConfigGen()` connects the RTC and UART hardware instances to the module configuration. This must be called once during initialization before any other module function. See [Section 6](#6-adapting-to-your-board) for details on the preset vs. FSP Stack Configurator instance options.

### Lifecycle Functions (Expert Mode / also used internally in Example Project Mode)

| Function | Valid States | Description |
|---|---|---|
| `rs_rtc_logger_Open(p_ctrl, p_cfg)` | CLOSED | Initialize; open RTC + UART HAL drivers in order |
| `rs_rtc_logger_Start(p_ctrl)` | OPEN, STOPPED | Enable UART output path; activate consumer task |
| `rs_rtc_logger_Stop(p_ctrl)` | STARTED | Suspend TX; buffer continues accepting events |
| `rs_rtc_logger_Close(p_ctrl)` | OPEN, STOPPED | Release all resources; close UART then RTC |

### Operational Functions

| Function | Valid States | Description |
|---|---|---|
| `rs_rtc_logger_EventTrigger(p_ctrl, p_data, length)` | STARTED, STOPPED (Example Project: also CLOSED on first call) | ISR-safe capture; enqueues to circular buffer |
| `rs_rtc_logger_Write(p_ctrl, p_entry)` | STARTED | Format and transmit one buffer entry (called internally in Example Project mode) |
| `rs_rtc_logger_WriteStr(p_ctrl, p_str)` | STARTED | Convenience: write a string with live RTC timestamp, bypassing the circular buffer |

### Configuration and Query Functions

| Function | Hot-Safe | Description |
|---|---|---|
| `rs_rtc_logger_TimestampFormatSet(p_ctrl, format)` | Yes | Switch timestamp format at runtime |
| `rs_rtc_logger_TimeSet(p_ctrl, p_time)` | Yes | Set RTC calendar date and time |
| `rs_rtc_logger_ClockValidate(p_ctrl, time_is_valid)` | Yes | Mark RTC time as valid or invalid (used on warm reset) |
| `rs_rtc_logger_LocoCalibrationSet(p_ctrl, rfrl_value)` | Yes | Adjust LOCO oscillator drift via RFRL register (valid range 8–510) |
| `rs_rtc_logger_SeqCountSet(p_ctrl, seq_count)` | No | Restore sequence counter after warm reset (OPEN or STOPPED only) |
| `rs_rtc_logger_StatusGet(p_ctrl, p_status)` | — | Snapshot of state, counters, and last timestamp |
| `rs_rtc_logger_TimeGet(p_ctrl, p_time)` | — | Read current RTC calendar time |
| `rs_rtc_logger_VersionGet(p_version)` | — | Module version (valid in any state including CLOSED) |

### Example Project Mode Setup Helper

```c
// Call from task context BEFORE enabling the external IRQ:
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);
```

This creates the internal static FreeRTOS consumer task. It must be called from task context (not ISR context) because `xTaskCreateStatic()` must not be called from within an ISR.

---

## 5. Configuration Assumptions for the Reference Board

The reference implementation targets the **FPB-RA4E2** with the following board-specific assumptions. These are reference assumptions for the demo hardware, not requirements for your application.

| Resource | Reference Assumption | Notes |
|---|---|---|
| MCU | RA4E2 (Cortex-M33, 64-pin QFN, R7FA4E2B93CFM) | Primary development target |
| UART channel | SCI0 (USB-Serial VCOM bridge via J-Link) | Schematic page 7: P410(RX)/P411(TX) |
| UART baud rate | 115200 baud, async, 8N1 | VCOM USB-Serial bridge default |
| RTC clock source | LOCO (Low-speed On-Chip Oscillator) | FSP default; see LOCO accuracy note in Section 8 |
| Sub-clock crystal | Y2 (32.768 kHz) populated but not used by default | Available as a higher-accuracy alternative |
| External IRQ | P304 / IRQ9 / User Switch S1 | Schematic page 2; application-owned |
| Power supply | USB 5V → 3.3V linear regulator (no VBATT) | No battery backup; see power-down limitation in Section 2 |
| FreeRTOS heap | heap_4 | Required for `xTaskCreateStatic()` |
| FreeRTOS static allocation | `configSUPPORT_STATIC_ALLOCATION = 1` | Required for static task creation |

### Required Non-Default FSP Settings

The following FSP Stack Configurator settings must be configured correctly for the module to operate. These are values that differ from FSP defaults and must be set in `configuration.xml` when not using the preset configuration path (see Section 6).

| Driver | Property | Required Value | Reason |
|---|---|---|---|
| `r_rtc` | Clock source | LOCO | Specifies the LOCO oscillator as the RTC clock source |
| `r_rtc` | Calendar mode | Enabled | Required for `R_RTC_CalendarTimeGet()` and `R_RTC_CalendarTimeSet()` |
| `r_rtc` | Set Source Clock in Open | **Disabled** | Required for warm-reset RTC preservation — prevents `R_RTC_ClockSourceSet()` from being called on every open, which would reset the running clock |
| `r_rtc` | RTC interrupt priority | Non-zero (≥ 2) | Required; must be lower than the FreeRTOS max syscall priority |
| `r_sci_uart` | TX callback | `rs_rtc_logger_uart_callback` | Module-owned callback; must not be changed |
| `r_sci_uart` | Operating mode | Asynchronous | Required for non-blocking UART write |
| `r_sci_uart` | Baud rate | 115200 | Matches the VCOM USB-Serial bridge default on FPB boards |
| `r_icu` | IRQ channel | IRQ9 (P304) | On-board user button S1; configured and owned by the application |
| FreeRTOS | Heap scheme | heap_4 (or heap_3) | Required for `xTaskCreateStatic()` |
| FreeRTOS | `configSUPPORT_STATIC_ALLOCATION` | 1 | Required for static task creation (no heap used for task stack or control block) |

### `rtc_time_t` Field Conventions

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

## 6. Adapting to Your Board

### Configuration Path Options

The module supports two ways to supply hardware configuration, selected by `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` in `rs_rtc_logger_cfg.h`.

**Preset Path (default, `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 1`):**  
Pre-built HAL instance definitions for the FPB-RA4E2 are provided in `src/rs_rtc_logger/config/rs_rtc_logger_config_fpb_ra4e2.c/.h`. The project builds and runs without opening the FSP Stack Configurator. This path is recommended for initial evaluation on the FPB-RA4E2.

```c
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
#include "rs_rtc_logger/config/rs_rtc_logger_config_fpb_ra4e2.h"
#endif

rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
```

**FSP Stack Configurator Path (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`):**  
Configure `r_rtc` and `r_sci_uart` stacks in the e2studio FSP Stack Configurator for your target board. FSP tooling generates `g_rtc0` and `g_uart0` symbols in `ra_gen/`. Pass these to `rs_rtc_logger_ConfigGen()` instead of the preset instances.

```c
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0, &g_uart0);
```

Use this path when targeting a board other than the FPB-RA4E2, or when you need to customize the UART channel, baud rate, RTC clock source, or any other FSP-configurable property.

### Key Adaptation Points

| Adaptation | Location | What to Change |
|---|---|---|
| Target board / UART channel | `src/rs_rtc_logger/config/` | Create a new preset config file for your board, or switch to the FSP Stack Configurator path |
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

### FPB-RA6E2 (Secondary Target)

The FPB-RA6E2 uses the same UART driver (`r_sci_b_uart`) and the same core source files without modification. Board-specific differences to address:

- The RA6E2 includes TrustZone. The module targets the non-secure partition by default. The reference application must use the BSP TrustZone template configured as a non-secure project.
- Confirm the equivalent user button pin and IRQ channel against the FPB-RA6E2 schematic and update the application-owned `r_icu` configuration accordingly.
- Confirm VBATT schematic support on FPB-RA6E2 if power-down RTC retention is required.

---

## 7. Dependencies

### Hardware Dependencies

| Resource | Usage | Owned By |
|---|---|---|
| On-chip RTC (`r_rtc`) | Calendar time read/set; clock source initialization | This module |
| UART — SCI0 (`r_sci_uart`) | VCOM serial output at 115200 baud | This module |
| External IRQ (`r_icu`) — P304/IRQ9 | User button S1 interrupt detection | **Application** |
| USB-Serial bridge (J-Link virtual COM) | VCOM physical layer | Board hardware |

### Software Dependencies

| Component | Version | Notes |
|---|---|---|
| Renesas FSP | v6.5.0 | Provides `r_rtc`, `r_sci_uart`, `r_icu`, BSP, startup |
| FreeRTOS | Bundled with FSP v6.5.0 | Required for time-critical event synchronization |
| e2studio IDE | Compatible with FSP v6.5.0 | Required to build and flash the project |
| J-Link On-Board | — | Used for debug/flash on FPB boards |

### Driver Dependency Details

The module references both HAL drivers exclusively through FSP abstract interface types (`rtc_instance_t *` and `uart_instance_t *`). This means the core source files are identical across both target boards — board-specific differences are confined to `configuration.xml` and the preset config files.

The External IRQ driver (`r_icu`) is **not** initialized, enabled, disabled, or closed by this module. The module has no dependency on any specific interrupt source. When the `r_icu` ISR fires, the application calls `rs_rtc_logger_EventTrigger()` to pass the event to the module core.

### No External Module Dependencies

This module does not depend on any other reference software module. If you need similar RTC or UART functionality elsewhere in your application, include it directly rather than creating cross-module dependencies.

---

## 8. Demonstrated Behavior and Performance

### Functional Behavior Demonstrated

- RTC calendar time set by user via VCOM CLI in `[YYYY-MM-DD HH:MM:SS]` format on power-on reset
- Warm reset detection via `R_SYSTEM->RSTSR0_b.PORF`; RTC configuration preserved; sequence counter restored from `BSP_SECTION_NOINIT` backup
- External interrupt from user button S1 (P304/IRQ9) triggers `rs_rtc_logger_EventTrigger()` from ISR context
- Timestamped log line transmitted over VCOM: `[YYYY-MM-DD HH:MM:SS] Event Detected #<n>`
- LOCO drift calibration via RFRL register adjustment at startup
- Events logged before the time is set display as `[0000-00-00 00:00:00]` with a per-event re-prompt for clock setting

### Performance Characteristics

**`rs_rtc_logger_EventTrigger()` ISR overhead:**  
The ISR path performs: one `R_RTC_CalendarTimeGet()` HAL call, one atomic `seq_count` increment, one inline copy of the event payload (bounded by `RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN`, default 32 bytes) into the circular buffer, and one `xTaskNotifyFromISR()` call. No UART activity, no formatting, and no heap allocation occur in the ISR path. Execution time is deterministic and bounded.

**UART TX latency:**  
Asynchronous and non-blocking. The consumer task initiates a non-blocking UART write and waits on a task notification from the UART TX-complete callback. Latency is task-scheduler-dependent; sub-millisecond at typical FreeRTOS tick rates at 115200 baud.

**RAM consumption (default configuration):**  
- Circular buffer: 8 entries × ~68 bytes per entry (`rtc_time_t` + seq_count + 32-byte payload + length + time_valid flag) ≈ 544 bytes  
- Line buffer: 256 bytes (inside `rs_rtc_logger_ctrl_t`)  
- Total control block: verified at build time via `static_assert` against `RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES` (default 1024 bytes)  
- Internal consumer task (Example Project mode): stack 2048 bytes + TCB ~88 bytes on Cortex-M33

**No dynamic memory allocation.** All buffers are statically allocated or stack-resident within the control block. No `malloc`/`free` calls are present in any code path.

**No floating-point.** Timestamp formatting uses integer arithmetic only throughout.

### Clock Accuracy Note

LOCO provides lower accuracy than the sub-clock (SOSC) crystal oscillator. For the RA4E2, LOCO typically drifts in the range of several hundred to several thousand ppm, which is approximately 0.5–5 minutes per day. For applications requiring higher time accuracy, the sub-clock with the external 32.768 kHz crystal (Y2 on FPB-RA4E2) is recommended. Runtime drift compensation is available via `rs_rtc_logger_LocoCalibrationSet()`, which adjusts the RFRL register by approximately 3906 ppm per unit.

---

## 9. File Guide — Naming Conventions and What Is Safe to Modify

### Naming Conventions Used in This Module

All identifiers follow a consistent scheme using the `rs_` / `RS_` prefix, which identifies Renesas reference software and avoids conflicts with FSP middleware names (`rm_` / `RM_`). The short name for this module is `rtc_logger`.

| Category | Pattern | Examples |
|---|---|---|
| **Public functions** | `rs_rtc_logger_OperationName()` — lowercase prefix, UpperCamelCase operation | `rs_rtc_logger_EventTrigger()`, `rs_rtc_logger_TimeSet()`, `rs_rtc_logger_StatusGet()` |
| **Internal helper functions** | `rs_rtc_logger_*` or abbreviated `rs_rl_*` — all lowercase | `rs_rtc_logger_uart_callback()`, `rs_rl_enqueue()`, `rs_rl_dequeue()` |
| **Build-time macros and config symbols** | `RS_RTC_LOGGER_CFG_*` — all uppercase | `RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH`, `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` |
| **Version macros** | `RS_RTC_LOGGER_VERSION*` — all uppercase | `RS_RTC_LOGGER_VERSION_MAJOR`, `RS_RTC_LOGGER_VERSION` |
| **Enumeration type names** | `rs_rtc_logger_*_t` — lowercase with `_t` suffix | `rs_rtc_logger_state_t`, `rs_rtc_logger_ts_format_t` |
| **Enumeration values** | `RS_RTC_LOGGER_GROUP_VALUE` — all uppercase | `RS_RTC_LOGGER_STATE_STARTED`, `RS_RTC_LOGGER_TS_FORMAT_ISO8601`, `RS_RTC_LOGGER_TERM_CRLF` |
| **Struct type names** | `rs_rtc_logger_*_t` — lowercase with `_t` suffix | `rs_rtc_logger_cfg_t`, `rs_rtc_logger_ctrl_t`, `rs_rtc_logger_status_t` |
| **Source and header files** | `rs_rtc_logger_*.c / .h` — lowercase | `rs_rtc_logger.c`, `rs_rtc_logger_cfg.h`, `rs_rtc_logger_types.h` |
| **Core source folder** | `src/rs_rtc_logger/` | All core module files live here |
| **Board config files** | `rs_rtc_logger_config_<board>.c/.h` | `rs_rtc_logger_config_fpb_ra4e2.c/.h` |
| **Project name** | `UseCaseName_<board>` — descriptive, not prefixed with `rs_` | `RTC_Event_Logger_FPB_RA4E2` |

The key rule to remember: **public functions use lowercase `rs_rtc_logger_` prefix** with an UpperCamelCase operation name (e.g. `rs_rtc_logger_EventTrigger`). Everything that is a macro, constant, or enumeration value uses the **all-uppercase `RS_RTC_LOGGER_`** form. Type names and internal helpers are always all-lowercase.

---

### Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_rtc_logger.c` | With care | Core logic; add capability at defined extension points only |
| `rs_rtc_logger.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_rtc_logger_types.h` | No | Public types; changing structures may break binary compatibility |
| `rs_rtc_logger_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_rtc_logger_format.c/.h` | With care | Pure formatting; portable; can be unit-tested in isolation |
| `rs_rtc_logger_hal.c/.h` | With care | Thin HAL shim; modify only if switching UART driver variant |

### Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for the FPB-RA4E2 and are marked "do not edit." They are reference-quality reproductions of FSP-generated output for the specified board. To customize the UART or RTC configuration for a different board, switch to the FSP Stack Configurator path as described in Section 6 (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`).

| File | Notes |
|---|---|
| `config/rs_rtc_logger_config_fpb_ra4e2.c` | Preset HAL instance definitions for FPB-RA4E2; do not edit |
| `config/rs_rtc_logger_config_fpb_ra4e2.h` | Preset instance declarations; do not edit |

### Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_rtc_logger_demo.c` | Starting point for your CLI task and ISR callback; replace or heavily adapt |
| `blinky_thread_entry.c` | FreeRTOS thread entry; adapt for your board's LED/button layout and initialization order |
| `hal_warmstart.c` | Warm-start detection; adapt if your system uses a different reset detection mechanism |
| `fault_handler.c` | Debug fault handler; replace with your production fault handling |

### FSP-Generated Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` | FSP-generated drivers, middleware, CMSIS, and board support; regenerate via e2studio FSP tooling when changing `configuration.xml` |
| `ra_gen/` (inside `ra/`) | Auto-generated by FSP Stack Configurator; overwritten on every re-generation |

---

## 10. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

**What typically changes between releases:**
- Core source files (`rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`) may receive bug fixes, new features, or FSP version compatibility updates.
- Public headers (`rs_rtc_logger.h`, `rs_rtc_logger_types.h`) may add new public functions or extend existing structures. Additions are generally backward-compatible; removals or signature changes are not.
- `rs_rtc_logger_cfg.h` constants may gain new `#ifndef`-guarded defaults. Your existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases or corrected for hardware errata.

**Recommended merge process:**
1. Diff the updated core files against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications.
3. Review the updated `rs_rtc_logger.h` and `rs_rtc_logger_types.h` for interface changes; update call sites in your application accordingly.
4. Rebuild and retest on target hardware.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading. If the FSP version changes between your baseline and the update, rerun the FSP Stack Configurator to regenerate `ra_gen/` content for the new FSP version.

---

## 11. Design Flow and Architecture

### Lifecycle State Machine

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

### Public Function State Gating

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

¹ EP only = Example Project mode only: `rs_rtc_logger_EventTrigger()` in CLOSED state enqueues the event immediately (no event loss) and triggers a deferred auto-`rs_rtc_logger_Open()`/`rs_rtc_logger_Start()` sequence. Expert mode returns `FSP_ERR_NOT_OPEN` in CLOSED state.

*hot = hot-safe: callable while STARTED; takes effect on the next `rs_rtc_logger_Write()` call without interrupting the current transmission. See the definition of "hot-safe" at the top of Section 4.*

### Event Capture and Telemetry Flow

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
                    ├── uart->write(line_buf)   ← non-blocking
                    └── wait for TX_COMPLETE callback
                            │
                            └── rs_rtc_logger_uart_callback() [UART ISR]
                                    ├── tx_complete = true
                                    └── xTaskNotifyFromISR()
```

### Module File Responsibilities

| Module | Responsibility |
|---|---|
| `rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; state gating; `rs_rtc_logger_EventTrigger()` ISR path; circular buffer management; diagnostic counters; Example Project mode auto-lifecycle logic and internal consumer task (compiled in only when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` is enabled) |
| `rs_rtc_logger_format.c` | Pure timestamp formatting — converts `rtc_time_t` plus format selection into an ASCII string in a caller-supplied buffer. No I/O. No side effects. Integer arithmetic only. Fully unit-testable in isolation. |
| `rs_rtc_logger_hal.c` | Thin HAL shim: UART TX-complete callback registered with UART HAL; sets completion flag; notifies consumer task via `xTaskNotifyFromISR()` |

---

## 12. Running the Project and Expected Output

### Prerequisites

- FPB-RA4E2 board connected via USB (provides both power and J-Link virtual COM)
- e2studio IDE with FSP v6.5.0 installed
- A serial terminal connected to the J-Link virtual COM port at **115200 baud, 8N1**

### Importing and Building

1. Import the project into e2studio using **File → Import → Existing Projects into Workspace** and select the `RTC_Event_Logger_FPB_RA4E2` project directory.
2. The project is fully functional upon import. The `configuration.xml` is pre-configured; no FSP Stack Configurator re-run is required for the default preset path.
3. Build the project (**Project → Build All**). The default build target is **Example Project mode** (`RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1`).
4. Flash and debug using **Run → Debug As → Renesas GDB Hardware Debugging**.

To switch to **Expert mode**, set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` to `0` in `rs_rtc_logger_cfg.h` (or as a project-level compiler define `-DRS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0`) and rebuild.

### Expected Output — Example Project Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Example Project
Creating RTC Event Logger consumer task...
Consumer task ready.
Triggering RTC Event Logger auto-init...
Waiting for RTC Event Logger auto-init to complete...
[0000-00-00 00:00:00] INIT #1
RTC Event Logger auto-init complete.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2024-07-15 14:30:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: +8
LOCO calibration set: trim +8, RFRL = 263.
Ready. Press S1 to log an event.
[2024-07-15 14:30:05] Event Detected #2
[2024-07-15 14:30:07] Event Detected #3
```

### Expected Output — Expert Mode (cold start)

```
*** RTC Event Logger Demo ***
Mode: Expert
Opening RTC Event Logger...
RTC Event Logger open OK.
Power-on reset detected.
Enter date/time [YYYY-MM-DD HH:MM:SS] : 2024-07-15 14:30:00
Time set OK.
LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: +8
LOCO calibration set: trim +8, RFRL = 263.
Starting RTC Event Logger...
RTC Event Logger started.
Ready. Press S1 to log an event.
[2024-07-15 14:30:05] Event Detected #1
```

### Expected Output — Warm Reset (both modes)

After pressing the reset button while USB power remains:

```
*** RTC Event Logger Demo ***
Mode: Example Project
...
Warm reset — RTC time retained.
Ready. Press S1 to log an event.
[2024-07-15 14:31:42] Event Detected #4
```

The sequence counter resumes from its pre-reset value and the RTC continues from its running time with no re-prompting for date/time.

### Interaction Notes

- **Date/time prompt:** Enter in `YYYY-MM-DD HH:MM:SS` format. Press Enter with no input to skip; events will display `[0000-00-00 00:00:00]` and you will be re-prompted after each event.
- **LOCO trim prompt:** Enter a signed integer (e.g. `+17`, `-8`, `0`) to adjust the RFRL register. Press Enter with no input to skip and use the default RFRL = 255.
- **S1 button:** Press User Switch S1 on the FPB-RA4E2 board to trigger an event.

---

## 13. Reusability and Adaptation for Similar Use Cases

### Core Function Reusability

The `rs_rtc_logger_EventTrigger()` function is the universal entry point and is by design **agnostic to the source of the external interrupt**. The core module has no knowledge of what generated the call. Any interrupt source — `r_icu` GPIO, accelerometer callback, timer expiry, UART RX, or a direct software call — can trigger a timestamped log entry by calling this one function. No changes to the core source are required when switching interrupt sources.

### Portions Applicable Without Modification

- All files in `src/rs_rtc_logger/` can be included directly in any FSP v6.5.0 FreeRTOS project targeting the RA4E2 or RA6E2 without modification, provided the configuration is supplied via `rs_rtc_logger_ConfigGen()`.
- The timestamp formatter (`rs_rtc_logger_format.c`) is a pure function with no I/O or side effects and can be extracted for use in unit tests or other logging implementations.
- The circular buffer and ISR-safe enqueue logic inside `rs_rtc_logger.c` represents a portable, heap-free, single-producer/single-consumer event buffer pattern reusable in similar telemetry designs.

### Portions Requiring Modification

- **Board-specific preset config files** (`config/rs_rtc_logger_config_fpb_<board>.c/.h`): Create new preset files for other target boards, or switch to the FSP Stack Configurator path (see Section 6). The UART channel, baud rate, and pin configuration are board-specific.
- **Reference application** (`rs_rtc_logger_demo.c`, `blinky_thread_entry.c`): Replace the CLI task, consumer task structure, and ISR callback with application-specific code. The demo application is demonstration scaffolding, not production code.
- **RTC clock source**: Replace LOCO with the sub-clock (SOSC) for applications requiring higher time accuracy. Change the `r_rtc` clock source property in `configuration.xml` and remove or adapt the LOCO calibration prompt.

### Example: Adapting for an Accelerometer Event Source

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

No changes to the core module source are required. The module is entirely agnostic to the interrupt source.

### Example: Expert Mode Firmware Update Pattern

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

The `seq_count` and circular buffer are preserved across `rs_rtc_logger_Stop()`/`rs_rtc_logger_Start()`. Events that arrived during the STOPPED window are transmitted in order once STARTED.

---