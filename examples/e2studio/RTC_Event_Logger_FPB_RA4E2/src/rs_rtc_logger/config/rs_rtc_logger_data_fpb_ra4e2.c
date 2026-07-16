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
 *   Standalone uart_instance_t (r_sci_uart / SCI0) — no ra_gen dependency.
 *   Fixed 115200 baud: the baud_setting_t register values below (brr=53,
 *   mddr=256/disabled, bgdm=1, cks=0, abcs=0, abcse=0, brme=false) are the
 *   exact values RASC computes for 115200 baud at FPB-RA4E2's PCLK — captured
 *   from a known-good ra_gen/hal_data.c. This makes g_uart0_preset run at
 *   115200 regardless of whatever the Stack Configurator's g_uart0 is
 *   currently set to. IRQ vectors resolved from vector_data.h the same way
 *   as the RTC preset. Callback = rs_rtc_logger_uart_callback (PES-owned,
 *   declared in rs_rtc_logger.h).
 **********************************************************************************************************************/

#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>
#include <rs_rtc_logger/rs_rtc_logger.h> /* rs_rtc_logger_uart_callback prototype */
#include <string.h>     /* memset, memcpy (rs_rtc_logger_ConfigGen) */

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
 * Note: g_rtc0_cfg_preset.carry_ipl (= 2, above) is validated by
 * rs_rtc_logger_ConfigCheck() / rs_rtc_logger_hal_rtc_open() against
 * RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT (default 4) — see rs_rtc_logger_ConfigCheck() below.
 */

/***********************************************************************************************************************
 * UART preset instance — FPB-RA4E2 (standalone, no ra_gen dependency)
 *
 * Fixed 115200 baud. Register values below were captured from a known-good
 * FSP-generated ra_gen/hal_data.c (RASC's own 115200 baud calculation for
 * this board's PCLK) and are baked in here so they no longer depend on
 * whatever the Stack Configurator currently has configured for g_uart0.
 **********************************************************************************************************************/

sci_uart_instance_ctrl_t g_uart0_ctrl_preset;

/* 115200 baud @ FPB-RA4E2 PCLK — 0.469% error (same figure RASC reports). */
static baud_setting_t g_uart0_baud_setting_preset =
{
    .semr_baudrate_bits_b.abcse = 0,
    .semr_baudrate_bits_b.abcs  = 0,
    .semr_baudrate_bits_b.bgdm  = 1,
    .cks                        = 0,
    .brr                        = 53,
    .mddr                       = (uint8_t) 256,  /* 256 == disabled (no MDDR modulation) */
    .semr_baudrate_bits_b.brme  = false,
};

static const sci_uart_extended_cfg_t g_uart0_cfg_extend_preset =
{
    .clock            = SCI_UART_CLOCK_INT,
    .rx_edge_start    = SCI_UART_START_BIT_FALLING_EDGE,
    .noise_cancel     = SCI_UART_NOISE_CANCELLATION_DISABLE,
    .rx_fifo_trigger  = SCI_UART_RX_FIFO_TRIGGER_MAX,
    .p_baud_setting   = &g_uart0_baud_setting_preset,
    .flow_control     = SCI_UART_FLOW_CONTROL_RTS,     /* SCI-internal RTS pin; no external flow control pin used */
    .flow_control_pin = (bsp_io_port_pin_t) UINT16_MAX, /* No dedicated flow-control GPIO */
    .rs485_setting    =
    {
        .enable         = SCI_UART_RS485_DISABLE,
        .polarity       = SCI_UART_RS485_DE_POLARITY_HIGH,
        .de_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
    },
    .irda_setting     =
    {
        .ircr_bits_b.ire     = 0,
        .ircr_bits_b.irrxinv = 0,
        .ircr_bits_b.irtxinv = 0,
    },
};

static const uart_cfg_t g_uart0_cfg_preset =
{
    .channel      = 0,
    .data_bits    = UART_DATA_BITS_8,
    .parity       = UART_PARITY_OFF,
    .stop_bits    = UART_STOP_BITS_1,
    .p_callback   = rs_rtc_logger_uart_callback,   /* PES-owned callback (rs_rtc_logger.h) */
    .p_context    = NULL,
    .p_extend     = &g_uart0_cfg_extend_preset,
    .p_transfer_tx = NULL,     /* Interrupt-driven; no DTC/DMAC transfer instance */
    .p_transfer_rx = NULL,
    /* Same working IPLs as the FSP-generated config (12) — independent
     * namespace from the RTC's carry_ipl (2) / N-12 constraint; no ordering
     * requirement versus the button ISR (5) since the UART ISRs never call
     * rs_rtc_logger_EventTrigger(). Keep below BSP_IRQ_DISABLED if changed. */
    .rxi_ipl      = (12),
    .txi_ipl      = (12),
    .tei_ipl      = (12),
    .eri_ipl      = (12),
#if defined(VECTOR_NUMBER_SCI0_RXI)
    .rxi_irq      = VECTOR_NUMBER_SCI0_RXI,
#else
    .rxi_irq      = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI0_TXI)
    .txi_irq      = VECTOR_NUMBER_SCI0_TXI,
#else
    .txi_irq      = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI0_TEI)
    .tei_irq      = VECTOR_NUMBER_SCI0_TEI,
#else
    .tei_irq      = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_SCI0_ERI)
    .eri_irq      = VECTOR_NUMBER_SCI0_ERI,
#else
    .eri_irq      = FSP_INVALID_VECTOR,
#endif
};

const uart_instance_t g_uart0_preset =
{
    .p_ctrl = &g_uart0_ctrl_preset,
    .p_cfg  = &g_uart0_cfg_preset,
    .p_api  = &g_uart_on_sci,
};

#endif /* RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */

/***********************************************************************************************************************
 * Param-checking macro (mirrors rs_rtc_logger.c convention; scoped to this file)
 **********************************************************************************************************************/

#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
 #define RS_RTC_LOGGER_DATA_ASSERT_RETURN(expr, err)  do { if (!(expr)) { return (err); } } while (0)
#else
 #define RS_RTC_LOGGER_DATA_ASSERT_RETURN(expr, err)  ((void)0)
#endif

/***********************************************************************************************************************
 * PES configuration bridge functions — FPB-RA4E2
 *
 * Compiled unconditionally so both the preset path and the FSP Stack
 * Configurator path can call these functions.
 **********************************************************************************************************************/

/*****************************************************************************************************************
 * @brief       Assign HAL instance pointers to the PES configuration structure.
 * @param[in]   p_cfg   PES configuration structure. p_rtc and p_uart are written.
 * @param[in]   p_rtc   RTC HAL instance pointer (preset or FSP-generated).
 * @param[in]   p_uart  UART HAL instance pointer (preset or FSP-generated).
 * @retval      FSP_SUCCESS       Instance pointers assigned.
 * @retval      FSP_ERR_ASSERTION p_cfg, p_rtc, or p_uart is NULL.
 ****************************************************************************************************************/
fsp_err_t rs_rtc_logger_ConfigGen (rs_rtc_logger_cfg_t   * p_cfg,
                                     rtc_instance_t  const * p_rtc,
                                     uart_instance_t const * p_uart)
{
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_cfg,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_rtc,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_DATA_ASSERT_RETURN(NULL != p_uart, FSP_ERR_ASSERTION);
#endif

    /* Clear the destination HAL instance-pointer fields before binding. */
    memset(&p_cfg->p_rtc,  0, sizeof(p_cfg->p_rtc));
    memset(&p_cfg->p_uart, 0, sizeof(p_cfg->p_uart));

    /* Bind the caller-supplied HAL instance pointers into the configuration. */
    memcpy(&p_cfg->p_rtc,  &p_rtc,  sizeof(p_cfg->p_rtc));
    memcpy(&p_cfg->p_uart, &p_uart, sizeof(p_cfg->p_uart));

    return FSP_SUCCESS;
}

/*****************************************************************************************************************
 * @brief       Validate a PES configuration structure before rs_rtc_logger_Open().
 * @param[in]   p_cfg   Configuration structure populated by rs_rtc_logger_ConfigGen().
 * @retval      FSP_SUCCESS                   Configuration is valid.
 * @retval      FSP_ERR_ASSERTION             p_cfg is NULL.
 * @retval      FSP_ERR_NOT_INITIALIZED       Required RTC or UART instance/config is missing.
 * @retval      FSP_ERR_INVALID_HW_CONDITION  RTC carry_ipl/carry_irq, or a UART rxi/txi/tei/eri
 *                                            ipl/irq, is disabled or has no NVIC vector allocated
 *                                            (see rs_rtc_logger_data_fpb_ra4e2.h — Onkar's
 *                                            validation-test finding: RASC removes the vector-table
 *                                            entry entirely when the r_rtc/r_sci_uart stack is
 *                                            deleted, even with the component left enabled).
 * @retval      FSP_ERR_INVALID_ARGUMENT      ts_format, line_term, or overflow_policy is out of range.
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

    /* Required UART instance must exist and carry a configuration block. */
    if ((NULL == p_cfg->p_uart) || (NULL == p_cfg->p_uart->p_cfg))
    {
        return FSP_ERR_NOT_INITIALIZED;
    }

    /* N-12 + hardening (per Onkar's validation-test review): the RTC carry
     * interrupt must (a) have a real NVIC vector allocated by the FSP Stack
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

    /* Same rationale, applied to the UART RXI/TXI/TEI/ERI interrupts: if the
     * r_sci_uart stack has been removed, these fall back to
     * FSP_INVALID_VECTOR / BSP_IRQ_DISABLED, and TX would silently stall
     * after the first byte because no TX-complete interrupt can ever fire. */
    if ((FSP_INVALID_VECTOR == p_cfg->p_uart->p_cfg->rxi_irq) ||
        (FSP_INVALID_VECTOR == p_cfg->p_uart->p_cfg->txi_irq) ||
        (FSP_INVALID_VECTOR == p_cfg->p_uart->p_cfg->tei_irq) ||
        (FSP_INVALID_VECTOR == p_cfg->p_uart->p_cfg->eri_irq) ||
        (BSP_IRQ_DISABLED   == (uint32_t) p_cfg->p_uart->p_cfg->rxi_ipl) ||
        (BSP_IRQ_DISABLED   == (uint32_t) p_cfg->p_uart->p_cfg->txi_ipl) ||
        (BSP_IRQ_DISABLED   == (uint32_t) p_cfg->p_uart->p_cfg->tei_ipl) ||
        (BSP_IRQ_DISABLED   == (uint32_t) p_cfg->p_uart->p_cfg->eri_ipl))
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
 * @brief       Get the size, in bytes, of the PES control block structure.
 * @param[in]   p_ctrl  Pointer to a PES control block (used only for sizeof(*p_ctrl)).
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
