# File Guide — Naming Conventions and What Is Safe to Modify

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Core Code vs. Reference Application Code](#1-core-code-vs-reference-application-code)
2. [Naming Conventions Used in This Module](#2-naming-conventions-used-in-this-module)
3. [Core Files — Copy and Keep; Modify with Care](#3-core-files--copy-and-keep-modify-with-care)
4. [Preset Configuration Files — Reference Only; Do Not Edit](#4-preset-configuration-files--reference-only-do-not-edit)
5. [Reference Application — Adapt Freely](#5-reference-application--adapt-freely)
6. [FSP-Generated Files — Do Not Edit Manually](#6-fsp-generated-files--do-not-edit-manually)

---

## 1. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference or demo application layer. The configuration path column refers to two options explained fully in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board): the **Preset path** (default, uses pre-built HAL instances that work out of the box) and the **FSP Stack Configurator path** (for custom boards or modified hardware settings).

| Layer | Files | Purpose | Reuse Expectation |
|---|---|---|---|
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; circular buffer management; `EventTrigger()` ISR path; Example Project auto-lifecycle and internal consumer task | Copy into your project; core logic |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger.h` | Public interface header — all public function prototypes | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_types.h` | Public type definitions and enumerations | Include directly; do not modify |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_cfg.h` | Build-time configuration constants with `#ifndef` guards | Override individual constants as needed for your project |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_format.c/.h` | Pure timestamp formatting — converts `rtc_time_t` to ASCII string; no I/O; integer arithmetic only | Copy into your project; no board-specific content |
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_hal.c/.h` | Thin HAL shim: UART TX-complete callback registered with UART HAL; sets completion flag and signals FreeRTOS consumer task | Copy into your project; board-agnostic |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.c/.h`<br>`src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.c/.h` | Pre-built HAL instance definitions per board; aliased to FSP-generated instances where appropriate. Also declares/defines `rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, and `rs_rtc_logger_ConfigGetSize()` (see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#2-configuration-bridge-call-before-open)) — these three functions are declared unconditionally regardless of `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` | Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) |
| **Reference App** | `src/rs_rtc_logger_demo.c` | Full demonstration of both modes; CLI task, consumer task (Expert mode), button ISR (including software debounce — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application)), LOCO calibration. Shared, unmodified, by both boards — board selection is via `RS_RTC_LOGGER_TARGET_RA4E2` / `RS_RTC_LOGGER_TARGET_RA6E2`, defined by `firmware/meson.build` from the `-Dmcu=` option | Adapt or replace entirely for your application |
| **Reference App** | `src/blinky_thread_entry_ra4e2.c`<br>`src/blinky_thread_entry_ra6e2.c` | FreeRTOS thread entry; banner output; `r_icu` External IRQ open/enable; LED blink. **Per-board, not shared** — the Meson build (`src/`) keeps one file per board because pin/IRQ/LED mapping differs; the e2studio example projects (`examples/e2studio/RTC_Event_Logger_FPB_<board>/src/`) each carry their own single `blinky_thread_entry.c` copy (no board suffix) since each example project targets only one board | Adapt for your board; not reusable as-is |
| **Reference App** | `src/hal_entry.c` | FSP v6.5.0 application hook, called after `g_hal_init()` in Meson firmware builds; intentionally empty (FreeRTOS scheduler owns application logic via `blinky_thread_entry_*.c`). Not present in the e2studio example projects, where the generated `main.c` does not call `hal_entry()` — see the FSP v6.5.0 migration notes in [BUILD.md](BUILD.md#fsp-v650-migration--deviations-from-e2studio-example-projects) | Meson build only; adapt if your `main()` flow calls `hal_entry()` |
| **Reference App** | `src/hal_warmstart.c` / `.h` | Warm-start detection helper used by demo (Meson build only — RASC generates its own copy for e2studio example projects; see [BUILD.md](BUILD.md#2-hal_warmstartc-is-now-rasc-generated)) | Adapt for your application |
| **Reference App** | `src/fault_handler.c` | Fault handler for debug | Optional; adapt or replace |
| **Reference App** | `src/crt0_stub.c` | Weak `_start` stub required by the Meson/ATfE toolchain link step (picolibc compatibility); not applicable to e2studio-built projects | Meson build only; leave as-is unless changing toolchains |
| **FSP Generated** | `ra/` and `ra_gen/` directories (e2studio example projects) / `firmware/generated/` (Meson build) | FSP-generated configuration, drivers, middleware | Do not edit manually; regenerate via e2studio FSP tooling or the `rasc_generate` Meson target |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it.

---

## 2. Naming Conventions Used in This Module

All identifiers follow a consistent scheme using the `rs_` / `RS_` prefix, which identifies Renesas reference software and avoids conflicts with FSP middleware names (`rm_` / `RM_`). The short name for this module is `rtc_logger`.

| Category | Pattern | Examples |
|---|---|---|
| **Public functions** | `rs_rtc_logger_OperationName()` — lowercase prefix, UpperCamelCase operation | `rs_rtc_logger_EventTrigger()`, `rs_rtc_logger_TimeSet()`, `rs_rtc_logger_StatusGet()` |
| **Internal helper functions** | `rs_rtc_logger_*` or abbreviated `rs_rl_*` — all lowercase | `rs_rtc_logger_uart_callback()`, `rs_rl_enqueue()`, `rs_rl_dequeue()` |
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

## 3. Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_rtc_logger.c` | With care | Core logic; add capability at defined extension points only |
| `rs_rtc_logger.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_rtc_logger_types.h` | No | Public types; changing structures may break binary compatibility |
| `rs_rtc_logger_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_rtc_logger_format.c/.h` | With care | Pure formatting; portable; can be unit-tested in isolation |
| `rs_rtc_logger_hal.c/.h` | With care | Thin HAL shim; modify only if switching UART driver variant |

---

## 4. Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for the FPB-RA4E2 and FPB-RA6E2 and are marked "do not edit." They are reference-quality reproductions of FSP-generated output for the specified board. To customize the UART or RTC configuration for a different board, switch to the FSP Stack Configurator path as described in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board) (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`).

| File | Notes |
|---|---|
| `config/rs_rtc_logger_data_fpb_ra4e2.c` | Preset HAL instance definitions for FPB-RA4E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra4e2.h` | Preset instance declarations; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.c` | Preset HAL instance definitions for FPB-RA6E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.h` | Preset instance declarations; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |

---

## 5. Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_rtc_logger_demo.c` | Starting point for your CLI task and ISR callback; replace or heavily adapt. Board-agnostic (selects target via `RS_RTC_LOGGER_TARGET_RA4E2`/`_RA6E2`) |
| `blinky_thread_entry_ra4e2.c` / `blinky_thread_entry_ra6e2.c` (Meson `src/`)<br>`blinky_thread_entry.c` (each e2studio example project) | FreeRTOS thread entry; adapt for your board's LED/button layout and initialization order |
| `hal_entry.c` | FSP v6.5.0 application hook (Meson build only); adapt if your application needs post-`g_hal_init()` logic that does not belong in the FreeRTOS thread entry |
| `hal_warmstart.c` | Warm-start detection; adapt if your system uses a different reset detection mechanism |
| `fault_handler.c` | Debug fault handler; replace with your production fault handling |

---

## 6. FSP-Generated Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` (e2studio example projects) | FSP-generated drivers, middleware, CMSIS, and board support; regenerate via e2studio FSP tooling when changing `configuration.xml` |
| `ra_gen/` (e2studio example projects) | Auto-generated by FSP Stack Configurator; overwritten on every re-generation |
| `firmware/generated/` (Meson build) | RASC `custom_target` outputs copied by `run_rasc.py` / `run_rasc_bundle.py`; overwritten by the `rasc_generate` Meson target — see [BUILD.md](BUILD.md#-rasc-code-regeneration) |

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
