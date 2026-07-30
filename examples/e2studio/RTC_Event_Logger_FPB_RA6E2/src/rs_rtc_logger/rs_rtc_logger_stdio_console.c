/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_stdio_console.c
 * @brief See rs_rtc_logger_stdio_console.h for design rationale.
 */

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
         * point for this purpose — RS_STDIO_UART_CheckApplicationConfiguration()
         * is declared in rs_stdio_uart.h and is the one function the driver's
         * own README (Section 4.6 / "Step 7: Validate UART Configuration")
         * tells applications to call. It builds its check struct from the
         * real, live g_uart_jlob_vcom/g_uart_jlob_vcom_cfg/etc. externs (see
         * stdio_uart_config_ra6e2_fpb.c), so this is a real, meaningful
         * comparison against the RA6E2 FPB reference — not a self-check of
         * the driver's internal reference table.
         *
         * (rs_stdio_config_gen(), used by the driver's own reference demo, was
         * deliberately NOT adopted here: it is not declared in rs_stdio_uart.h
         * at all — the demo only reaches it via an ad-hoc inline `extern` —
         * and it only validates the driver's static reference table against
         * itself, never touching the real g_uart_jlob_vcom instance. Calling
         * only the genuinely public, genuinely meaningful check keeps this
         * adapter on the driver's documented public surface wherever possible.)
         *
         * This is a pure comparison against static/extern data — no hardware
         * access — so it's safe to call from any context that can tolerate
         * the fflush() below.
         *
         * STRICT: unlike the vendored driver's own reference demo, which spins
         * forever on failure (while (FSP_SUCCESS != err);), this function
         * reports the failure to its caller instead. rs_rtc_logger_hal_uart_open()
         * propagates it, rs_rtc_logger_Open() rolls back and propagates it
         * further, and the demo's existing Open()-failure path already halts
         * with a diagnostic — so the failure surfaces the same way any other
         * Open() failure does, without a separate infinite spin-wait here.
         *
         * Cached: the result of this check cannot change without a rebuild
         * (it's comparing compile-time/link-time data), so it's computed
         * once and the same fsp_err_t is returned on every subsequent call —
         * including every retry of rs_rtc_logger_Open() (e.g. Example mode's
         * auto-init retry loop), since a misconfiguration is not something a
         * retry can fix. */
        validation_result = RS_STDIO_UART_CheckApplicationConfiguration();

        has_run = true;
    }

    if (FSP_SUCCESS != validation_result)
    {
        return validation_result;
    }

    /* fflush() on stdout is one of the three lazy-init triggers documented by
     * the driver (printf / getchar / fflush all call jlob_uart_init_once()
     * internally). Calling it here with nothing queued yet is a harmless
     * way to force deterministic, early initialization. */
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
        /* Nothing queued yet (or the driver has not been initialized/opened by
         * a prior stdio call) — return immediately without blocking. */
        return false;
    }

    /* A byte is already known to be queued, so this getchar() call resolves
     * immediately and never enters the driver's blocking wait loop. */
    int c = getchar();

    if (c < 0)
    {
        return false;
    }

    *p_c = (uint8_t)c;
    return true;
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

    (void)fwrite(p_str, sizeof(char), len, stdout);

    /* Explicit flush: fwrite() alone only queues bytes into the driver's TX
     * byteq. fflush() is what submits the queued bytes to the UART driver
     * (non-blocking submit; physical transmission finishes under ISR). */
    (void)fflush(stdout);
}
