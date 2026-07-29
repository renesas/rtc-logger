# File Guide — Naming Conventions and What Is Safe to Modify

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Core Code vs. Reference Application Code](#1-core-code-vs-reference-application-code)
2. [Naming Conventions Used in This Module](#2-naming-conventions-used-in-this-module)
3. [Core Files — Copy and Keep; Modify with Care](#3-core-files--copy-and-keep-modify-with-care)
4. [Preset Configuration Files — Reference Only; Do Not Edit](#4-preset-configuration-files--reference-only-do-not-edit)
4a. [Vendored Buffered I/O UART Driver — Reference Only; Do Not Edit](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit)
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
| **Core** | `src/rs_rtc_logger/rs_rtc_logger_hal.c/.h` | Thin HAL shim: RTC open/close/get/set wrappers (direct raw-driver calls). UART open/close/write wrappers — they route through `rs_rtc_logger_stdio_console.c` to the shared Buffered I/O UART driver's stdio stream. This component owns no UART ISR callback (see [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update)) | Copy into your project; board-agnostic |
| **Core (new dependency)** | `src/rs_rtc_logger/rs_rtc_logger_stdio_console.c/.h` | Thin adapter bridging this module to the vendored Buffered I/O UART driver. Physically lives in the core folder; its `rs_rtc_logger_console_init()` is called from `rs_rtc_logger_hal_uart_open()`, making it a genuine core dependency, not just demo glue — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#11-internal-functions--rs_rtc_logger_stdio_consolec--h) | Copy into your project alongside the rest of core; board-agnostic itself, but requires the vendored driver below |
| **Vendored (new, used unmodified)** | `src/rs_stdio_uart_buffer_io/` (`r_byteq.c/.h`, `rs_stdio_uart.c/.h`, `syscalls_picolib_stdio_to_uart.c`, plus one `stdio_uart_config_<board>_fpb.c` per board) | Buffered I/O UART driver: byte-queue-backed picolibc stdio-to-UART bridge. Owns the shared physical UART (`g_uart_jlob_vcom`) for the whole application — both log output and CLI console | Use verbatim; do not modify (see [Section 4a](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) below). Add a new `stdio_uart_config_<board>_fpb.c` when porting to a new board. |
| **Preset Config** | `src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.c/.h`<br>`src/rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.c/.h` | Pre-built **RTC-only** HAL instance definitions per board (no UART preset any more — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board)). Also declares/defines `rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, and `rs_rtc_logger_ConfigGetSize()` (see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#2-configuration-bridge-call-before-open)) — these three functions are declared unconditionally regardless of `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE` | Reference only — do not edit; for custom boards, use the FSP Stack Configurator path instead (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) |
| **Reference App** | `src/rs_rtc_logger_demo.c` | Full demonstration of both modes; CLI task, consumer task (Expert mode), button ISR (including software debounce — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application)), LOCO calibration. CLI I/O routes through `rs_rtc_logger_console_getc_nb()`/`rs_rtc_logger_console_puts()`. Shared, unmodified, by both boards in the Meson build — board selection is via `RS_RTC_LOGGER_TARGET_RA4E2` / `RS_RTC_LOGGER_TARGET_RA6E2` | Adapt or replace entirely for your application |
| **Reference App** | `src/blinky_thread_entry.c` (one copy per e2studio board project; `blinky_thread_entry_ra4e2.c` / `_ra6e2.c` in the Meson `src/` tree) | FreeRTOS thread entry; sign-on banner; `r_icu` External IRQ open/enable; LED blink. The banner calls `printf()`, which transparently triggers the vendored driver's init | Adapt for your board; not reusable as-is |
| **Reference App** | `src/hal_entry.c` | FSP v6.5.0 application hook, called after `g_hal_init()` in Meson firmware builds; intentionally empty (FreeRTOS scheduler owns application logic via `blinky_thread_entry_*.c`). Not present in the e2studio example projects, where the generated `main.c` does not call `hal_entry()` — see the FSP v6.5.0 migration notes in [BUILD.md](BUILD.md#fsp-v650-migration--deviations-from-e2studio-example-projects) | Meson build only; adapt if your `main()` flow calls `hal_entry()` |
| **Reference App** | `src/hal_warmstart.c` / `.h` | Warm-start detection helper used by demo (Meson build only — RASC generates its own copy for e2studio example projects; see [BUILD.md](BUILD.md#2-hal_warmstartc-is-now-rasc-generated)) | Adapt for your application |
| **Reference App** | `src/fault_handler.c` | Fault handler for debug | Optional; adapt or replace |
| **Reference App** | `src/crt0_stub.c` | Weak `_start` stub required by the Meson/ATfE toolchain link step (picolibc compatibility); not applicable to e2studio-built projects | Meson build only; leave as-is unless changing toolchains |
| **FSP Generated** | `ra/` and `ra_gen/` directories (e2studio example projects) / `firmware/generated/` (Meson build) | FSP-generated configuration, drivers, middleware. `g_uart_jlob_vcom` (the shared VCOM UART instance) is generated here — the FSP naming convention renamed what earlier development iterations called `g_uart0` | Do not edit manually; regenerate via e2studio FSP tooling or the `rasc_generate` Meson target |

**Key separation:** The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer; the RTC logging core remains independent of it. The CLI and the core module's log writes go through the vendored Buffered I/O UART driver, shared by both — see the new rows above.

---

## 2. Naming Conventions Used in This Module

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

## 3. Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_rtc_logger.c` | With care | Core logic; add capability at defined extension points only |
| `rs_rtc_logger.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_rtc_logger_types.h` | No | Public types; changing structures may break binary compatibility |
| `rs_rtc_logger_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_rtc_logger_format.c/.h` | With care | Pure formatting; portable; can be unit-tested in isolation |
| `rs_rtc_logger_hal.c/.h` | With care | Thin HAL shim; RTC wrappers call the raw driver directly, UART wrappers route through `rs_rtc_logger_stdio_console.c` — modify only if switching UART driver variant or RTC driver |
| `rs_rtc_logger_stdio_console.c/.h` | With care | Vendored Buffered I/O UART driver. `rs_rtc_logger_hal_uart_open()` depends on `rs_rtc_logger_console_init()` here — do not remove this file without re-pointing that dependency |

---

## 4. Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for the FPB-RA4E2 and FPB-RA6E2 and are marked "do not edit." They are reference-quality reproductions of FSP-generated output for the specified board's **RTC instance only** — there is no UART preset any more (see [Section 4a](#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) below for the UART side). To customize the RTC configuration for a different board, switch to the FSP Stack Configurator path as described in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board) (`RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0`).

| File | Notes |
|---|---|
| `config/rs_rtc_logger_data_fpb_ra4e2.c` | Preset RTC instance definition for FPB-RA4E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra4e2.h` | Preset RTC instance declaration; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.c` | Preset RTC instance definition for FPB-RA6E2; do not edit |
| `config/rs_rtc_logger_data_fpb_ra6e2.h` | Preset RTC instance declaration; also declares `ConfigGen()`/`ConfigCheck()`/`ConfigGetSize()`; do not edit |

---

## 4a. Vendored Buffered I/O UART Driver — Reference Only; Do Not Edit

The Buffered I/O over UART driver(see [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update)). This driver is used **verbatim, unmodified** — it is a separate component, not merged into the RTC Event Logger's own source tree, and is documented here only so you know what it is and where its board-specific boundary is.

| File | Notes |
|---|---|
| `rs_stdio_uart_buffer_io/r_byteq.c/.h` | Generic byte-queue implementation backing the RX/TX streams; board-agnostic; do not edit |
| `rs_stdio_uart_buffer_io/rs_stdio_uart.c/.h` | Generic UART instance config/validation helpers, supporting `r_sci_uart`, `r_sci_b_uart`, or `r_uarta` depending on what the target's FSP HAL provides; do not edit |
| `rs_stdio_uart_buffer_io/syscalls_picolib_stdio_to_uart.c` | picolibc stdio stream redirection (`printf`/`getchar`/`fflush` → UART), lazy driver init, ISR callback; do not edit |
| `rs_stdio_uart_buffer_io/stdio_uart_config_ra4e2_fpb.c` | **Board-specific** — the one file in this directory you copy and adapt when porting to a new board. Guarded by `BOARD_RA4E2_FPB`; provides the reference pin mapping, baud registers, and `RS_STDIO_UART_CheckApplicationConfiguration()` reference values for FPB-RA4E2 |
| `rs_stdio_uart_buffer_io/stdio_uart_config_ra6e2_fpb.c` | Same as above, for FPB-RA6E2, guarded by `BOARD_RA6E2_FPB` |

Porting to a new board means adding a new `stdio_uart_config_<board>.c` file following the pattern in the two files above — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board).

---

## 5. Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_rtc_logger_demo.c` | Starting point for your CLI task and ISR callback; replace or heavily adapt. Board-agnostic (selects target via `RS_RTC_LOGGER_TARGET_RA4E2`/`_RA6E2`). CLI I/O goes through `rs_rtc_logger_console_getc_nb()`/`rs_rtc_logger_console_puts()` |
| `blinky_thread_entry_ra4e2.c` / `blinky_thread_entry_ra6e2.c` (Meson `src/`)<br>`blinky_thread_entry.c` (each e2studio example project) | FreeRTOS thread entry; adapt for your board's LED/button layout and initialization order. The sign-on banner is `printf()` calls |
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
