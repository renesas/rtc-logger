/***********************************************************************************************************************
 * PES pre-set configuration header — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_rtc_logger_data_fpb_ra4e2.h
 * @brief Pre-set HAL instance declarations for the RTC Event Logger PES — FPB-RA4E2.
 *
 * Included when RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1 (default).
 *
 * Hardware mapping (FPB-RA4E2):
 *   RTC  : On-chip, LOCO clock source, no error adjustment
 *   UART : SCI0 — USB-Serial VCOM bridge, 115200 baud, async
 *   IRQ  : P304 / IRQ9 / S1 (application-owned r_icu)
 *
 * N-12 — CRITICAL carry_ipl requirement:
 *   The RTC carry interrupt (carry_ipl = 2 in the preset .c file) MUST have
 *   HIGHER preemption (lower IPL number) than every ISR that calls
 *   rs_rtc_logger_EventTrigger().  R_RTC_CalendarTimeGet() spins waiting for
 *   the carry ISR; if the calling ISR has higher priority the carry ISR can
 *   never fire → infinite loop / system hang.
 *   FSP-generated hal_data.c uses carry_ipl = 12 — DO NOT use that value.
 *   When creating a board-specific data file, mirror the carry_ipl = 2 here,
 *   or set it to any value strictly less than the IPL of every EventTrigger()
 *   caller.  rs_rtc_logger_Open() enforces RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT
 *   (default 4) at runtime and returns FSP_ERR_INVALID_HW_CONDITION if the
 *   limit is exceeded.
 *
 * Usage:
 *   #if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
 *   #include "rs_rtc_logger_data_fpb_ra4e2.h"
 *   #endif
 *   ...
 *   rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
 *   rs_rtc_logger_ConfigCheck(&g_logger_cfg);      // optional: validate before Open()
 *   rs_rtc_logger_ConfigGetSize(&g_logger_ctrl, &sz); // optional: query control-block size
 **********************************************************************************************************************/

#ifndef RS_RTC_LOGGER_DATA_FPB_RA4E2_H
#define RS_RTC_LOGGER_DATA_FPB_RA4E2_H

#include <rs_rtc_logger/rs_rtc_logger_cfg.h>

#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE

#include "r_rtc.h"
#include "r_rtc_api.h"
#include "r_uart_api.h"

FSP_HEADER

/***********************************************************************************************************************
 * RTC preset instance — standalone, no ra_gen dependency.
 **********************************************************************************************************************/
extern rtc_instance_ctrl_t g_rtc0_ctrl_preset;
extern const rtc_instance_t g_rtc0_preset;

/***********************************************************************************************************************
 * UART preset instance — macro alias for FSP-generated g_uart0 (ra_gen/common_data.c).
 * SCI0, 115200 baud, async, TX callback = rs_rtc_logger_uart_callback.
 **********************************************************************************************************************/
extern const uart_instance_t g_uart0;
#define g_uart0_preset  g_uart0

FSP_FOOTER

#endif /* RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */

#endif /* RS_RTC_LOGGER_DATA_FPB_RA4E2_H */
