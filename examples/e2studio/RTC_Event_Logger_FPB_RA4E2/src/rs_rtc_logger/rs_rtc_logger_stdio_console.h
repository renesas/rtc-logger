/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_stdio_console.h
 * @brief Thin, non-blocking adapter between the RTC Event Logger demo application and the
 *        vendored Buffered I/O UART driver (src/rs_stdio_uart_buffer_io/).
 *
 * WHY THIS FILE EXISTS
 * ---------------------
 * The Buffered I/O UART driver is used *verbatim, unmodified* (see src/rs_stdio_uart_buffer_io/)
 * as a secondary component — it is not merged into the RTC Event Logger's own source tree.
 * Its public surface is picolibc stdio (printf/fprintf/putchar/getchar/fflush).
 *
 * getchar() in that driver blocks indefinitely (documented limitation — see
 * docs/README-BUFFERED-UART.md §11.1 in the driver's own repository). The RTC Event Logger's
 * CLI task must remain responsive (it also has to react to the S1 button / RTC event
 * notifications), so this adapter provides a NON-BLOCKING character read on top of the *same*
 * stdin stream, without modifying a single line of the vendored driver.
 *
 * HOW IT WORKS
 * ------------
 * syscalls_picolib_stdio_to_uart.c (part of the vendored driver) declares its RX byteq control
 * block, `uart_rx_q_ctrl`, at file scope WITHOUT the `static` qualifier. That makes it an
 * ordinary externally-linkable symbol, so this adapter can link against it directly and ask
 * the byteq "how many bytes are waiting?" (R_BYTEQ_Used) before ever calling stdio's getchar(),
 * which only blocks when no data is queued. No vendored source file is touched to make this work.
 *
 * PUBLIC VS. PRIVATE SURFACE
 * --------------------------
 * This adapter prefers the driver's documented public API wherever one exists, and only reaches
 * past it where the public surface genuinely has no alternative:
 *   - Console I/O:            picolibc stdio (printf/fprintf/getchar/fflush) — fully public.
 *   - Config validation:      RS_STDIO_UART_CheckApplicationConfiguration() — declared in
 *                             rs_stdio_uart.h and explicitly documented (README §4.6, "Step 7")
 *                             as the function applications should call. rs_stdio_config_gen()
 *                             (used by the driver's own reference demo) was deliberately NOT
 *                             adopted here: it isn't declared in any header of the driver — the
 *                             demo only reaches it via an ad-hoc inline `extern` — and it only
 *                             validates the driver's static reference table against itself,
 *                             never touching the real g_uart_jlob_vcom instance, so it adds no
 *                             real assurance for this project.
 *   - Non-blocking RX peek:   R_BYTEQ_Used(&uart_rx_q_ctrl, ...) — NOT public (uart_rx_q_ctrl is
 *                             an undeclared, merely non-static symbol). Used anyway because the
 *                             driver's documented public API has no non-blocking read primitive
 *                             at all (README §11.1: RX always blocks) — there is no public
 *                             alternative for the non-blocking requirement this adapter exists
 *                             to satisfy.
 */

#ifndef RS_RTC_LOGGER_STDIO_CONSOLE_H
#define RS_RTC_LOGGER_STDIO_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Force Buffered I/O UART driver initialization deterministically, and
 *        strictly validate g_uart_jlob_vcom's configuration.
 *
 * The driver initializes lazily on first printf()/getchar()/fflush() call
 * (jlob_uart_init_once()). Call this once, early (e.g. from the banner),
 * so the UART is guaranteed open and its ISR callback installed before any
 * other code (the RTC Event Logger itself, button ISR, etc.) runs.
 *
 * Also runs the Buffered I/O UART driver's documented, public configuration
 * validation entry point — RS_STDIO_UART_CheckApplicationConfiguration()
 * (declared in rs_stdio_uart.h; this is the one function the driver's own
 * README, Section 4.6, tells applications to call) — exactly once, and
 * caches the result. This is a pure comparison against static/extern data —
 * no hardware side effects — so it's safe to call from any context that can
 * tolerate the fflush() below. If validation fails, that failure is returned
 * on every call (not just the first) since a misconfiguration cannot be
 * fixed by retrying.
 *
 * @retval FSP_SUCCESS  Buffered I/O UART driver is open, validated, and ready.
 * @retval (other)      Validation failed (propagated from
 *                       RS_STDIO_UART_CheckApplicationConfiguration()), or
 *                       fflush() itself failed (FSP_ERR_NOT_OPEN).
 */
fsp_err_t rs_rtc_logger_console_init(void);

/**
 * @brief Non-blocking single-character console read.
 *
 * Peeks the Buffered I/O UART driver's RX byte queue via R_BYTEQ_Used(). Only
 * calls the driver's own getchar() when a byte is already known to be
 * available, so this function NEVER blocks — it returns immediately in both
 * the "character available" and "nothing to read yet" cases.
 *
 * @param[out] p_c  Received character, valid only when this function returns true.
 *
 * @retval true   A character was available and has been copied to *p_c.
 * @retval false  No character was available; *p_c is left unmodified.
 */
bool rs_rtc_logger_console_getc_nb(uint8_t * p_c);

/**
 * @brief Write a NUL-terminated string to the console (non-blocking submit).
 *
 * Queues the string through the Buffered I/O UART driver's stdout stream and
 * triggers a flush. The flush hands the queued bytes to the UART driver and
 * returns — physical transmission completes asynchronously under UART ISR
 * control (UART_EVENT_TX_COMPLETE inside the vendored driver's own callback).
 *
 * Note: if a previous flush's transmission is still physically in flight,
 * the vendored driver's uart_flush() briefly spin-waits for it internally
 * (documented behavior of the driver in its current, unmodified form) before
 * queuing the new data. For typical short CLI/log lines at 115200 baud this
 * window is well under a millisecond and only affects the calling task, never
 * an ISR.
 *
 * @param[in] p_str  NUL-terminated string to send.
 */
void rs_rtc_logger_console_puts(char const * p_str);

#ifdef __cplusplus
}
#endif

#endif /* RS_RTC_LOGGER_STDIO_CONSOLE_H */
