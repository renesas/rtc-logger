/***********************************************************************************************************************
 * PES pre-set configuration source — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_rtc_logger_data_fpb_ra4e2.c
 * @brief Pre-set HAL instance definitions for the RTC Event Logger PES — FPB-RA4E2.
 *
 * Compiled only when RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1 (default).
 *
 * g_rtc0_preset:
 *   Standalone rtc_instance_t — no ra_gen dependency.
 *   Clock source: LOCO.  No error adjustment (p_err_cfg = NULL).
 *   carry_ipl = 2 (required for R_RTC_CalendarTimeSet / Get).
 *   IRQ vectors resolved from vector_data.h via #if defined(VECTOR_NUMBER_...).
 *
 * g_uart0_preset:
 *   Macro alias for g_uart0 (ra_gen/common_data.c).
 *   Defined via #define in rs_rtc_logger_data_fpb_ra4e2.h — no separate
 *   object definition needed here.
 **********************************************************************************************************************/

#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>

#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE

/***********************************************************************************************************************
 * RTC preset instance — FPB-RA4E2 (standalone, no ra_gen dependency)
 **********************************************************************************************************************/

rtc_instance_ctrl_t g_rtc0_ctrl_preset;

static const rtc_cfg_t g_rtc0_cfg_preset =
{
    .clock_source      = RTC_CLOCK_SOURCE_LOCO,
    .freq_compare_value = 255U,           /* Default LOCO frequency comparison value */
    .p_err_cfg         = NULL,            /* No error adjustment */
    .alarm_ipl         = (BSP_IRQ_DISABLED),
    .periodic_ipl      = (BSP_IRQ_DISABLED),
    /* N-12 CRITICAL: carry_ipl MUST be strictly lower (higher preemption) than
     * the IPL of every ISR that calls rs_rtc_logger_EventTrigger().
     * R_RTC_CalendarTimeGet() called from EventTrigger() spins waiting for the
     * carry ISR.  If the calling ISR has higher priority (lower IPL number),
     * the carry ISR can never fire — infinite loop / deadlock.
     * The demo button ISR is at ipl = 5, so carry_ipl must be < 5.
     * FSP-generated hal_data.c uses carry_ipl = 12 — DO NOT USE that value
     * with EventTrigger() in an ISR; use this preset (carry_ipl = 2) instead. */
    .carry_ipl         = (2),
#if defined(VECTOR_NUMBER_RTC_ALARM)
    .alarm_irq         = VECTOR_NUMBER_RTC_ALARM,
#else
    .alarm_irq         = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_RTC_PERIOD)
    .periodic_irq      = VECTOR_NUMBER_RTC_PERIOD,
#else
    .periodic_irq      = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_RTC_CARRY)
    .carry_irq         = VECTOR_NUMBER_RTC_CARRY,
#else
    .carry_irq         = FSP_INVALID_VECTOR,
#endif
    .p_callback        = NULL,
    .p_context         = NULL,
    .p_extend          = NULL,
};

const rtc_instance_t g_rtc0_preset =
{
    .p_ctrl = &g_rtc0_ctrl_preset,
    .p_cfg  = &g_rtc0_cfg_preset,
    .p_api  = &g_rtc_on_rtc,
};

/*
 * g_uart0_preset is a macro alias for the FSP-generated g_uart0 (common_data.c).
 * The alias is defined in rs_rtc_logger_data_fpb_ra4e2.h — nothing to define here.
 */

/*
 * Note: g_rtc0_cfg_preset.carry_ipl (= 2, above) is validated by
 * rs_rtc_logger_ConfigCheck() / rs_rtc_logger_hal_rtc_open() against
 * RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT (default 4) — see rs_rtc_logger.h.
 */

#endif /* RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */
