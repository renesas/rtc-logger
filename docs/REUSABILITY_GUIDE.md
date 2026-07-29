# Reusability and Adaptation for Similar Use Cases

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).
> This guide covers **adapting the core logic to a different event source or use case**. For **wiring the PES into your application as-is** (initialization order, task integration, multi-instance considerations, firmware-update Stop/Start pattern), see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).

## Table of Contents

1. [Core Function Reusability](#1-core-function-reusability)
2. [Portions Applicable Without Modification](#2-portions-applicable-without-modification)
3. [Portions Requiring Modification](#3-portions-requiring-modification)
4. [Example: Adapting for an Accelerometer Event Source](#4-example-adapting-for-an-accelerometer-event-source)

---

## 1. Core Function Reusability

The `rs_rtc_logger_EventTrigger()` function is the universal entry point and is by design **agnostic to the source of the external interrupt**. The core module has no knowledge of what generated the call. Any interrupt source — `r_icu` GPIO, accelerometer callback, timer expiry, UART RX, or a direct software call — can trigger a timestamped log entry by calling this one function. No changes to the core source are required when switching interrupt sources.

Note that the reference application's button-press path adds a software debounce (`demo_button_s1_callback()`, `RS_RTC_LOGGER_CFG_DEBOUNCE_MS` — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application)) *before* calling `EventTrigger()`. That debounce is specific to a mechanical switch and is not carried along automatically when you swap in a different event source — decide independently whether your new source needs deglitching.

---

## 2. Portions Applicable Without Modification

- All files in `src/rs_rtc_logger/` can be included directly in any FSP v6.5.0 FreeRTOS project targeting the RA4E2 or RA6E2 without modification, provided the configuration is supplied via `rs_rtc_logger_ConfigGen()`. This requires bringing along the vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`, used unmodified) and a matching `stdio_uart_config_<board>.c` file for your target board — `rs_rtc_logger_hal_uart_open()` depends on it via `rs_rtc_logger_stdio_console.c`. See [FILE_GUIDE.md](FILE_GUIDE.md#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit).
- The timestamp formatter (`rs_rtc_logger_format.c`) is a pure function with no I/O or side effects and can be extracted for use in unit tests or other logging implementations.
- The circular buffer and ISR-safe enqueue logic inside `rs_rtc_logger.c` represents a portable, heap-free, single-producer/single-consumer event buffer pattern reusable in similar telemetry designs. See [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#8-internal-functions--rs_rtc_loggerc) for the internal `rs_rl_enqueue()` / `rs_rl_dequeue()` functions that implement this pattern.

---

## 3. Portions Requiring Modification

- **Board-specific RTC preset config files** (`config/rs_rtc_logger_data_fpb_<board>.c/.h`): Create new preset files for other target boards, or switch to the FSP Stack Configurator path (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board)). These now cover the RTC instance only.
- **Board-specific Buffered I/O UART config** (`src/rs_stdio_uart_buffer_io/stdio_uart_config_<board>.c`): Create a new one for other target boards, following the pattern in `stdio_uart_config_ra4e2_fpb.c` / `_ra6e2_fpb.c`. The UART channel, baud rate, and pin configuration live here now, not in the RTC preset files.
- **Reference application** (`rs_rtc_logger_demo.c`, `blinky_thread_entry_ra4e2.c` / `_ra6e2.c`): Replace the CLI task, consumer task structure, ISR callback, and debounce logic with application-specific code. The demo application is demonstration scaffolding, not production code.
- **RTC clock source**: Replace LOCO with the sub-clock (SOSC) for applications requiring higher time accuracy. Change the `r_rtc` clock source property in `configuration.xml` and remove or adapt the LOCO calibration prompt.

---

## 4. Example: Adapting for an Accelerometer Event Source

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

For the initialization order, task integration, and multi-instance considerations relevant to wiring this new event source in, see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
