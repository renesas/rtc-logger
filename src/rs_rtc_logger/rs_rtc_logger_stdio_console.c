/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

#include "rs_rtc_logger_stdio_console.h"

#include <string.h>

static rs_rtc_logger_ctrl_t * gp_console_ctrl = NULL;

void rs_rtc_logger_console_bind_ctrl(rs_rtc_logger_ctrl_t * p_ctrl)
{
    gp_console_ctrl = p_ctrl;
}

/* Internal accessor used by the RTT backend implementation. */
rs_rtc_logger_ctrl_t * rs_rtc_logger_console_bound_ctrl_get(void)
{
    return gp_console_ctrl;
}

#if RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0

fsp_err_t rs_rtc_logger_console_init(void)
{
    if ((NULL == gp_console_ctrl) || (NULL == gp_console_ctrl->p_cfg) || (NULL == gp_console_ctrl->p_cfg->p_uart))
    {
        return FSP_ERR_ASSERTION;
    }

    return FSP_SUCCESS;
}

bool rs_rtc_logger_console_getc_nb(uint8_t * p_c)
{
    FSP_PARAMETER_NOT_USED(p_c);
    return false;
}

fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length)
{
    if ((NULL == p_data) || (0U == length))
    {
        return FSP_SUCCESS;
    }

    if ((NULL == gp_console_ctrl) || (NULL == gp_console_ctrl->p_cfg) || (NULL == gp_console_ctrl->p_cfg->p_uart))
    {
        return FSP_ERR_ASSERTION;
    }

    gp_console_ctrl->tx_complete = false;

    const uart_instance_t * p_uart = gp_console_ctrl->p_cfg->p_uart;
    return p_uart->p_api->write(p_uart->p_ctrl, p_data, length);
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
