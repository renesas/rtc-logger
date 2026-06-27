/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_hal.c
 * @brief HAL abstraction shim for the RTC Event Logger PES.
 *
 * This module is a thin wrapper around FSP HAL driver calls. It owns:
 *   - RTC open/close/get/set wrappers
 *   - UART open/close/write wrappers
 *   - The UART TX-complete callback (ISR context; must not block)
 *
 * The UART callback uses the p_context field of the UART callback args to
 * locate the PES control block. At Open() time, the PES stores a pointer to
 * itself in the UART driver's extend structure context so the callback can
 * update tx_complete and signal the consumer task.
 *
 * Design note: The approach of finding the control block via p_context
 * requires that the application populates the UART instance's p_context
 * field with the address of the rs_rtc_logger_ctrl_t in configuration.xml
 * or via the hardware config file. This is documented in Section 10 of
 * the architecture document. Alternatively, a single global pointer
 * (g_rs_rtc_logger_uart_ctrl_ptr) is used as a fallback when p_context
 * is NULL — this is safe for single-instance usage (primary target use case).
 */

#include <rs_rtc_logger/rs_rtc_logger_hal.h>

/* Forward declaration — implemented in rs_rtc_logger_demo.c.
 * Called from ISR context to enqueue one received character
 * into the demo CLI RX ring buffer. */
extern void demo_uart_rx_char_enqueue(uint8_t c);
#include <rs_rtc_logger/rs_rtc_logger.h>

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
 #include "FreeRTOS.h"
 #include "task.h"
#endif

/***********************************************************************************************************************
 * Module-scope pointer — allows the UART ISR callback to find the ctrl block.
 * Valid for single-instance usage (primary reference target use case).
 * For multi-instance, applications must supply p_context in the UART config.
 **********************************************************************************************************************/
static rs_rtc_logger_ctrl_t * volatile gp_rs_rl_uart_ctrl = NULL;

/***********************************************************************************************************************
 * Private helper functions
 **********************************************************************************************************************/

/**
 * @brief Locate the control block for the UART ISR callback.
 *
 * Attempts p_context first (multi-instance safe), falls back to the
 * module-global pointer (single-instance path).
 *
 * @param[in] p_args  UART callback args from HAL.
 *
 * @return Pointer to the PES control block, or NULL if not found.
 */
static rs_rtc_logger_ctrl_t * rs_rl_hal_find_ctrl(uart_callback_args_t * p_args)
{
    if ((NULL != p_args) && (NULL != p_args->p_context))
    {
        return (rs_rtc_logger_ctrl_t *)p_args->p_context;
    }
    return gp_rs_rl_uart_ctrl;
}

/***********************************************************************************************************************
 * RTC HAL wrappers
 **********************************************************************************************************************/

/**
 * @brief Open the RTC HAL driver.
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS              RTC opened successfully.
 * @retval FSP_ERR_NOT_INITIALIZED  RTC opened but oscillator probe failed.
 * @retval (other)                  Propagated from RTC HAL open().
 */
fsp_err_t rs_rtc_logger_hal_rtc_open(rs_rtc_logger_ctrl_t * p_ctrl)
{
    const rtc_instance_t * p_rtc = p_ctrl->p_cfg->p_rtc;

    /* With RTC_CFG_OPEN_SET_CLOCK_SOURCE = 0, R_RTC_Open() initialises the
     * driver control block without touching the running RTC counter.
     * This preserves the calendar time across warm resets (FR-060, FR-062). */
    fsp_err_t err = p_rtc->p_api->open(p_rtc->p_ctrl, p_rtc->p_cfg);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* On Power-On Reset the RTC hardware has not been started yet.
     * Call clockSourceSet() explicitly to select LOCO and start the counter.
     * On a warm reset the counter is already running — skip this to avoid
     * stopping and resetting the calendar time (FR-061). */
    if (1U == R_SYSTEM->RSTSR0_b.PORF)
    {
        /* Power-On Reset: initialise LOCO clock source and reset the RTC
         * hardware. With RTC_CFG_OPEN_SET_CLOCK_SOURCE=0, R_RTC_Open()
         * does not do this automatically — we call clockSourceSet()
         * explicitly on POR only (FR-061, FR-062).
         * After clockSourceSet() the counter is stopped and the count
         * registers are zeroed; CalendarTimeSet() in the application
         * sets the user time and restarts the counter. */
        err = p_rtc->p_api->clockSourceSet(p_rtc->p_ctrl);
        if (FSP_SUCCESS != err)
        {
            (void)p_rtc->p_api->close(p_rtc->p_ctrl);
            return err;
        }
    }
    else
    {
        /* Warm reset: the calendar registers retain their values because
         * R_RTC_Open() (with RTC_CFG_OPEN_SET_CLOCK_SOURCE=0) does not
         * touch any RTC hardware register.
         *
         * However, the hardware resets RCR2.START to 0, stopping the
         * counter even though the register values are preserved.
         * Fix: read the retained time and write it straight back.
         * CalendarTimeSet() performs: stop → write same values → start,
         * so the counter resumes advancing from the preserved time
         * with no calendar change (FR-060). */
        rtc_time_t retained_time;
        err = p_rtc->p_api->calendarTimeGet(p_rtc->p_ctrl, &retained_time);
        if (FSP_SUCCESS != err)
        {
            (void)p_rtc->p_api->close(p_rtc->p_ctrl);
            return FSP_ERR_NOT_INITIALIZED;
        }

        /* Restart the counter by writing the retained time back via
         * calendarTimeSet() (stop → write registers → start).
         *
         * FIX: previously this error was silently ignored with (void).  If
         * calendarTimeSet() fails the counter remains stopped; all subsequent
         * EventTrigger() timestamps would be identical stale values.  Treat
         * failure as FSP_ERR_NOT_INITIALIZED so the caller knows the RTC is
         * not usable, consistent with the POR failure path above. */
        err = p_rtc->p_api->calendarTimeSet(p_rtc->p_ctrl, &retained_time);
        if (FSP_SUCCESS != err)
        {
            (void)p_rtc->p_api->close(p_rtc->p_ctrl);
            return FSP_ERR_NOT_INITIALIZED;
        }
    }

    return FSP_SUCCESS;
}

/**
 * @brief Close the RTC HAL driver.
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS  RTC closed.
 * @retval (other)      Propagated from RTC HAL close().
 */
fsp_err_t rs_rtc_logger_hal_rtc_close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    const rtc_instance_t * p_rtc = p_ctrl->p_cfg->p_rtc;
    return p_rtc->p_api->close(p_rtc->p_ctrl);
}

/**
 * @brief Get the current RTC calendar time.
 *
 * @param[in]  p_ctrl  PES control block.
 * @param[out] p_time  Populated with current RTC value.
 *
 * @retval FSP_SUCCESS  Time retrieved.
 * @retval (other)      Propagated from RTC HAL calendarTimeGet().
 */
fsp_err_t rs_rtc_logger_hal_rtc_time_get(rs_rtc_logger_ctrl_t * p_ctrl,
                                          rtc_time_t           * p_time)
{
    const rtc_instance_t * p_rtc = p_ctrl->p_cfg->p_rtc;
    return p_rtc->p_api->calendarTimeGet(p_rtc->p_ctrl, p_time);
}

/**
 * @brief Set the RTC calendar time.
 *
 * @param[in] p_ctrl  PES control block.
 * @param[in] p_time  Time value to set.
 *
 * @retval FSP_SUCCESS  Time set.
 * @retval (other)      Propagated from RTC HAL calendarTimeSet().
 */
fsp_err_t rs_rtc_logger_hal_rtc_time_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                          rtc_time_t           * p_time)
{
    const rtc_instance_t * p_rtc = p_ctrl->p_cfg->p_rtc;
    return p_rtc->p_api->calendarTimeSet(p_rtc->p_ctrl, p_time);
}

/**
 * @brief Update the LOCO frequency register (RFRL) for drift compensation.
 *
 * R_RTC_ErrorAdjustmentSet() (RADJ register) is Sub-clock only and returns
 * FSP_ERR_UNSUPPORTED for LOCO.  For LOCO sources the correct compensation
 * mechanism is the RFRL (Frequency Register Low) register.
 *
 * Procedure (per RA4E2 HW manual, RTC section "Frequency Register"):
 *   1. Read and save the current calendar time.
 *   2. Stop the RTC counter (RCR2.START = 0).
 *   3. Write 0 to RFRH, then write the new value to RFRL.
 *   4. Restore the saved time via calendarTimeSet(), which also restarts
 *      the counter — no seconds are lost.
 *
 * @param[in] p_ctrl      PES control block.
 * @param[in] rfrl_value  New RFRL value (FSP valid range: 8–510).
 *
 * @retval FSP_SUCCESS              RFRL updated; counter restarted.
 * @retval FSP_ERR_INVALID_ARGUMENT rfrl_value outside FSP-permitted range.
 * @retval (other)                  Propagated from calendarTimeGet/Set.
 */
fsp_err_t rs_rtc_logger_hal_loco_freq_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                           uint16_t               rfrl_value)
{
    /* FSP validates 8 ≤ RFRL ≤ 510 in r_rtc_rfrl_validate().
     * Mirror that check here so callers get a clean error code. */
    if ((rfrl_value < 8U) || (rfrl_value > 510U))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    const rtc_instance_t * p_rtc = p_ctrl->p_cfg->p_rtc;

    /* Step 1: Capture the current calendar time so we can restore it
     * after the counter is stopped.  This keeps the timestamp continuous
     * (the counter is stopped for only a few microseconds). */
    rtc_time_t saved_time;
    fsp_err_t err = p_rtc->p_api->calendarTimeGet(p_rtc->p_ctrl, &saved_time);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* Step 2: Stop the RTC counter before touching RFRL.
     * Per RA HW manual: RFRL must only be written while the counter is stopped. */
    R_RTC->RCR2_b.START = 0U;
    FSP_HARDWARE_REGISTER_WAIT(R_RTC->RCR2_b.START, 0U);

    /* Step 3: Write new RFRL.
     * Per RA HW manual: write 0 to RFRH before writing RFRL. */
    R_RTC->RFRH = 0U;
    R_RTC->RFRL = rfrl_value;

    /* Step 4: Restore the saved time.  calendarTimeSet() stops the counter,
     * writes all calendar registers, and restarts — the RFRL value written
     * in step 3 is preserved and takes effect immediately on restart.
     *
     * FIX: If calendarTimeSet() fails the counter is left stopped (Step 2
     * cleared START and calendarTimeSet never restarted it).  Attempt a
     * direct restart so at minimum the RTC keeps running at the retained
     * pre-update time, rather than halting timestamping entirely. */
    fsp_err_t restore_err = p_rtc->p_api->calendarTimeSet(p_rtc->p_ctrl, &saved_time);
    if (FSP_SUCCESS != restore_err)
    {
        /* Best-effort counter restart — write 1 to RCR2.START directly.
         * The counter will resume from whatever the calendar registers hold.
         * This is better than leaving the RTC fully stopped. */
        R_RTC->RCR2_b.START = 1U;
        FSP_HARDWARE_REGISTER_WAIT(R_RTC->RCR2_b.START, 1U);
    }

    return restore_err;
}

/***********************************************************************************************************************
 * UART HAL wrappers
 **********************************************************************************************************************/

/**
 * @brief Open the UART HAL driver.
 *
 * Also registers the module-global control block pointer for the ISR callback
 * fallback path (single-instance use case).
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS  UART opened.
 * @retval (other)      Propagated from UART HAL open().
 */
fsp_err_t rs_rtc_logger_hal_uart_open(rs_rtc_logger_ctrl_t * p_ctrl)
{
    const uart_instance_t * p_uart = p_ctrl->p_cfg->p_uart;
    fsp_err_t               err    = p_uart->p_api->open(p_uart->p_ctrl, p_uart->p_cfg);

    if (FSP_ERR_ALREADY_OPEN == err)
    {
        /* The UART was left open by the early banner path in blinky_thread_entry.c
         * so that demo_uart_write_blocking() could print CLI status messages before
         * the logger initialises.  Close it now and reopen with the logger's own
         * callback and configuration so ownership transfers cleanly.             */
        (void)p_uart->p_api->close(p_uart->p_ctrl);
        err = p_uart->p_api->open(p_uart->p_ctrl, p_uart->p_cfg);
    }

    if (FSP_SUCCESS == err)
    {
        /* Register for ISR callback fallback path */
        gp_rs_rl_uart_ctrl = p_ctrl;
    }
    return err;
}

/**
 * @brief Close the UART HAL driver.
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS  UART closed.
 * @retval (other)      Propagated from UART HAL close().
 */
fsp_err_t rs_rtc_logger_hal_uart_close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    const uart_instance_t * p_uart = p_ctrl->p_cfg->p_uart;
    fsp_err_t               err    = p_uart->p_api->close(p_uart->p_ctrl);

    /* Clear the global pointer if this instance is the registered one */
    if (gp_rs_rl_uart_ctrl == p_ctrl)
    {
        gp_rs_rl_uart_ctrl = NULL;
    }

    return err;
}

/**
 * @brief Submit a non-blocking UART write.
 *
 * Clears the tx_complete flag before submitting so the callback can set it
 * on completion.
 *
 * @param[in] p_ctrl   PES control block.
 * @param[in] p_data   Data buffer to transmit.
 * @param[in] length   Byte count to transmit.
 *
 * @retval FSP_SUCCESS  Write submitted.
 * @retval (other)      Propagated from UART HAL write().
 */
fsp_err_t rs_rtc_logger_hal_uart_write(rs_rtc_logger_ctrl_t * p_ctrl,
                                        uint8_t        const * p_data,
                                        uint32_t               length)
{
    p_ctrl->tx_complete = false;

    const uart_instance_t * p_uart = p_ctrl->p_cfg->p_uart;
    return p_uart->p_api->write(p_uart->p_ctrl, p_data, length);
}

/***********************************************************************************************************************
 * UART TX-complete callback — ISR context
 **********************************************************************************************************************/

/**
 * @brief UART TX-complete callback registered with the UART HAL driver.
 *
 * Called from the UART ISR when transmission completes or an error occurs.
 * Sets tx_complete in the control block and optionally signals the consumer task.
 *
 * @param[in] p_args  UART callback arguments from the HAL driver.
 */
void rs_rtc_logger_uart_callback(uart_callback_args_t * p_args)
{
    rs_rtc_logger_ctrl_t * p_ctrl = rs_rl_hal_find_ctrl(p_args);
    if (NULL == p_ctrl)
    {
        return; /* Defensive — should not happen in normal operation */
    }

    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        p_ctrl->tx_complete = true;

        /* Signal consumer task to transmit next entry */
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
        if (NULL != p_ctrl->internal_task_handle)
        {
            BaseType_t higher_priority_task_woken = pdFALSE;
            vTaskNotifyGiveFromISR(p_ctrl->internal_task_handle, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
#endif

        /* Application callback — TX_COMPLETE */
        if (NULL != p_ctrl->p_cfg->p_callback)
        {
            rs_rtc_logger_callback_args_t args =
            {
                .event     = RS_RTC_LOGGER_EVENT_TX_COMPLETE,
                .err       = FSP_SUCCESS,
                .seq_count = p_ctrl->seq_count,
                .p_context = p_ctrl->p_cfg->p_context,
            };
            p_ctrl->p_cfg->p_callback(&args);
        }
    }
    else if (UART_EVENT_RX_CHAR == p_args->event)
    {
        /* Route received byte to the demo CLI input buffer.
         * The PES itself has no use for RX data; this allows the demo
         * CLI task to read user input from VCOM without a separate
         * UART instance. demo_uart_rx_char_enqueue() is ISR-safe. */
        demo_uart_rx_char_enqueue((uint8_t)p_args->data);
    }
    else if (UART_EVENT_TX_DATA_EMPTY == p_args->event)
    {
        /* TX data empty — not treated as completion; ignore */
    }
    else
    {
        /* Other events (RX, error) — treat as TX error for logger purposes */
        p_ctrl->tx_errors++;
        p_ctrl->tx_complete = true; /* Unblock consumer to allow retry / skip */

        if (NULL != p_ctrl->p_cfg->p_callback)
        {
            rs_rtc_logger_callback_args_t args =
            {
                .event     = RS_RTC_LOGGER_EVENT_TX_ERROR,
                .err       = FSP_ERR_TRANSFER_ABORTED,
                .seq_count = p_ctrl->seq_count,
                .p_context = p_ctrl->p_cfg->p_context,
            };
            p_ctrl->p_cfg->p_callback(&args);
        }
    }
}
