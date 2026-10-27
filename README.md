# RTC Internal Timestamp Event Logger (`rs_rtc_logger`)

## 1. What This Component Does

The RTC Event Logger demonstrates **timestamped serial event logging using the MCU's on-chip Real-Time Clock (RTC)**. An application-owned interrupt source (by default, a user button) triggers a capture; the RTC Event Logger timestamps the event from the RTC at ISR time, buffers it in a static circular buffer, and transmits a formatted line over a shared UART (VCOM) through a vendored Buffered I/O UART driver, without blocking the calling ISR.

The reference implementation demonstrates:

- ISR-safe RTC timestamp capture, decoupled from UART transmission via a circular buffer
- UART transmission via a shared Buffered I/O UART driver (stdio-based; see [`RELEASE_NOTES`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md)) — the same UART serves both the CLI console and event logging
- Four selectable timestamp formats (ISO 8601, time-only, compact, syslog-style)
- Warm-reset preservation of RTC configuration and event sequence numbering
- LOCO oscillator drift calibration
- Two operating modes — Example Project (simple, auto-managed) and Expert (full application-owned lifecycle)

This repository contains:

- Reusable core source code (`src/rs_rtc_logger/`) with a selectable console
  transport backend (`RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE`):
  - `0` = direct UART HAL path (default)
  - `1` = SEGGER RTT path via J-Link
- A vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`)
- Board-specific reference/preset configuration for FPB-RA4E2 and FPB-RA6E2
- Integration, configuration, and troubleshooting documentation

---

## 2. Who Should Use The RTC Event Logger

Use this Component if you want to:

- Evaluate timestamped event logging built on the RA-family on-chip RTC and UART.
- Learn how RTC, UART, and FreeRTOS can be combined for ISR-safe, non-blocking telemetry capture.
- Reuse the core source in your own application.
- Start from a working reference implementation with warm-reset time preservation already solved.
- Understand how to configure Renesas RA devices for RTC-timestamped serial logging.

> **Reusing the logger in another application:** copy `src/rs_rtc_logger/`.
> For RTT backend support, also copy `src/SEGGER_RTT/`.
>
> Select backend with `RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE`:
> - `0` (default): direct UART HAL write path
> - `1`: SEGGER RTT transport
>


---

## 3. Supported Reference Configuration

| Item | Value |
|---|---|
| Reference Board (primary) | FPB-RA4E2 |
| Reference Board (secondary) | FPB-RA6E2 |
| Device Family | RA4E2 / RA6E2 (Cortex-M33) |
| RTC Driver | `r_rtc` |
| UART Driver | `r_sci_uart` (SCI0, VCOM, 115200 8N1), owned by the vendored Buffered I/O UART driver — see [`CONFIGURATION_GUIDE`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md) |
| External IRQ | `r_icu` (application-owned; User Switch S1) |
| RTOS | FreeRTOS (bundled with FSP v6.5.0) |
| FSP Baseline | v6.5.0 |
| IDE / Tool Flow | e² studio, or Meson + LLVM/Clang from the command line |

Full board-specific assumptions and required non-default FSP settings are in [`CONFIGURATION_GUIDE`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md).

---

## 4. Repository Layout

```text
.
├── README.md                     ← This file: high-level overview, quick start, documentation map
│
├── docs/
│   ├── RTC_EVENT_LOGGER-USER_MANUAL.md   ← Start here: use case, core capabilities, documentation map
│
├── examples/
│   └── VSCode
│       │ 
│       ├── RTC_Event_Logger_FPB_RA4E2/    ← VSCode project for FPB-RA4E2
│       └── RTC_Event_Logger_FPB_RA6E2/    ← VSCode project for FPB-RA6E2
│
├── src/
│   ├── rs_rtc_logger/              ← core source and per-board RTC preset configuration
│       ├── rs_rtc_logger.c / .h
│       ├── rs_rtc_logger_cfg.h
│       ├── rs_rtc_logger_format.c / .h
│       ├── rs_rtc_logger_hal.c / .h
│       ├── rs_rtc_logger_stdio_console.c / .h   ← Adapter API + UART backend (default)
│       ├── rs_rtc_logger_rtt_console.c          ← SEGGER RTT backend
│       ├── rs_rtc_logger_types.h
│       └── config/
│           ├── rs_rtc_logger_data_fpb_ra4e2.c / .h   ← RTC instance
│           └── rs_rtc_logger_data_fpb_ra6e2.c / .h   ← RTC instance
│   └── SEGGER_RTT/                 ← Vendored SEGGER RTT source (used when backend=1)


```

---

## 5. Quick Start

1. Open an example project:

```text
examples/VSCode/RTC_Event_Logger_FPB_RA4E2
```

2. Open the project in VSCode.
3. Confirm the required FSP version (v6.5.0).
4. Build the project (default: Example Project mode).
5. Program the target board (**Run → Debug As → Renesas GDB Hardware Debugging**).
6. Open a serial terminal on the J-Link virtual COM port at **115200 8N1**.
7. Power cycle the target board by disconnecting and re-connecting the USB cable.
8. Follow the date/time and LOCO trim calibration prompts which are only available after a cold start.
9. Press User Switch **S1** on the board to trigger a logged event.

### Build with SEGGER RTT in VSCode (RA4E2 and RA6E2 examples)

The example projects support compile-time transport selection.

1. Open the imported project properties.
2. Go to **C/C++ Build -> Settings -> Tool Settings -> GNU ARM Cross C Compiler -> Preprocessor**.
3. Add this preprocessor symbol:

```text
RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE=1
```

4. Clean and rebuild the project.
5. Start a debug session and open the RTT viewer/terminal in your J-Link tooling.

Notes:
- `RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE=0` (default) uses the Buffered I/O UART backend.
- `RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE=1` uses SEGGER RTT.

---

## 6. Key Features

- ISR-safe, non-blocking event capture via `rs_rtc_logger_EventTrigger()`.
- Static circular event buffer — no dynamic memory allocation in the component core itself.
- Selectable console backend: direct UART HAL (default) or SEGGER RTT.
- Four selectable timestamp formats; runtime-switchable.
- Warm-reset preservation of RTC configuration and event sequence count.
- LOCO oscillator drift calibration at runtime.
- Two operating modes: Example Project (simple) and Expert (full lifecycle control).
- Reference-board configuration for two boards (FPB-RA4E2, FPB-RA6E2): RTC preset plus a Buffered I/O UART board config for each.
- Software debounce for the reference application's button event source.
- Host-based Unity unit tests plus on-target Unity test firmware (predates the Buffered I/O update — see [`RELEASE_NOTES`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md)).

---

## 7. Public Function Model

The RTC Event Logger (Component) supports two usage models. See [`FUNCTIONS_GUIDE`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md) for complete function details, and [`INTEGRATION_GUIDE`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md) for the recommended initialization sequence for each path.

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
| [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md) | What the RTC Event Logger does, dual operating modes, core-vs-demo split, documentation map |

---

## 9. Main Example

See:

```text
examples/VSCode/RTC_Event_Logger_FPB_RA4E2
examples/VSCode/RTC_Event_Logger_FPB_RA6E2
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
---

## 11. Limitations

Current validated scope does not imply support for:

- Power-down RTC retention on FPB-RA4E2 (no VBATT pin/circuit on this board — see [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md)).
- High-accuracy timekeeping out of the box — the default LOCO clock source drifts several hundred to several thousand ppm; the sub-clock (SOSC) alternative requires manual adaptation.
- Boards other than FPB-RA4E2 / FPB-RA6E2 without creating new preset configuration (RTC) and a new Buffered I/O UART board config file (UART), or switching to the FSP Stack Configurator path for the RTC side.
- Event sources other than the reference application's debounced button without independently deciding on deglitch/debounce needs for the new source.
- The core module itself performs no dynamic allocation, but the vendored Buffered I/O UART driver calls `malloc()`/`free()` internally.

---

## 12. Support and Modification Note

The RTC Event Logger is provided as reference source code.

Customers may modify the source to fit their application requirements. Once modified, the resulting implementation becomes customer-owned and future updates may require manual review and merge of local changes.

---

## 13. Next Steps

If you are:

- Evaluating the RTC Event Logger → Start with [`docs/RTC_EVENT_LOGGER-USER_MANUAL.md`](docs/RTC_EVENT_LOGGER-USER_MANUAL.md).
