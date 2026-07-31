# Release Notes and Adopting a Newer Release

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Current Version](#1-current-version)
2. [Buffered I/O over UART](#2-buffered-io-over-uart)
3. [Adopting a Newer Release](#3-adopting-a-newer-release)
4. [What Typically Changes Between Releases](#4-what-typically-changes-between-releases)
5. [Recommended Merge Process](#5-recommended-merge-process)

---

## 1. Current Version

| Field | Value |
|---|---|
| Version | 1.0.0 |
| FSP Baseline | v6.5.0 |
| Primary Target | FPB-RA4E2 |
| Secondary Target | FPB-RA6E2 |

Both targets include the Buffered I/O over UART driver described in [Section 2](#2-buffered-io-over-uart) below.

Query the running version at any time (including CLOSED state) via `rs_rtc_logger_VersionGet()` — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#5-configuration-and-query-functions).

---

## 2. Buffered I/O over UART

This release affects how the RTC Event Logger transmits and receives console/log data over UART. It shares a single physical UART with the reference application's CLI console through a vendored **Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`), accessed through standard C stdio (`printf`/`getchar`/`fwrite`/`fflush`).

**Delivered and confirmed on both reference boards:** FPB-RA4E2 and FPB-RA6E2 (`RTC_Event_Logger_FPB_RA4E2` and `RTC_Event_Logger_FPB_RA6E2` e2studio projects). The two board ports are symmetric — same core module, same adapter, same vendored driver — differing only in the per-board files called out in [Section 2.2](#22-per-board-differences) below.

### 2.1 New components

| Component | Nature | Purpose |
|---|---|---|
| `src/rs_stdio_uart_buffer_io/` | Vendored, used **unmodified** — not part of the RTC Event Logger's own source tree | Byte-queue-backed (`r_byteq.c/.h`) picolibc stdio-to-UART bridge (`syscalls_picolib_stdio_to_uart.c`); generic UART instance config/validation helpers (`rs_stdio_uart.c/.h`) supporting `r_sci_uart`, `r_sci_b_uart`, or `r_uarta` depending on what the target MCU's FSP HAL provides; per-board reference UART configuration (`stdio_uart_config_ra4e2_fpb.c` guarded by `BOARD_RA4E2_FPB`, and `stdio_uart_config_ra6e2_fpb.c` guarded by `BOARD_RA6E2_FPB`) |
| `src/rs_rtc_logger/rs_rtc_logger_stdio_console.c/.h` | New — thin adapter, ships inside the core folder | Bridges the RTC Event Logger to the vendored driver's stdio surface: forces deterministic init + validation (`rs_rtc_logger_console_init()`, called by `rs_rtc_logger_hal_uart_open()` — a genuine **core dependency**, not just demo glue), plus non-blocking character read (`rs_rtc_logger_console_getc_nb()`) and string write (`rs_rtc_logger_console_puts()`) used only by the reference CLI |

See [FILE_GUIDE.md](FILE_GUIDE.md) for the full reuse/modify guidance on these files, and [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) and [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) for the updated hardware and initialization details.

### 2.2 Per-Board Differences

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

The differing `brr`/PCLKA values reflect each board's own PLL configuration in this project (RA6E2: HOCO ×10 ÷1 = 200 MHz, PCLKA ÷4 = 50 MHz) — not a difference in the driver or the Buffered I/O design itself. See [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) for the full per-board hardware table.

---

## 3. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

---

## 4. What Typically Changes Between Releases

- Core source files (`rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`) may receive reliability improvements, new features, or FSP version compatibility updates.
- Public headers (`rs_rtc_logger.h`, `rs_rtc_logger_types.h`) may add new public functions or extend existing structures. Additions are generally straightforward to adopt; removals or signature changes — are not always accompanied by a version-number bump, so always diff the public headers rather than relying on `RS_RTC_LOGGER_VERSION` alone.
- `rs_rtc_logger_cfg.h` constants may gain new `#ifndef`-guarded defaults. Your existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases or corrected for hardware errata, or to change which peripherals the module owns directly.

---

## 5. Recommended Merge Process

1. Diff the updated core files against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications.
3. Review the updated `rs_rtc_logger.h` and `rs_rtc_logger_types.h` for interface changes; update call sites in your application accordingly.
4. Rebuild and retest on target hardware.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading. If the FSP version changes between your baseline and the update, rerun the FSP Stack Configurator to regenerate `ra_gen/` content for the new FSP version, or re-run the Meson `rasc_generate` target — see [BUILD.md](BUILD.md#-rasc-code-regeneration).

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
