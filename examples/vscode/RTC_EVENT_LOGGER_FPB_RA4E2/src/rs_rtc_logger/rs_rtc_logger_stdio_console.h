/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_stdio_console.h
 * @brief Console adapter — transport-agnostic API, two selectable backends.
 *
 * WHY THIS FILE EXISTS
 * ---------------------
 * All console I/O for the RTC Event Logger demo (log lines, CLI prompts, banner)
 * routes through the four functions declared here.  The active backend is chosen
 * at compile time by RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE (rs_rtc_logger_cfg.h):
 *
 *   0 — Buffered I/O UART (default, unmodified vendored driver).
 *       Implemented in rs_rtc_logger_stdio_console.c.
 *       Uses picolibc stdio (fwrite / fflush / getchar) backed by the
 *       rs_stdio_uart_buffer_io PES.  Non-blocking RX is achieved by peeking
 *       the driver's internal byteq (uart_rx_q_ctrl) before calling getchar().
 *
 *   1 — SEGGER RTT via J-Link debug connection.
 *       Implemented in rs_rtc_logger_rtt_console.c.
 *       Uses SEGGER_RTT_Write (up-buffer 0) and SEGGER_RTT_GetKey (down-buffer 0).
 *       No heap use, no UART hardware dependency, ~648 B RAM total.
 *
 * PUBLIC VS. PRIVATE SURFACE (UART backend notes — backend 0 only)
 * ----------------------------------------------------------------
 * Non-blocking RX peek uses uart_rx_q_ctrl (undeclared, non-static symbol from
 * the vendored driver) — the only mechanism available since the driver's public
 * API has no non-blocking read primitive (README §11.1).
 */

#ifndef RS_RTC_LOGGER_STDIO_CONSOLE_H
#define RS_RTC_LOGGER_STDIO_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_api.h"
#include "rs_rtc_logger_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the active console transport.
 *
 * UART backend (RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0):
 *   Forces deterministic early initialisation of the Buffered I/O UART driver
 *   (normally lazy) and validates g_uart_jlob_vcom's configuration via
 *   RS_STDIO_UART_CheckApplicationConfiguration().  Idempotent.
 *
 * RTT backend (RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 1):
 *   Calls SEGGER_RTT_Init() to set up the control block and channel 0
 *   buffers.  Idempotent — safe to call more than once.
 *
 * @retval FSP_SUCCESS  Transport initialised and ready.
 * @retval (other)      UART validation failed (UART backend only).
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
 * @brief Write raw bytes to the active console transport.
 *
 * UART backend: queues bytes through the Buffered I/O UART driver's stdout
 *   stream (fwrite + fflush).  Physical transmission completes asynchronously
 *   under UART ISR control; the call itself serialises with any in-flight TX.
 * RTT  backend: writes to SEGGER RTT up-buffer 0.  Bytes are trimmed (not
 *   blocked) if the up-buffer is full.
 *
 * @param[in] p_data  Source data (need not be NUL-terminated).
 * @param[in] length  Byte count to write.
 *
 * @retval FSP_SUCCESS           Data submitted.
 * @retval FSP_ERR_WRITE_FAILED  Write failed (UART backend only).
 */
fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length);

/**
 * @brief Write a NUL-terminated string to the active console transport.
 *
 * Convenience wrapper around rs_rtc_logger_console_write(); computes strlen
 * and delegates.  Both backends are handled — see console_write for details.
 *
 * @param[in] p_str  NUL-terminated string to send.
 */
void rs_rtc_logger_console_puts(char const * p_str);

#ifdef __cplusplus
}
#endif

#endif /* RS_RTC_LOGGER_STDIO_CONSOLE_H */
