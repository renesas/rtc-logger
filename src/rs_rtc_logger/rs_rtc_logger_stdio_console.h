/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_stdio_console.h
 * @brief Console adapter for the reusable RTC Event Logger core.
 *
 * Backend selection is compile-time only:
 *   RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 0 -> UART HAL write path (default)
 *   RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE == 1 -> SEGGER RTT
 */

#ifndef RS_RTC_LOGGER_STDIO_CONSOLE_H
#define RS_RTC_LOGGER_STDIO_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_api.h"
#include "rs_rtc_logger_cfg.h"
#include "rs_rtc_logger_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bind the logger control block used by backend implementations.
 *
 * Must be called before rs_rtc_logger_console_init()/write().
 */
void rs_rtc_logger_console_bind_ctrl(rs_rtc_logger_ctrl_t * p_ctrl);

/* Internal helper used by backend implementations. */
rs_rtc_logger_ctrl_t * rs_rtc_logger_console_bound_ctrl_get(void);

/**
 * @brief Initialize the selected console backend.
 */
fsp_err_t rs_rtc_logger_console_init(void);

/**
 * @brief Best-effort non-blocking char read (used by CLI paths).
 */
bool rs_rtc_logger_console_getc_nb(uint8_t * p_c);

/**
 * @brief Write bytes through the selected backend.
 */
fsp_err_t rs_rtc_logger_console_write(uint8_t const * p_data, uint32_t length);

/**
 * @brief Write a NUL-terminated string through the selected backend.
 */
void rs_rtc_logger_console_puts(char const * p_str);

#ifdef __cplusplus
}
#endif

#endif /* RS_RTC_LOGGER_STDIO_CONSOLE_H */
