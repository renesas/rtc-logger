/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_stdio_console.c
 * @brief UART-backend implementation of the console adapter
 *        (RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0, default).
 *
 * When RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 1 (RTT) the four functions are
 * defined in rs_rtc_logger_rtt_console.c instead.  This translation unit
 * becomes empty in that case — no UART or picolibc stdio symbols are pulled in.
 *
 * See rs_rtc_logger_stdio_console.h for the full design rationale.
 */

#include "rs_rtc_logger_cfg.h"

#if RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0   /* UART backend — compile this file */

#include "rs_rtc_logger_stdio_console.h"

#include <stdio.h>
#include <string.h>

/* Headers from the vendored Buffered I/O UART driver. Not modified — only consumed. */
#include "../rs_stdio_uart_buffer_io/r_byteq.h"
#include "../rs_stdio_uart_buffer_io/rs_stdio_uart.h"

/**
 * uart_rx_q_ctrl is defined (non-static, file scope) in
 * src/rs_stdio_uart_buffer_io/syscalls_picolib_stdio_to_uart.c.
 * Declaring it extern here lets us query the RX queue depth without
 * modifying that file. This is the entire mechanism behind non-blocking RX.
 */
extern byteq_state_ctrl_t uart_rx_q_ctrl;

fsp_err_t rs_rtc_logger_console_init(void)
{
    static bool      has_run           = false;
    static fsp_err_t validation_result = FSP_SUCCESS;

    if (!has_run)
    {
        /* One-time validation using the driver's own documented, public entry
         * point — RS_STDIO_UART_CheckApplicationConfiguration() is the
         * function the driver's README (Section 4.6) tells applications to
         * call.  Result is cached: a misconfiguration cannot be fixed by
         * retrying, and validation compares compile-/link-time data only
         * (no hardware side-effects). */
        validation_result = RS_STDIO_UART_CheckApplicationConfiguration();
        has_run = true;
    }

    if (FSP_SUCCESS != validation_result)
    {
        return validation_result;
    }

    /* fflush() on stdout forces deterministic, early initialization of the
     * vendored driver's lazy jlob_uart_init_once().  Calling it here with
     * nothing queued yet is a harmless way to guarantee the UART is open
     * before any other code runs. */
    if (0 != fflush(stdout))
    {
        return FSP_ERR_NOT_OPEN;
    }

    return FSP_SUCCESS;
}

bool rs_rtc_logger_console_getc_nb(uint8_t * p_c)
{
    if (NULL == p_c)
    {
        return false;
    }

    uint32_t  count = 0U;
    fsp_err_t err    = R_BYTEQ_Used(&uart_rx_q_ctrl, &count);

    if ((FSP_SUCCESS != err) || (0U == count))
    {
        return false;
    }

    /* A byte is already known to be queued, so getchar() resolves immediately
     * and never enters the driver's blocking wait loop. */
    int c = getchar();
    if (c < 0)
    {
        return false;
    }

    *p_c = (uint8_t)c;
    return true;
}

fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length)
{
    if ((NULL == p_data) || (0U == length))
    {
        return FSP_SUCCESS;
    }

    size_t written = fwrite(p_data, sizeof(uint8_t), (size_t)length, stdout);
    int    flushed = fflush(stdout);

    return ((written == (size_t)length) && (0 == flushed)) ? FSP_SUCCESS
                                                           : FSP_ERR_WRITE_FAILED;
}

void rs_rtc_logger_console_puts(char const * p_str)
{
    if (NULL == p_str)
    {
        return;
    }

    size_t len = strlen(p_str);
    if (0U == len)
    {
        return;
    }

    (void)rs_rtc_logger_console_write((uint8_t const *)p_str, (uint32_t)len);
}

#endif /* RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0 */
