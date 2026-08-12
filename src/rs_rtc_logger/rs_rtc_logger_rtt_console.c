/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

#include "rs_rtc_logger_cfg.h"

#if RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 1

#include "rs_rtc_logger_stdio_console.h"

#include "SEGGER_RTT/SEGGER_RTT.h"
#include <string.h>

fsp_err_t rs_rtc_logger_console_init(void)
{
    if (NULL == rs_rtc_logger_console_bound_ctrl_get())
    {
        return FSP_ERR_ASSERTION;
    }

    SEGGER_RTT_Init();
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length)
{
    rs_rtc_logger_ctrl_t * p_ctrl = rs_rtc_logger_console_bound_ctrl_get();

    if ((NULL == p_data) || (0U == length))
    {
        return FSP_SUCCESS;
    }

    if (NULL == p_ctrl)
    {
        return FSP_ERR_ASSERTION;
    }

    (void)SEGGER_RTT_Write(0U, p_data, (unsigned)length);

    /* Mark TX complete immediately — SEGGER_RTT_Write() is synchronous.
     * The consumer task's tx_complete spinloop sees this on its next check
     * and exits without any additional notification needed. */
    p_ctrl->tx_complete = true;

    return FSP_SUCCESS;
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
        return false;
    }

    *p_c = (uint8_t)c;
    return true;
}

#endif /* RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 1 */
