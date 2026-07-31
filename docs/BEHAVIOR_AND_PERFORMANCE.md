# Demonstrated Behavior and Performance

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Functional Behavior Demonstrated](#1-functional-behavior-demonstrated)
2. [Performance Characteristics](#2-performance-characteristics)
3. [Clock Accuracy Note](#3-clock-accuracy-note)

---

## 1. Functional Behavior Demonstrated

- RTC calendar time set by user via VCOM CLI in `[YYYY-MM-DD HH:MM:SS]` format on power-on reset
- Warm reset detection via `R_SYSTEM->RSTSR0_b.PORF`; RTC configuration preserved; sequence counter restored from `BSP_SECTION_NOINIT` backup. `rs_rtc_logger_hal_rtc_open()` explicitly clears `PORF` after reading it on a power-on reset, so the flag correctly reflects only the reset that just occurred. Without this, the sticky flag would remain set and cause every subsequent reset — including a genuine warm reset — to be misdetected as another power-on reset, silently discarding the retained RTC time. See [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update) for details.
- External interrupt from user button S1 (P304/IRQ9) triggers `rs_rtc_logger_EventTrigger()` from ISR context, after the reference application's software debounce (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application))
- Timestamped log line transmitted over VCOM: `[YYYY-MM-DD HH:MM:SS] Event Detected #<n>`
- LOCO drift calibration via RFRL register adjustment at startup
- Events logged before the time is set display as `[0000-00-00 00:00:00]` with a per-event re-prompt for clock setting

---

## 2. Performance Characteristics

**`rs_rtc_logger_EventTrigger()` ISR overhead:**
The ISR path performs: one `R_RTC_CalendarTimeGet()` HAL call, one atomic `seq_count` increment, one inline copy of the event payload (bounded by `RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN`, default 32 bytes) into the circular buffer, and one `xTaskNotifyFromISR()` call. No UART activity, no formatting, and no heap allocation occur in the ISR path. Execution time is deterministic and bounded.

This overhead applies from the moment `rs_rtc_logger_EventTrigger()` is entered. It does **not** include the reference application's software debounce check in `demo_button_s1_callback()` (a tick-count comparison), which runs before `EventTrigger()` is called and is negligible in cost but is application code, not core module overhead.

**UART TX latency:**
`rs_rtc_logger_hal_uart_write()` submits through `fwrite()`+`fflush(stdout)` on the shared Buffered I/O UART driver's stdio stream, rather than initiating a non-blocking UART write and waiting on a task notification from a UART TX-complete callback. The call briefly blocks the calling task only if a previous transmission (from either the log path or the CLI console) is still physically in flight — the vendored driver's `uart_flush()` spin-waits on its own internal busy flag before queuing the new data — otherwise it returns immediately after queuing and flushing. The typical latency remains sub-millisecond at 115200 baud for log-line-length strings.

**Heap allocation** The core RTC Event Logger module itself performs no dynamic memory allocation: all its buffers are statically allocated or stack-resident within the control block, and no `malloc`/`free` calls appear in `rs_rtc_logger.c`, `rs_rtc_logger_format.c`, or `rs_rtc_logger_hal.c`. However, the **Buffered I/O UART driver** (`syscalls_picolib_stdio_to_uart.c`) does call `malloc()`/`free()` internally, inside its `uart_flush()` implementation, to stage the byte-queue contents into a transmit buffer before handing them to the UART HAL. This is a property of the vendored Buffered I/O driver — if your project has a no-heap requirement, account for this dependency's allocation pattern (or budget for it). See [FILE_GUIDE.md](FILE_GUIDE.md#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit).

**RAM consumption (default configuration):**
- Circular buffer: 8 entries × ~80 bytes per entry (`rtc_time_t`/`struct tm` (9×int = 36 B) + 32-byte payload + seq_count (4 B) + length (4 B) + time_valid (4 B padded)) ≈ 640 bytes
- Line buffer: 256 bytes (inside `rs_rtc_logger_ctrl_t`)
- Total control block: verified at build time via `static_assert` against `RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES` (default 1024 bytes)
- Internal consumer task (Example Project mode): stack 2048 bytes + TCB ~88 bytes on Cortex-M33

**No dynamic memory allocation in the core module.** All of `rs_rtc_logger`'s own buffers are statically allocated or stack-resident within the control block; no `malloc`/`free` calls appear in `rs_rtc_logger.c`, `rs_rtc_logger_format.c`, `rs_rtc_logger_hal.c`, or `rs_rtc_logger_stdio_console.c`. As noted above, the vendored Buffered I/O UART driver does allocate/free heap memory internally.

**No floating-point.** Timestamp formatting uses integer arithmetic only throughout.

---

## 3. Clock Accuracy Note

LOCO provides lower accuracy than the sub-clock (SOSC) crystal oscillator. For the RA4E2, LOCO typically drifts in the range of several hundred to several thousand ppm, which is approximately 0.5–5 minutes per day. For applications requiring higher time accuracy, the sub-clock with the external 32.768 kHz crystal (Y2 on FPB-RA4E2) is recommended — see [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md) for the adaptation path. Runtime drift compensation is available via `rs_rtc_logger_LocoCalibrationSet()`, which adjusts the RFRL register by approximately 3906 ppm per unit.

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
