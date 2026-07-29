# RTC Internal Timestamp Event Logger (`rs_rtc_logger`)

## 1. What This PES Does

The RTC Event Logger PES demonstrates **timestamped serial event logging using the MCU's on-chip Real-Time Clock (RTC)**. An application-owned interrupt source (by default, a user button) triggers a capture; the PES timestamps the event from the RTC at ISR time, buffers it in a static circular buffer, and transmits a formatted line over a shared UART (VCOM) through a vendored Buffered I/O UART driver, without blocking the calling ISR.

The reference implementation demonstrates:

- ISR-safe RTC timestamp capture, decoupled from UART transmission via a circular buffer
- UART transmission via a shared Buffered I/O UART driver (stdio-based; see [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md#2-buffered-io-over-uart-update)) — the same UART serves both the CLI console and event logging
- Four selectable timestamp formats (ISO 8601, time-only, compact, syslog-style)
- Warm-reset preservation of RTC configuration and event sequence numbering
- LOCO oscillator drift calibration
- Two operating modes — Example Project (simple, auto-managed) and Expert (full application-owned lifecycle)

This repository contains:

- Reusable PES core source code (`src/rs_rtc_logger/`)
- A vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`)
- Board-specific reference/preset configuration for FPB-RA4E2 and FPB-RA6E2
- Complete e2studio example projects for both boards
- A Meson + LLVM/Clang build for host unit tests and command-line firmware builds
- Integration, configuration, and troubleshooting documentation
- Host and on-target unit test suites (51 Unity tests)

---

## 2. Who Should Use This PES

Use this PES if you want to:

- Evaluate timestamped event logging built on the RA-family on-chip RTC and UART.
- Learn how RTC, UART, and FreeRTOS can be combined for ISR-safe, non-blocking telemetry capture.
- Reuse the PES core source in your own application.
- Start from a working reference implementation with warm-reset time preservation already solved.
- Understand how to configure Renesas RA devices for RTC-timestamped serial logging.

---

## 3. Supported Reference Configuration

| Item | Value |
|---|---|
| Reference Board (primary) | FPB-RA4E2 |
| Reference Board (secondary) | FPB-RA6E2 |
| Device Family | RA4E2 / RA6E2 (Cortex-M33) |
| RTC Driver | `r_rtc` |
| UART Driver | `r_sci_uart` (SCI0, VCOM, 115200 8N1), owned by the vendored Buffered I/O UART driver — see [`docs/CONFIGURATION_GUIDE.md`](docs/CONFIGURATION_GUIDE.md) |
| External IRQ | `r_icu` (application-owned; User Switch S1) |
| RTOS | FreeRTOS (bundled with FSP v6.5.0) |
| FSP Baseline | v6.5.0 |
| IDE / Tool Flow | e² studio, or Meson + LLVM/Clang from the command line |

Full board-specific assumptions and required non-default FSP settings are in [`docs/CONFIGURATION_GUIDE.md`](docs/CONFIGURATION_GUIDE.md).

---

## 4. Repository Layout

```text
.
├── README.md                     ← This file: high-level overview, quick start, documentation map
│
├── docs/
│   ├── RTC_EVENT_LOGGER-USER_MANUAL.md   ← Start here: use case, core capabilities, documentation map
│   ├── FUNCTIONS_GUIDE.md               ← Function reference — public and private/internal, each tagged; naming conventions
│   ├── INTEGRATION_GUIDE.md              ← Initialization order, task/RTOS integration, warm-reset ordering, multi-instance considerations
│   ├── CONFIGURATION_GUIDE.md            ← Reference board config, adapting to your board, dependencies
│   ├── FILE_GUIDE.md                     ← Core vs. demo vs. FSP-generated files; what is safe to modify
│   ├── ARCHITECTURE.md                   ← Lifecycle state machine, state gating, event/telemetry flow
│   ├── BEHAVIOR_AND_PERFORMANCE.md       ← Demonstrated behavior, timing, RAM budget, clock accuracy
│   ├── EXAMPLES_GUIDE.md                 ← Importing/building the e2studio example, expected VCOM output
│   ├── REUSABILITY_GUIDE.md              ← Adapting the core logic to other event sources and workflows
│   ├── RELEASE_NOTES.md                  ← Version history, adopting a newer release
│   └── BUILD.md                          ← Command-line build/test instructions (Meson + LLVM/Clang)
│
├── examples/
│   └── e2studio/
│       ├── RTC_Event_Logger_FPB_RA4E2/    ← e²studio project for FPB-RA4E2
│       └── RTC_Event_Logger_FPB_RA6E2/    ← e²studio project for FPB-RA6E2
│
├── firmware/
│   ├── meson.build                ← Meson firmware build (per-board ELF targets)
│   └── generated/                 ← RASC custom_target outputs (populated by rasc_generate)
│
├── src/
│   ├── rs_rtc_logger/              ← PES core source and per-board RTC preset configuration
│   │   ├── rs_rtc_logger.c / .h
│   │   ├── rs_rtc_logger_cfg.h
│   │   ├── rs_rtc_logger_format.c / .h
│   │   ├── rs_rtc_logger_hal.c / .h
│   │   ├── rs_rtc_logger_stdio_console.c / .h   ← Adapter to the vendored Buffered I/O UART driver
│   │   ├── rs_rtc_logger_types.h
│   │   └── config/
│   │       ├── rs_rtc_logger_data_fpb_ra4e2.c / .h   ← RTC instance only (no UART preset)
│   │       └── rs_rtc_logger_data_fpb_ra6e2.c / .h   ← RTC instance only (no UART preset)
│   ├── rs_stdio_uart_buffer_io/     ← Vendored Buffered I/O UART driver, used unmodified
│   │   ├── r_byteq.c / .h
│   │   ├── rs_stdio_uart.c / .h
│   │   ├── syscalls_picolib_stdio_to_uart.c
│   │   ├── stdio_uart_config_ra4e2_fpb.c
│   │   └── stdio_uart_config_ra6e2_fpb.c
│   ├── rs_rtc_logger_demo.c        ← Reference application (shared by both boards)
│   ├── blinky_thread_entry_ra4e2.c / _ra6e2.c   ← Per-board FreeRTOS thread entry
│   ├── hal_entry.c                 ← FSP v6.5.0 application hook (Meson build only)
│   ├── hal_warmstart.c / .h        ← Warm-start detection helper
│   ├── fault_handler.c             ← Debug fault handler
│   └── crt0_stub.c                 ← Weak _start stub (Meson/ATfE toolchain compatibility)
│
├── tests/
│   ├── meson.build                 ← Test build configuration (host vs. target tests)
│   ├── run_ut.robot                ← Run on-target Unity firmware tests
│   ├── stubs/                      ← Host-build FSP/FreeRTOS type stubs
│   ├── target/                     ← On-target Unity port (SCI UART)
│   ├── test_main.c                 ← Host-based Unity test entry point
│   ├── test_main_firmware.c        ← On-target Unity test entry point
│   ├── test_rs_rtc_logger_core.c   ← Lifecycle + circular buffer unit tests
│   └── test_rs_rtc_logger_format.c ← Timestamp formatting unit tests
│
├── meson/
│   ├── llvm-clangcl-msvc.ini              ← Windows host native file (unit tests)
│   ├── llvm-clangcl-ubuntu2404-ci.ini     ← Linux host native file (unit tests / CI)
│   └── llvm-cortex-m33.ini                ← Cortex-M33 cross file (both boards)
│
├── toolchains/
│   ├── llvm-cortex-m23.ini         ← Meson cross file for Cortex-M23 (RA0 series)
│   └── llvm-cortex-m33.ini         ← Meson cross file for Cortex-M23/M33 (RA2/RA4/RA6 series)
│
└── scripts/
    ├── env.ps1                     ← Windows PATH setup (edit before use)
    ├── run_rasc.py                 ← Headless RASC code generation + output copy
    ├── run_rasc_bundle.py          ← Smart bundle generation for TrustZone MCUs
    └── UartLibrary.py              ← Optional VCOM helper (Robot Framework)
```

> **Note:** A top-level `meson.build` / `meson_options.txt` and a `subprojects/` directory
> (holding the `unity`, `fsp`, and `cmsis` Meson wraps) are required to drive the builds
> described in [`docs/BUILD.md`](docs/BUILD.md)

---

## 5. Quick Start

1. Open an example project:

```text
examples/e2studio/RTC_Event_Logger_FPB_RA4E2
```

2. Import the project into e² studio.
3. Confirm the required FSP version (v6.5.0).
4. Build the project (default: Example Project mode).
5. Program the target board (**Run → Debug As → Renesas GDB Hardware Debugging**).
6. Open a serial terminal on the J-Link virtual COM port at **115200 8N1**.
7. Follow the date/time and LOCO calibration prompts.
8. Press User Switch **S1** on the board to trigger a logged event.

See [`docs/EXAMPLES_GUIDE.md`](docs/EXAMPLES_GUIDE.md) for full expected output, and [`docs/BUILD.md`](docs/BUILD.md) for the command-line Meson build (host unit tests and firmware).

---

## 6. Key Features

- ISR-safe, non-blocking event capture via `rs_rtc_logger_EventTrigger()`.
- Static circular event buffer — no dynamic memory allocation in the PES core itself.
- UART transmission via a shared Buffered I/O UART driver (vendored, used unmodified) — the same UART serves both CLI console and event logging.
- Four selectable timestamp formats; runtime-switchable.
- Warm-reset preservation of RTC configuration and event sequence count.
- LOCO oscillator drift calibration at runtime.
- Two operating modes: Example Project (simple) and Expert (full lifecycle control).
- Reference-board configuration for two boards (FPB-RA4E2, FPB-RA6E2): RTC preset plus a Buffered I/O UART board config for each.
- Software debounce for the reference application's button event source.
- Host-based Unity unit tests plus on-target Unity test firmware (predates the Buffered I/O update — see [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md#2-buffered-io-over-uart-update)).

---

## 7. Public Function Model

The PES supports two usage models. See [`docs/FUNCTIONS_GUIDE.md`](docs/FUNCTIONS_GUIDE.md) for complete function details, and [`docs/INTEGRATION_GUIDE.md`](docs/INTEGRATION_GUIDE.md) for the recommended initialization sequence for each path.

### Simple Path (Example Project mode)

```c
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);   // once, from task context
// ...
rs_rtc_logger_EventTrigger(&g_logger_ctrl, (uint8_t const *)"Event Detected", 14U); // from ISR
```

### Expert Path (full lifecycle control)

```c
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
rs_rtc_logger_ConfigCheck(&g_logger_cfg);
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
rs_rtc_logger_Start(&g_logger_ctrl);
rs_rtc_logger_EventTrigger(&g_logger_ctrl, p_data, length);
rs_rtc_logger_Stop(&g_logger_ctrl);
rs_rtc_logger_Close(&g_logger_ctrl);
```

---

## 8. Main Documentation

| Document | Purpose |
|---|---|
| [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md) | What the PES does, dual operating modes, core-vs-demo split, documentation map |
| [`docs/FUNCTIONS_GUIDE.md`](docs/FUNCTIONS_GUIDE.md) | Public **and** private/internal function behavior (each tagged), naming conventions, state gating |
| [`docs/INTEGRATION_GUIDE.md`](docs/INTEGRATION_GUIDE.md) | Initialization order, task/RTOS integration, warm-reset ordering, multi-instance considerations |
| [`docs/CONFIGURATION_GUIDE.md`](docs/CONFIGURATION_GUIDE.md) | Reference board config, required FSP settings, porting to a new board |
| [`docs/FILE_GUIDE.md`](docs/FILE_GUIDE.md) | Core vs. demo vs. FSP-generated files; what is safe to modify |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Lifecycle state machine, state gating table, event flow |
| [`docs/BEHAVIOR_AND_PERFORMANCE.md`](docs/BEHAVIOR_AND_PERFORMANCE.md) | Demonstrated behavior, ISR/UART timing, RAM budget |
| [`docs/EXAMPLES_GUIDE.md`](docs/EXAMPLES_GUIDE.md) | Importing/building the e2studio example, expected output |
| [`docs/REUSABILITY_GUIDE.md`](docs/REUSABILITY_GUIDE.md) | Adapting the core logic to other event sources |
| [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md) | Version history, adopting a newer release |
| [`docs/BUILD.md`](docs/BUILD.md) | Command-line build/test instructions (Meson + LLVM/Clang), FSP v6.5.0 migration notes, troubleshooting |

---

## 9. Main Example

See:

```text
examples/e2studio/RTC_Event_Logger_FPB_RA4E2
examples/e2studio/RTC_Event_Logger_FPB_RA6E2
```

The examples demonstrate:

- RTC-timestamped event logging over VCOM.
- Cold-start date/time and LOCO calibration prompts.
- Warm-reset time and sequence-counter preservation.
- Both Example Project and Expert operating modes.
- Debounced button-triggered event capture.

---

## 10. Validated Scope

The intended reference validation scope includes:

- FPB-RA4E2 (primary) and FPB-RA6E2 (secondary)
- RTC + UART (VCOM) integration at 115200 baud
- Example Project and Expert operating modes
- Warm-reset RTC and sequence-counter preservation
- Host-based Unity unit tests (lifecycle, circular buffer, formatting) plus on-target Unity test firmware

See [`docs/BEHAVIOR_AND_PERFORMANCE.md`](docs/BEHAVIOR_AND_PERFORMANCE.md) and [`docs/BUILD.md`](docs/BUILD.md#-host-unit-tests) for details.

---

## 11. Limitations

Current validated scope does not imply support for:

- Power-down RTC retention on FPB-RA4E2 (no VBATT pin/circuit on this board — see [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md)).
- High-accuracy timekeeping out of the box — the default LOCO clock source drifts several hundred to several thousand ppm; the sub-clock (SOSC) alternative requires manual adaptation (see [`docs/REUSABILITY_GUIDE.md`](docs/REUSABILITY_GUIDE.md)).
- Boards other than FPB-RA4E2 / FPB-RA6E2 without creating new preset configuration (RTC) and a new Buffered I/O UART board config file (UART), or switching to the FSP Stack Configurator path for the RTC side.
- Event sources other than the reference application's debounced button without independently deciding on deglitch/debounce needs for the new source.
- The core module itself performs no dynamic allocation, but the vendored Buffered I/O UART driver calls `malloc()`/`free()` internally (see [`docs/BEHAVIOR_AND_PERFORMANCE.md`](docs/BEHAVIOR_AND_PERFORMANCE.md)).

---

## 12. Support and Modification Note

This PES is provided as reference source code.

Customers may modify the source to fit their application requirements. Once modified, the resulting implementation becomes customer-owned and future PES updates may require manual review and merge of local changes. See [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md).

---

## 13. Next Steps

If you are:

- Evaluating the PES → Start with [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md).
- Running the example → Read [`docs/EXAMPLES_GUIDE.md`](docs/EXAMPLES_GUIDE.md).
- Building from the command line → Read [`docs/BUILD.md`](docs/BUILD.md).
- Integrating the PES → Read [`docs/INTEGRATION_GUIDE.md`](docs/INTEGRATION_GUIDE.md).
- Writing code against the module → Read [`docs/FUNCTIONS_GUIDE.md`](docs/FUNCTIONS_GUIDE.md).
- Porting to your board → Read [`docs/CONFIGURATION_GUIDE.md`](docs/CONFIGURATION_GUIDE.md).
- Reviewing what's safe to modify → Read [`docs/FILE_GUIDE.md`](docs/FILE_GUIDE.md).
- Reusing this for a different use case → Read [`docs/REUSABILITY_GUIDE.md`](docs/REUSABILITY_GUIDE.md).
