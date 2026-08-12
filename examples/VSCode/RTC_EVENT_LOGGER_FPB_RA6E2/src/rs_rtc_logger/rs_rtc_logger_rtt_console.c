/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * rs_rtc_logger_rtt_console.c — RTT backend for the RTC Event Logger console adapter.
 *
 * This file is compiled only when RS_RTC_LOGGER_CFG_CONSOLE_BACKEND == 1.
 * At backend 0 (UART default) this translation unit is empty: the three console
 * functions are defined in rs_rtc_logger_stdio_console.c instead.
 *
 * Transport mapping
 * -----------------
 *   rs_rtc_logger_console_init()   → SEGGER_RTT_Init()         (idempotent)
 *   rs_rtc_logger_console_write()  → SEGGER_RTT_Write(0, ...)   (log + CLI TX)
 *   rs_rtc_logger_console_puts()   → console_write(strlen)      (string helper)
 *   rs_rtc_logger_console_getc_nb()→ SEGGER_RTT_HasKey/GetKey() (CLI RX)
 *
 * Thread-safety
 * -------------
 * All four functions are called from FreeRTOS task context only (never from ISR).
 * SEGGER_RTT_Write() and SEGGER_RTT_GetKey() protect their ring-buffer pointers
 * with FreeRTOS critical sections (configured in SEGGER_RTT_Conf.h).
 * SEGGER_RTT_HasKey() performs an atomic 32-bit read of WrOff — safe without
 * locking on Cortex-M33 per the ARMv8-M architecture reference.
 **********************************************************************************************************************/

#include "rs_rtc_logger_cfg.h"

#if RS_RTC_LOGGER_CFG_CONSOLE_BACKEND == 1   /* RTT backend — compile this file */

#include "rs_rtc_logger_stdio_console.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────── */

fsp_err_t rs_rtc_logger_console_init(void)
{
    /* SEGGER_RTT_Init() is idempotent — subsequent calls return immediately. */
    SEGGER_RTT_Init();
    return FSP_SUCCESS;
}

/* ──────────────────────────────────────────────────────────────────────────── */

fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length)
{
    if ((NULL == p_data) || (0U == length))
    {
        return FSP_SUCCESS;
    }

    /* SEGGER_RTT_Write trims silently when the up-buffer is full
     * (SEGGER_RTT_MODE_NO_BLOCK_TRIM, configured in SEGGER_RTT_Conf.h).
     * The return value (bytes actually written) is intentionally ignored here:
     * losing a few bytes of a log burst when the host is not draining the
     * buffer fast enough is preferable to blocking the consumer task. */
    (void)SEGGER_RTT_Write(0U, p_data, (unsigned)length);

    return FSP_SUCCESS;
}

/* ──────────────────────────────────────────────────────────────────────────── */

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

/* ──────────────────────────────────────────────────────────────────────────── */

bool rs_rtc_logger_console_getc_nb(uint8_t * p_c)
{
    if (NULL == p_c)
    {
        return false;
    }

    if (!SEGGER_RTT_HasKey())
    {
        return false;
    }

    int c = SEGGER_RTT_GetKey();
    if (c < 0)
    {
        /* Defensive: HasKey() returned true but GetKey() found nothing
         * (should not happen in practice). */
        return false;
    }

    *p_c = (uint8_t)c;
    return true;
}

#endif /* RS_RTC_LOGGER_CFG_CONSOLE_BACKEND == 1 */
