/***********************************************************************************************************************
 * Pre-set configuration source — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_rtc_logger_data_fpb_ra6e2.c
 * @brief Pre-set HAL instance definitions for the RTC Event Logger — FPB-RA6E2.
 *
 * g_rtc0_preset (compiled only when RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1, default):
 *   Standalone rtc_instance_t — no ra_gen dependency.
 *   Clock source: LOCO.  No error adjustment (p_err_cfg = NULL).
 *   carry_ipl = 2 (required for R_RTC_CalendarTimeSet / Get).
 *   IRQ vectors resolved from vector_data.h via #if defined(VECTOR_NUMBER_...).
 *
 * There is no UART preset in this file: rs_rtc_logger_hal_uart_open/_close/_write()
 * do not open or write a uart_instance_t directly — the physical UART is
 * owned exclusively by the vendored Buffered I/O UART driver, bound to
 * g_uart_jlob_vcom. rs_rtc_logger_ConfigGen() takes only an RTC HAL instance
 * pointer.
 **********************************************************************************************************************/

#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.h>
#include <string.h>     /* memset, memcpy (rs_rtc_logger_ConfigGen) */

#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE

/***********************************************************************************************************************
 * RTC preset instance — FPB-RA6E2 (standalone, no ra_gen dependency)
 **********************************************************************************************************************/

rtc_instance_ctrl_t g_rtc0_ctrl_preset;

static const rtc_cfg_t g_rtc0_cfg_preset =
{
    .clock_source      = RTC_CLOCK_SOURCE_LOCO,
    .freq_compare_value = 255U,           /* Default LOCO frequency comparison value */
    .p_err_cfg         = NULL,            /* No error adjustment */
    .alarm_ipl         = (BSP_IRQ_DISABLED),
    .periodic_ipl      = (BSP_IRQ_DISABLED),
    /* Carry_ipl MUST be strictly lower (higher preemption) than
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
 * Note: g_rtc0_cfg_preset.carry_ipl (= 2, above) is validated by
 * rs_rtc_logger_ConfigCheck() / rs_rtc_logger_hal_rtc_open() against
 * RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT (default 4) — see rs_rtc_logger_ConfigCheck() below.
 */

#endif /* RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */

/***********************************************************************************************************************
 * There is no UART preset in this file. The physical UART is opened exactly
 * once by the Buffered I/O UART driver, using whatever the FSP Stack
 * Configurator has set on g_uart_jlob_vcom, and that driver validates its
 * own configuration via RS_STDIO_UART_CheckApplicationConfiguration()
 * (see rs_rtc_logger_console_init()). rs_rtc_logger_ConfigGen() only takes
 * an RTC HAL instance pointer; rs_rtc_logger_ConfigCheck()'s hardware
 * validation is likewise scoped to the RTC instance.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Param-checking macro (mirrors rs_rtc_logger.c convention; scoped to this file)
 **********************************************************************************************************************/

#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
 #define RS_RTC_LOGGER_DATA_ASSERT_RETURN(expr, err)  do { if (!(expr)) { return (err); } } while (0)
#else
 #define RS_RTC_LOGGER_DATA_ASSERT_RETURN(expr, err)  ((void)0)
#endif

/***********************************************************************************************************************
 * RTC Event Logger configuration bridge functions — FPB-RA6E2
 *
 * Compiled unconditionally so both the preset path and the FSP Stack
 * Configurator path can call these functions.
 **********************************************************************************************************************/

/*****************************************************************************************************************
 * @brief       Assign the RTC HAL instance pointer to the RTC Event Logger configuration structure.
 * @param[in]   p_cfg   RTC Event Logger configuration structure. p_rtc is written.
 * @param[in]   p_rtc   RTC HAL instance pointer (preset or FSP-generated).
 * @retval      FSP_SUCCESS       Instance pointer assigned.
 * @retval      FSP_ERR_ASSERTION p_cfg or p_rtc is NULL.
 ****************************************************************************************************************/
fsp_err_t rs_rtc_logger_ConfigGen (rs_rtc_logger_cfg_t   * p_cfg,
                                     rtc_instance_t  const * p_rtc)
{
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_cfg,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_rtc,  FSP_ERR_ASSERTION);
#endif

    /* Clear the destination HAL instance-pointer field before binding. */
    memset(&p_cfg->p_rtc,  0, sizeof(p_cfg->p_rtc));

    /* Bind the caller-supplied HAL instance pointer into the configuration. */
    memcpy(&p_cfg->p_rtc,  &p_rtc,  sizeof(p_cfg->p_rtc));

    return FSP_SUCCESS;
}

/*****************************************************************************************************************
 * @brief       Validate a RTC Event Logger configuration structure before rs_rtc_logger_Open().
 * @param[in]   p_cfg   Configuration structure populated by rs_rtc_logger_ConfigGen().
 * @retval      FSP_SUCCESS                   Configuration is valid.
 * @retval      FSP_ERR_ASSERTION             p_cfg is NULL.
 * @retval      FSP_ERR_NOT_INITIALIZED       Required RTC instance/config is missing.
 * @retval      FSP_ERR_INVALID_HW_CONDITION  RTC carry_ipl/carry_irq is disabled or has no NVIC
 *                                            vector allocated (see rs_rtc_logger_data_fpb_ra6e2.h —
 *                                            Onkar's validation-test finding: RASC removes the
 *                                            vector-table entry entirely when the r_rtc stack is
 *                                            deleted, even with the component left enabled).
 * @retval      FSP_ERR_INVALID_ARGUMENT      ts_format, line_term, or overflow_policy is out of range.
 *
 * This function validates the RTC instance only. UART configuration is
 * validated independently by the Buffered I/O UART driver's own
 * RS_STDIO_UART_CheckApplicationConfiguration() (wired in through
 * rs_rtc_logger_console_init(), called from rs_rtc_logger_hal_uart_open()),
 * which performs a full comparison of pins, baud registers, extended
 * config, and vectors/IPLs against the RA6E2 FPB reference.
 ****************************************************************************************************************/
fsp_err_t rs_rtc_logger_ConfigCheck (rs_rtc_logger_cfg_t const * p_cfg)
{
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_cfg, FSP_ERR_ASSERTION);
#endif

    /* Required RTC instance must exist and carry a configuration block. */
    if ((NULL == p_cfg->p_rtc) || (NULL == p_cfg->p_rtc->p_cfg))
    {
        return FSP_ERR_NOT_INITIALIZED;
    }

    /* The RTC carry interrupt
     * must (a) have a real NVIC vector allocated by the FSP Stack
     * Configurator, and (b) run at a priority strictly below every ISR that
     * calls rs_rtc_logger_EventTrigger(). If the r_rtc "stack" has been
     * removed from configuration.xml (module instance deleted, component
     * left enabled), RASC deletes the vector-table entry / ISR trampoline
     * for RTC_CARRY entirely — no ConfigGen()-produced value can restore a
     * vector slot RASC never allocated (see PES_Validation_Test_Findings).
     * Checking for FSP_INVALID_VECTOR / BSP_IRQ_DISABLED here converts that
     * silent hang (inside R_RTC_CalendarTimeGet()) into a clear, immediate
     * ConfigCheck() failure instead. */
    if ((FSP_INVALID_VECTOR == p_cfg->p_rtc->p_cfg->carry_irq) ||
        (BSP_IRQ_DISABLED   == (uint32_t) p_cfg->p_rtc->p_cfg->carry_ipl) ||
        ((uint32_t) p_cfg->p_rtc->p_cfg->carry_ipl > RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT))
    {
        return FSP_ERR_INVALID_HW_CONDITION;
    }

    /* Enumerated configuration fields must hold in-range values. */
    if (p_cfg->ts_format > RS_RTC_LOGGER_TS_FORMAT_SYSLOG)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (p_cfg->line_term > RS_RTC_LOGGER_TERM_CR)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (p_cfg->overflow_policy > RS_RTC_LOGGER_OVERFLOW_DROP_NEWEST)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    return FSP_SUCCESS;
}

/*****************************************************************************************************************
 * @brief       Get the size, in bytes, of the RTC Event Logger control block structure.
 * @param[in]   p_ctrl  Pointer to a RTC Event Logger control block (used only for sizeof(*p_ctrl)).
 * @param[out]  p_size  Populated with sizeof(rs_rtc_logger_ctrl_t).
 * @retval      FSP_SUCCESS       Size returned in *p_size.
 * @retval      FSP_ERR_ASSERTION p_ctrl or p_size is NULL.
 ****************************************************************************************************************/
fsp_err_t rs_rtc_logger_ConfigGetSize (rs_rtc_logger_ctrl_t const * p_ctrl, size_t * p_size)
{
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_size, FSP_ERR_ASSERTION);
#endif

    *p_size = sizeof(*p_ctrl);

    return FSP_SUCCESS;
}
