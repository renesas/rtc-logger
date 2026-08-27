/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_hal.c
 * @brief HAL abstraction shim for the RTC Event Logger.
 *
 * This module is a thin wrapper around FSP HAL driver calls. It owns:
 *   - RTC open/close/get/set wrappers (direct raw-driver calls)
 *   - UART open/close/write wrappers (routed through the Buffered I/O
 *     UART driver's stdio stream — see the note below)
 *
 * There is no UART ISR callback owned by this component: rs_rtc_logger_hal_uart_write()
 * blocks briefly, on the calling task only, until the write is safely
 * submitted/serialized, so there is no asynchronous TX-complete event for
 * this component to track.
 */

#include <rs_rtc_logger/rs_rtc_logger_hal.h>
#include <rs_rtc_logger/rs_rtc_logger.h>

/* All console I/O routes through the transport-agnostic console adapter.
 * RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE selects UART (0, default) or RTT (1).
 * No direct stdio or SEGGER RTT calls are made from this file. */
#include "rs_rtc_logger_stdio_console.h"

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
 #include "FreeRTOS.h"
 #include "task.h"
#endif

/***********************************************************************************************************************
 * RTC HAL wrappers
 **********************************************************************************************************************/

/**
 * @brief Open the RTC HAL driver.
 *
 * @param[in] p_ctrl  control block.
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

    /* N-12: Validate that the RTC carry interrupt has HIGHER priority (lower IPL
     * number) than any ISR that calls rs_rtc_logger_EventTrigger().
     *
     * R_RTC_CalendarTimeGet() spins waiting for the carry ISR to fire.
     * If the EventTrigger() ISR has HIGHER priority than carry, the carry ISR
     * can never preempt it — deadlock.  The FSP warning is:
     *   "Do not call from interrupt with higher priority than carry interrupt."
     *
     * The preset config uses carry_ipl = 2, button ISR ipl = 5 → safe.
     * The FSP-generated hal_data.c uses carry_ipl = 12, button ISR ipl = 5
     * → carry has LOWER priority than the button ISR → deadlock path.
     *
     * This check reads carry_ipl directly from the RTC config.  If it exceeds
     * RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT (default 4), the open fails with
     * FSP_ERR_INVALID_HW_CONDITION so the misconfiguration is caught at init
     * time rather than producing an invisible infinite loop in the ISR. */
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    {
        uint32_t carry_ipl = (uint32_t)p_rtc->p_cfg->carry_ipl;
        if (carry_ipl > RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT)
        {
            (void)p_rtc->p_api->close(p_rtc->p_ctrl);
            return FSP_ERR_INVALID_HW_CONDITION;
        }
    }
#endif

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

        /* BUG FIX (validation-test finding): RSTSR0.PORF is a sticky
         * hardware flag — per the MCU register description, it is
         * "Writable only to clear the flag. Confirm the value is 1 and
         * then write 0." It is NOT cleared automatically on the next
         * reset. Left unclear, every subsequent reset (including a
         * genuine warm reset via the reset button) would ALSO read
         * PORF == 1, wrongly re-enter this POR branch, and call
         * clockSourceSet() again — which stops the counter and zeroes
         * the calendar registers, silently destroying the retained time
         * on what should have been a warm-reset retention path.
         * Clear it now that we've consumed it, so the flag reflects only
         * the reset that just occurred. */
        R_SYSTEM->RSTSR0_b.PORF = 0U;
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

    /* Keep the carry interrupt permanently active,
     * BOTH at the RTC peripheral level (CIE) AND at the NVIC level.
     *
     * The AHB-domain calendar registers (RSECCNT etc.) are
     * refreshed from the LOCO domain by the carry ISR mechanism.  When
     * FSP's calendarTimeGet() finishes, it disables CIE AND disables the
     * NVIC interrupt (see R_RTC_CalendarTimeGet() in r_rtc.c: it calls
     * R_BSP_IrqDisable(carry_irq) at the end unless the NVIC was already
     * enabled on entry).  Without a live carry ISR, the AHB-visible
     * RSECCNT freezes at the value from the last ISR execution.  After a
     * long idle period the sandwich read in hal_rtc_time_get() reads the
     * same stale value before AND after the register sequence (sec_before
     * == sec_after), exits the loop, and returns the wrong timestamp —
     * observed as multiple successive button presses returning an
     * identical frozen time.
     *
     * Explicitly call R_BSP_IrqEnable() on the carry IRQ vector, in
     * addition to setting CIE, so the carry interrupt is actually unmasked
     * and delivered to the CPU every second for the lifetime of the RTC
     * Event Logger instance.
     *
     * N-12 constraint: carry_ipl must be LOWER (higher preemption) than the
     * button ISR IPL so the carry ISR can always run promptly, including
     * during the sandwich-read retry window in hal_rtc_time_get().  The
     * preset config sets carry_ipl = 2 (button ISR ipl = 5), which
     * satisfies this.  The N-12 validation earlier in this function
     * enforces it at runtime. */
    R_RTC->RCR1_b.CIE = 1U;
    R_BSP_IrqEnable(p_rtc->p_cfg->carry_irq);

    return FSP_SUCCESS;
}

/**
 * @brief Close the RTC HAL driver.
 *
 * @param[in] p_ctrl  control block.
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
 * @brief Convert a packed BCD byte to a decimal integer.
 *
 * Used exclusively by rs_rtc_logger_hal_rtc_time_get() to decode RTC
 * calendar registers. Integer arithmetic only — no division (FR-014).
 *
 * @param[in] bcd  Packed BCD byte (e.g. 0x21 → 21).
 * @return Decimal value.
 */
static uint8_t rs_rl_bcd_to_dec (uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4U) & 0x0FU) * 10U + (bcd & 0x0FU));
}

/**
 * @brief Get the current RTC calendar time — seconds-sandwich direct read.
 *
 * **Why NOT R_RTC_CalendarTimeGet() here? **
 *
 * R_RTC_CalendarTimeGet() uses the carry-interrupt method: it enables CIE
 * and spins until the carry ISR fires (once per second, when RSECCNT
 * increments).  The carry ISR reads RSECCNT at the EXACT moment of the
 * carry event — the worst possible time — because that is precisely when
 * the LOCO-domain register is propagating through the AHB synchronizer.
 *
 * On RA6E2, the AHB-LOCO synchronizer has more pipeline stages than RA4E2
 * (additional isolation layers for TrustZone power domains).  The carry ISR
 * executes within ~300 ns of the carry, but the register needs ~60-120 us
 * to settle in the AHB domain.  The ISR therefore reads the OLD second
 * value (e.g. 21) instead of the incremented value (22), causing all
 * EventTrigger() timestamps to show the same stuck second for 1-5 seconds
 * after a warm restart (observed as events #21-#28 all at 10:30:21 in
 * TestIssue_03).  The issue resolves spontaneously once the synchronizer
 * has stabilised after several carries.  It is more pronounced on RA6E2
 * than RA4E2 because the RA6E2 synchronizer has more pipeline stages.
 *
 * **Instead use — seconds-sandwich read:**
 *
 * The RA6E2 RCR1 register has no software-readable carry status flag
 * (RCR1 contains only AIE, CIE, PIE, RTCOS, PES — there is no CF bit
 * on this device).  The consistency mechanism used here is the seconds
 * sandwich: snapshot RSECCNT before and after the full register read,
 * retry if the value changed.  If RSECCNT is identical both times, no
 * carry occurred during the read and all fields are consistent.
 *
 *   1. Snapshot RSECCNT (sec_before)
 *   2. Read all calendar registers into a local struct
 *   3. Re-read RSECCNT (sec_after)
 *   4. If sec_before != sec_after, carry occurred — retry from 1
 *
 * Reading BETWEEN carry events (not AT the carry moment) gives the fully
 * settled AHB-domain value.  This is non-blocking and ISR-safe.
 *
 * The carry ISR continues to fire and update the FSP carry_irq_occurred /
 * carry_time shadow for calendarTimeGet() calls from task context (e.g.
 * Open(), LocoCalibrationSet()); those calls are unaffected.
 *
 * **N-12 carry_ipl note (Issue4 fix):**
 * CIE is now kept permanently enabled after Open() (see hal_rtc_open()).
 * The carry ISR therefore fires every second for the lifetime of the RTC
 * Event Logger instance.  The N-12 constraint — carry_ipl must be LOWER (higher
 * preemption) than the button ISR IPL — IS REQUIRED for correct operation:
 * when the sandwich loop detects a mid-carry read (sec_before != sec_after),
 * it retries; the carry ISR must be able to preempt the button ISR during
 * that retry to settle the registers before the next read.
 * The carry_ipl validation in hal_rtc_open() enforces this at runtime.
 *
 * @param[in]  p_ctrl  control block (unused; R_RTC addressed directly).
 * @param[out] p_time  Populated with current RTC calendar time.
 *
 * @retval FSP_SUCCESS  Always.
 */
fsp_err_t rs_rtc_logger_hal_rtc_time_get(rs_rtc_logger_ctrl_t * p_ctrl,
                                          rtc_time_t           * p_time)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

    rtc_time_t t;
    uint8_t    sec_before;

    do
    {
        /* Step 1: snapshot RSECCNT before reading all fields             */
        sec_before = R_RTC->RSECCNT;

        /* Data memory barrier: RSECCNT read must architecturally complete
         * before the subsequent field reads are issued.                   */
        __DMB();

        /* Step 2: read all calendar fields into local struct             */
        t.tm_sec  = (int) rs_rl_bcd_to_dec(sec_before);
        t.tm_min  = (int) rs_rl_bcd_to_dec(R_RTC->RMINCNT);
        t.tm_hour = (int) rs_rl_bcd_to_dec((uint8_t)(R_RTC->RHRCNT  & 0x3FU));
        t.tm_mday = (int) rs_rl_bcd_to_dec((uint8_t)(R_RTC->RDAYCNT & 0x3FU));
        /* RMONCNT is 1-based (01-12); struct tm tm_mon is 0-based (0-11) */
        t.tm_mon  = (int) rs_rl_bcd_to_dec((uint8_t)(R_RTC->RMONCNT & 0x1FU)) - 1;
        /* RYRCNT is 2-digit BCD (00-99 = years 2000-2099);
         * struct tm tm_year is years since 1900, so +100                 */
        t.tm_year = (int) rs_rl_bcd_to_dec((uint8_t)R_RTC->RYRCNT) + 100;
        t.tm_wday = (int)(R_RTC->RWKCNT & 0x07U);
        t.tm_yday  = 0;
        t.tm_isdst = 0;

        /* Data memory barrier: all field reads must complete before the
         * confirmation re-read of RSECCNT below.                         */
        __DMB();

        /* Step 3+4: if RSECCNT changed, a carry occurred during the read;
         * the fields may be inconsistent — retry the full read.          */
    } while (sec_before != R_RTC->RSECCNT);

    *p_time = t;
    return FSP_SUCCESS;
}

/**
 * @brief Set the RTC calendar time.
 *
 * @param[in] p_ctrl  control block.
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
 * Procedure (per RA6E2 HW manual, RTC section "Frequency Register"):
 *   1. Read and save the current calendar time.
 *   2. Stop the RTC counter (RCR2.START = 0).
 *   3. Write 0 to RFRH, then write the new value to RFRL.
 *   4. Restore the saved time via calendarTimeSet(), which also restarts
 *      the counter — no seconds are lost.
 *
 * @param[in] p_ctrl      control block.
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
 * Ensures the shared Buffered I/O UART driver is initialized (idempotent) and
 * strictly validated against the RA6E2 FPB reference configuration — see
 * rs_rtc_logger_console_init(). A validation failure here is a real,
 * non-recoverable-by-retry configuration error, and is propagated to the
 * caller the same way any other Open() failure would be.
 *
 * @param[in] p_ctrl  control block (unused).
 *
 * @retval FSP_SUCCESS  UART ready.
 * @retval (other)      Propagated from rs_rtc_logger_console_init().
 */
fsp_err_t rs_rtc_logger_hal_uart_open(rs_rtc_logger_ctrl_t * p_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

    /* The physical UART (g_uart_jlob_vcom) is opened and owned by the
     * Buffered I/O UART driver on first stdio use - typically already done
     * by the sign-on banner in blinky_thread_entry.c before this ever runs.
     * rs_rtc_logger_console_init() is idempotent and simply guarantees that
     * has happened. */
    return rs_rtc_logger_console_init();
}

/**
 * @brief Close the UART HAL driver.
 *
 * The physical UART is shared with (and owned by) the Buffered I/O UART
 * driver for the CLI console, and is never closed for the life of the
 * application, so this function is a no-op.
 *
 * @param[in] p_ctrl  control block.
 *
 * @retval FSP_SUCCESS  Always.
 */
fsp_err_t rs_rtc_logger_hal_uart_close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

    return FSP_SUCCESS;
}

/**
 * @brief Submit a UART write.
 *
 * @param[in] p_ctrl   control block.
 * @param[in] p_data   Data buffer to transmit.
 * @param[in] length   Byte count to transmit.
 *
 * @retval FSP_SUCCESS           Write submitted (and, per the Buffered I/O
 *                                UART driver's fflush() semantics, serialized
 *                                against any prior in-flight transmission).
 * @retval FSP_ERR_WRITE_FAILED  console_write() did not complete cleanly.
 */
fsp_err_t rs_rtc_logger_hal_uart_write(rs_rtc_logger_ctrl_t * p_ctrl,
                                        uint8_t        const * p_data,
                                        uint32_t               length)
{
    fsp_err_t err = rs_rtc_logger_console_write(p_data, length);
    if (FSP_SUCCESS != err)
    {
        p_ctrl->tx_errors++;
        return FSP_ERR_WRITE_FAILED;
    }

    return FSP_SUCCESS;
}

