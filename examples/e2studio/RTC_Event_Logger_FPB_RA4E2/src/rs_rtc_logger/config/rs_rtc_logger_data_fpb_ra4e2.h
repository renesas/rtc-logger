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
 * g_uart0_preset is a standalone r_sci_uart instance — NOT an alias for the
 * FSP-generated g_uart0. Its register-level baud settings (brr/mddr/bgdm/
 * cks/abcs/abcse) are the exact values RASC computes for 115200 baud on
 * FPB-RA4E2's PCLK (captured from a known-good ra_gen/hal_data.c and baked
 * in below), so g_uart0_preset always runs at 115200 regardless of whatever
 * baud rate (or other SCI settings) the Stack Configurator currently has for
 * g_uart0. This mirrors how g_rtc0_preset is already independent of whatever
 * the Stack Configurator has for g_rtc0.
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
 * Preset UART's early-open handoff:
 *   demo_banner_setup() (blinky_thread_entry.c) and demo_uart_write_blocking()
 *   (rs_rtc_logger_demo.c) must open/write through g_uart0_preset (not
 *   g_uart0) whenever RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1, or the
 *   pre-demo banner and CLI prompts would keep using the Stack Configurator's
 *   (possibly broken) g_uart0 settings while the logger itself uses this
 *   preset — a silent baud-rate mismatch between demo output and logged
 *   events. See DEMO_UART_INSTANCE in those two files.
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

#include <stddef.h>                       /* size_t (rs_rtc_logger_ConfigGetSize) */
#include <rs_rtc_logger/rs_rtc_logger_cfg.h>
#include <rs_rtc_logger/rs_rtc_logger_types.h> /* rs_rtc_logger_cfg_t, rs_rtc_logger_ctrl_t,
                                                 * rtc_instance_t, uart_instance_t */

#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE

#include "r_rtc.h"
#include "r_rtc_api.h"
#include "r_uart_api.h"
#include "r_sci_uart.h"

FSP_HEADER

/***********************************************************************************************************************
 * RTC preset instance — standalone, no ra_gen dependency.
 **********************************************************************************************************************/
extern rtc_instance_ctrl_t g_rtc0_ctrl_preset;
extern const rtc_instance_t g_rtc0_preset;

/***********************************************************************************************************************
 * UART preset instance — standalone r_sci_uart instance, SCI0, fixed 115200
 * baud (register values captured from a known-good FSP-generated config;
 * see rs_rtc_logger_data_fpb_ra4e2.c). Independent of the Stack
 * Configurator's current g_uart0 settings.
 **********************************************************************************************************************/
extern sci_uart_instance_ctrl_t g_uart0_ctrl_preset;
extern const uart_instance_t g_uart0_preset;

FSP_FOOTER

#endif /* RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */

/***********************************************************************************************************************
 * PES configuration bridge functions — FPB-RA4E2
 *
 * Board-specific ConfigGen/ConfigCheck/
 * ConfigGetSize live alongside the *_preset instance data they are most
 * commonly paired with. Declared and defined UNCONDITIONALLY (outside the
 * RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE guard above) so the same three
 * functions remain available whether the caller binds *_preset instances or
 * FSP Stack Configurator-generated instances (g_rtc0 / g_uart0).
 **********************************************************************************************************************/

FSP_HEADER

/**
 * @brief Assign HAL instance pointers to the PES configuration structure.
 *
 * Called once during application initialisation, before rs_rtc_logger_Open().
 * Accepts either pre-set instances (g_rtc0_preset / g_uart0_preset, above,
 * when RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1) or FSP Stack Configurator-
 * generated instances (RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 0).
 *
 * Non-instance fields (ts_format, ts_brackets, line_term, overflow_policy,
 * p_callback, p_context) are already set in p_cfg by the caller and are not
 * modified by this function.
 *
 * @param[in,out] p_cfg   PES configuration structure. p_rtc and p_uart are written.
 * @param[in]     p_rtc   RTC HAL instance pointer (preset or FSP-generated).
 * @param[in]     p_uart  UART HAL instance pointer (preset or FSP-generated).
 *
 * @retval FSP_SUCCESS        Instance pointers assigned.
 * @retval FSP_ERR_ASSERTION  p_cfg, p_rtc, or p_uart is NULL.
 *
 * @note Valid in any PES lifecycle state. Not ISR-safe.
 */
fsp_err_t rs_rtc_logger_ConfigGen(rs_rtc_logger_cfg_t   * p_cfg,
                                    rtc_instance_t  const * p_rtc,
                                    uart_instance_t const * p_uart);

/**
 * @brief Validate a PES configuration structure before rs_rtc_logger_Open().
 *
 * Verifies that the required RTC and UART HAL instances (and their
 * configuration blocks) are present, that the RTC carry interrupt
 * priority satisfies the N-12 constraint above, and that the enumerated
 * configuration fields (ts_format, line_term, overflow_policy) are in
 * range. Call any time after rs_rtc_logger_ConfigGen() and before
 * rs_rtc_logger_Open(), so misconfiguration is caught early.
 *
 * @param[in] p_cfg  Configuration structure populated by rs_rtc_logger_ConfigGen().
 *
 * @retval FSP_SUCCESS                   Configuration is valid.
 * @retval FSP_ERR_ASSERTION             p_cfg is NULL.
 * @retval FSP_ERR_NOT_INITIALIZED       Required RTC or UART instance/config is missing.
 * @retval FSP_ERR_INVALID_HW_CONDITION  RTC carry_ipl exceeds RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT.
 * @retval FSP_ERR_INVALID_ARGUMENT      ts_format, line_term, or overflow_policy is out of range.
 *
 * @note Valid in any PES lifecycle state. Not ISR-safe.
 */
fsp_err_t rs_rtc_logger_ConfigCheck(rs_rtc_logger_cfg_t const * p_cfg);

/**
 * @brief Get the size, in bytes, of the PES control block structure.
 *
 * Useful for confirming control-block sizing (e.g. against
 * RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES) without requiring
 * rs_rtc_logger_Open() to have been called first.
 *
 * @param[in]  p_ctrl  Pointer to a PES control block (may be caller-allocated,
 *                     not-yet-Open()'d storage — only used for sizeof(*p_ctrl)).
 * @param[out] p_size  Populated with sizeof(rs_rtc_logger_ctrl_t).
 *
 * @retval FSP_SUCCESS       Size returned in *p_size.
 * @retval FSP_ERR_ASSERTION p_ctrl or p_size is NULL.
 *
 * @note Valid in any PES lifecycle state. Not ISR-safe.
 */
fsp_err_t rs_rtc_logger_ConfigGetSize(rs_rtc_logger_ctrl_t const * p_ctrl,
                                      size_t                      * p_size);

FSP_FOOTER

#endif /* RS_RTC_LOGGER_DATA_FPB_RA4E2_H */
