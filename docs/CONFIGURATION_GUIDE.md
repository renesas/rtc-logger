# Configuration Guide

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Configuration Overview](#1-configuration-overview)
2. [Configuration Assumptions for the Reference Board](#2-configuration-assumptions-for-the-reference-board)
3. [Required Non-Default FSP Settings](#3-required-non-default-fsp-settings)
4. [`rtc_time_t` Field Conventions](#4-rtc_time_t-field-conventions)
5. [Adapting to Your Board](#5-adapting-to-your-board)
6. [Debounce Configuration (Reference Application)](#6-debounce-configuration-reference-application)
7. [Dependencies](#7-dependencies)

---

## 1. Configuration Overview

The `rs_rtc_logger` PES relies on a combination of:

- FSP-generated or PES preset RTC/UART instance configuration
- Build-time configuration constants in `rs_rtc_logger_cfg.h`
- A runtime configuration structure (`rs_rtc_logger_cfg_t`) bound via `rs_rtc_logger_ConfigGen()`

The reference implementation targets the **FPB-RA4E2** (primary) and **FPB-RA6E2** (secondary).

---

## 2. Configuration Assumptions for the Reference Board

The reference implementation targets the **FPB-RA4E2** with the following board-specific assumptions. These are reference assumptions for the demo hardware, not requirements for your application.

| Resource | Reference Assumption | Notes |
|---|---|---|
| MCU | RA4E2 (Cortex-M33, 64-pin QFN, R7FA4E2B93CFM) | Primary development target |
| UART channel | SCI0 (USB-Serial VCOM bridge via J-Link) | Schematic page 7: P410(RX)/P411(TX) |
| UART baud rate | 115200 baud, async, 8N1 | VCOM USB-Serial bridge default |
| RTC clock source | LOCO (Low-speed On-Chip Oscillator) | FSP default; see clock accuracy note in [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md) |
| Sub-clock crystal | Y2 (32.768 kHz) populated but not used by default | Available as a higher-accuracy alternative |
| External IRQ | P304 / IRQ9 / User Switch S1 | Schematic page 2; application-owned |
| Power supply | USB 5V → 3.3V linear regulator (no VBATT) | No battery backup; see power-down limitation in [RTC_EVENT_LOGGER-USER_MANUAL.md](RTC_EVENT_LOGGER-USER_MANUAL.md) |
| FreeRTOS heap | heap_4 | Required for `xTaskCreateStatic()` |
| FreeRTOS static allocation | `configSUPPORT_STATIC_ALLOCATION = 1` | Required for static task creation |

---

## 3. Required Non-Default FSP Settings

The following FSP Stack Configurator settings must be configured correctly for the module to operate. These are values that differ from FSP defaults and must be set in `configuration.xml` when not using the preset configuration path (see [Section 5](#5-adapting-to-your-board)).

| Driver | Property | Required Value | Reason |
|---|---|---|---|
| `r_rtc` | Clock source | LOCO | Specifies the LOCO oscillator as the RTC clock source |
| `r_rtc` | Calendar mode | Enabled | Required for `R_RTC_CalendarTimeGet()` and `R_RTC_CalendarTimeSet()` |
| `r_rtc` | Set Source Clock in Open | **Disabled** | Required for warm-reset RTC preservation — prevents `R_RTC_ClockSourceSet()` from being called on every open, which would reset the running clock |
| `r_rtc` | RTC interrupt priority | Non-zero (≥ 2) | Required; must be lower than the FreeRTOS max syscall priority |
| `r_rtc` | Carry interrupt priority (`carry_ipl`) | ≤ 4 (e.g. 2) | `R_RTC_CalendarTimeGet()` is called from `EventTrigger()` in ISR context and spins waiting for the carry ISR. The carry ISR MUST have higher preemption (lower IPL number) than every ISR that calls `EventTrigger()`. The demo button ISR is at IPL 5; carry must be < 5. The FSP-generated `hal_data.c` default of 12 will cause an **infinite loop/deadlock** when `EventTrigger()` is called from an ISR. The preset config uses `carry_ipl = 2` (safe). `rs_rtc_logger_Open()` checks this at runtime and returns `FSP_ERR_INVALID_HW_CONDITION` if the limit is exceeded. |
| `r_sci_uart` | TX callback | `rs_rtc_logger_uart_callback` | Module-owned callback; must not be changed |
| `r_sci_uart` | Operating mode | Asynchronous | Required for non-blocking UART write |
| `r_sci_uart` | Baud rate | 115200 | Matches the VCOM USB-Serial bridge default on FPB boards |
| `r_icu` | IRQ channel | IRQ9 (P304) | On-board user button S1; configured and owned by the application |
| FreeRTOS | Heap scheme | heap_4 (or heap_3) | Required for `xTaskCreateStatic()` |
| FreeRTOS | `configSUPPORT_STATIC_ALLOCATION` | 1 | Required for static task creation (no heap used for task stack or control block) |

---

## 4. `rtc_time_t` Field Conventions

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

## 5. Adapting to Your Board

### Configuration Path Options

The module supports two ways to supply hardware configuration, selected by `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` in `rs_rtc_logger_cfg.h`.

**Preset Path (default, `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 1`):**
Pre-built HAL instance definitions for the FPB-RA4E2 are provided in `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.c/.h`. The project builds and runs without opening the FSP Stack Configurator. This path is recommended for initial evaluation on the FPB-RA4E2.

```c
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
#include "rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h"
#endif

rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
```

**FSP Stack Configurator Path (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`):**
Configure `r_rtc` and `r_sci_uart` stacks in the e2studio FSP Stack Configurator for your target board. FSP tooling generates `g_rtc0` and `g_uart0` symbols in `ra_gen/`. Pass these to `rs_rtc_logger_ConfigGen()` instead of the preset instances.

```c
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0, &g_uart0);
```

Use this path when targeting a board other than the FPB-RA4E2, or when you need to customize the UART channel, baud rate, RTC clock source, or any other FSP-configurable property. Validate your bindings with `rs_rtc_logger_ConfigCheck()` — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#2-configuration-bridge-call-before-open).

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
| RTC carry interrupt priority (`carry_ipl`) | `configuration.xml` r_rtc properties → Carry Interrupt Priority | Must be strictly lower number (higher preemption) than the IPL of every ISR that calls `rs_rtc_logger_EventTrigger()`. The FSP Stack Configurator default is `carry_ipl = 12`, which is lower preemption than the demo button ISR at `ipl = 5`. Set `carry_ipl` to a value ≤ `RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT` (default 4). `rs_rtc_logger_Open()` returns `FSP_ERR_INVALID_HW_CONDITION` if this is violated. |
| Button debounce window (reference app only) | `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` in `rs_rtc_logger_cfg.h` | Default 150 ms; see [Section 6](#6-debounce-configuration-reference-application) |

### FPB-RA6E2 (Secondary Target)

The FPB-RA6E2 uses the same UART driver (`r_sci_uart`) and the same core source files without modification. Board-specific differences to address:

- The RA6E2 includes TrustZone. The module targets the non-secure partition by default. The reference application must use the BSP TrustZone template configured as a non-secure project.
- Confirm the equivalent user button pin and IRQ channel against the FPB-RA6E2 schematic and update the application-owned `r_icu` configuration accordingly.
- Confirm VBATT schematic support on FPB-RA6E2 if power-down RTC retention is required.

---

## 6. Debounce Configuration (Reference Application)

This is **reference application behavior**, not core module behavior. The core module (`rs_rtc_logger_EventTrigger()`) has no concept of debouncing and accepts every call it receives from any source.

The FPB boards' `r_icu` hardware digital filter is set to `PCLKB_div_1` (120 ns at 25 MHz), which is far shorter than the 5–20 ms bounce period of a mechanical button. Every bounce edge triggers the ISR, producing 7–13 spurious `EventTrigger()` calls per physical button press if left unfiltered.

The reference application implements a software debounce inside `demo_button_s1_callback()` (in `rs_rtc_logger_demo.c`), using the FreeRTOS tick count (`xTaskGetTickCountFromISR()`) to reject ISR firings that arrive sooner than `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` after the last accepted event. This covers typical button bounce while allowing rapid intentional presses spaced apart.

| Constant | Default | Location | Effect |
|---|---|---|---|
| `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` | `150U` | `rs_rtc_logger_cfg.h` | Minimum milliseconds between accepted button-triggered events. Set to `0` to disable the software debounce and rely on the hardware filter only. |

If you replace the button ISR with a different event source (accelerometer, timer, GPIO), decide independently whether that source needs its own debounce/deglitch strategy — the `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` constant and its consuming code are specific to the demo's button callback and are not part of the reusable core.

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

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
