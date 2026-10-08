/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger.h
 * @brief Public interface for the RTC Event Logger (rs_rtc_logger).
 *
 * This component logs timestamped serial events using the MCU's on-chip RTC.
 * Events are captured asynchronously via a circular buffer and transmitted
 * over UART (VCOM). Two operating modes are supported, selected at build time
 * via RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE in rs_rtc_logger_cfg.h:
 *
 *   Example Project mode (=1, default):
 *     - Application calls only rs_rtc_logger_EventTrigger().
 *     - RTC Event Logger auto-manages Open()/Start() on the first EventTrigger() call.
 *     - RTC Event Logger owns an internal static FreeRTOS consumer task.
 *
 *   Expert mode (=0):
 *     - Application owns full lifecycle: Open() -> Start() -> EventTrigger()
 *       -> (consumer task) Write() -> Stop() -> Close().
 *     - Application owns the async consumer task.
 *
 * Usage (Expert mode):
 * @code
 *   static rs_rtc_logger_ctrl_t g_logger_ctrl;
 *   static const rs_rtc_logger_cfg_t g_logger_cfg = {
 *       .p_rtc          = &g_rtc,
 *       .ts_format      = RS_RTC_LOGGER_TS_FORMAT_ISO8601,
 *       .ts_brackets    = true,
 *       .line_term      = RS_RTC_LOGGER_TERM_CRLF,
 *       .overflow_policy = RS_RTC_LOGGER_OVERFLOW_OVERWRITE_OLDEST,
 *       .p_callback     = NULL,
 *       .p_context      = NULL,
 *   };
 *   rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
 *   rs_rtc_logger_Start(&g_logger_ctrl);
 *   // In application ISR:
 *   rs_rtc_logger_EventTrigger(&g_logger_ctrl, (uint8_t*)"Button", 6);
 * @endcode
 */

#ifndef RS_RTC_LOGGER_H
#define RS_RTC_LOGGER_H

#include <rs_rtc_logger/rs_rtc_logger_types.h>

/***********************************************************************************************************************
 * Module version
 **********************************************************************************************************************/

/** RTC Event Logger module version — encoded as (MAJOR << 16) | (MINOR << 8) | PATCH */
#define RS_RTC_LOGGER_VERSION_MAJOR    (1U)
#define RS_RTC_LOGGER_VERSION_MINOR    (0U)
#define RS_RTC_LOGGER_VERSION_PATCH    (0U)
#define RS_RTC_LOGGER_VERSION          ((RS_RTC_LOGGER_VERSION_MAJOR << 16U) | \
                                        (RS_RTC_LOGGER_VERSION_MINOR <<  8U) | \
                                        (RS_RTC_LOGGER_VERSION_PATCH))

/***********************************************************************************************************************
 * Public function prototypes
 **********************************************************************************************************************/

/* ── Configuration bridge ─────────────────────────────────────────────────── */

/**
 * rs_rtc_logger_ConfigGen(), rs_rtc_logger_ConfigCheck(), and
 * rs_rtc_logger_ConfigGetSize() live in the board-specific
 * pre-set configuration header, alongside the *_preset instance data they
 * are most commonly paired with:
 *
 *   #include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>   (FPB-RA4E2)
 *   #include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.h>   (FPB-RA6E2)
 *
 * They are declared and defined unconditionally in those headers/sources
 * (regardless of RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE), so the same three
 * functions remain available whether the caller binds *_preset instances
 * or FSP Stack Configurator-generated instances (g_rtc0 / g_uart0).
 *
 * Example (FPB-RA4E2):
 * @code
 *   #include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>
 *   ...
 *   rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
 *   fsp_err_t err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
 *   if (FSP_SUCCESS != err) { // handle bad configuration }
 *   rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
 * @endcode
 *
 * @note On boards using the Buffered I/O UART driver (e.g. FPB-RA4E2), this
 * component never opens, writes, or validates a UART instance at all — the
 * shared UART is owned and independently validated by that driver. There is
 * no p_uart field on the config struct and no UART parameter on ConfigGen();
 * see rs_rtc_logger_types.h and rs_rtc_logger_data_fpb_ra4e2.c for why.
 */

/* ── Lifecycle functions ──────────────────────────────────────────────────── */

/**
 * @brief Validate configuration, open RTC and UART HAL drivers, prepare RTC Event Logger state.
 *
 * Opens HAL drivers in order: RTC first, then UART.
 * Validates RTC oscillator is running.
 * Circular buffer and state are initialized.
 * Does NOT enable active UART output — call Start() for that.
 *
 * Power-on reset (POR) behavior (MRD v06 Section 5):
 * On POR the RTC hardware resets its calendar to 00:00:00, 00/00/00 (all-zeros
 * default). Open() does not alter this — the RTC is left in its reset state until
 * the application calls rs_rtc_logger_TimeSet(). The reference application CLI task
 * detects POR (R_SYSTEM->RSTSR0 POR flag set), prompts the user via VCOM to enter
 * the current date and time, then calls TimeSet() before logging begins.
 * On a warm reset (non-POR), the RTC continues running and retains its calendar
 * configuration; Open() preserves the existing RTC time without modification.
 *
 * @param[in] p_ctrl  Pointer to caller-allocated control block (uninitialised on entry).
 * @param[in] p_cfg   Pointer to configuration structure (must remain valid until Close()).
 *
 * @retval FSP_SUCCESS              Opened successfully; state -> OPEN.
 * @retval FSP_ERR_ASSERTION        p_ctrl or p_cfg is NULL (param checking enabled).
 * @retval FSP_ERR_ALREADY_OPEN     RTC Event Logger is not in CLOSED state.
 * @retval FSP_ERR_NOT_INITIALIZED  RTC oscillator not running.
 * @retval FSP_ERR_INVALID_HW_CONDITION RTC carry interrupt priority exceeds
 *                                  RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT. The carry ISR
 *                                  must have higher preemption (lower IPL number) than
 *                                  any ISR that calls rs_rtc_logger_EventTrigger().
 *                                  See RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT in cfg.h.
 * @retval (other)                  Propagated from underlying HAL Open() calls.
 *
 * @note Valid in: CLOSED state only.
 */
fsp_err_t rs_rtc_logger_Open(rs_rtc_logger_ctrl_t       * p_ctrl,
                              rs_rtc_logger_cfg_t  const * p_cfg);

/**
 * @brief Enable active UART output path.
 *
 * Transitions the RTC Event Logger from OPEN or STOPPED to STARTED.
 * After this call, rs_rtc_logger_EventTrigger() enqueues events for transmission.
 * The application consumer task may begin dequeuing and calling rs_rtc_logger_Write().
 *
 * In Example Project mode, this function also creates the internal FreeRTOS
 * consumer task on the first call (if not already created).
 *
 * @param[in] p_ctrl  Pointer to RTC Event Logger control block.
 *
 * @retval FSP_SUCCESS           Started; state -> STARTED.
 * @retval FSP_ERR_NOT_OPEN      RTC Event Logger is not in OPEN or STOPPED state.
 * @retval FSP_ERR_INVALID_MODE  RTC Event Logger is already STARTED.
 *
 * @note Valid in: OPEN, STOPPED states.
 */
fsp_err_t rs_rtc_logger_Start(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Suspend UART output, preserving all configuration and buffer contents.
 *
 * The RTC Event Logger transitions to STOPPED. EventTrigger() continues to enqueue to the circular
 * buffer (events are not lost). Write() returns FSP_ERR_INVALID_MODE.
 * Call Start() to resume transmission.
 *
 * @param[in] p_ctrl  Pointer to RTC Event Logger control block.
 *
 * @retval FSP_SUCCESS        Stopped; state -> STOPPED.
 * @retval FSP_ERR_NOT_OPEN   RTC Event Logger is not in OPEN or STARTED state (CLOSED).
 * @retval FSP_ERR_INVALID_MODE RTC Event Logger is already STOPPED.
 *
 * @note Valid in: STARTED state only.
 */
fsp_err_t rs_rtc_logger_Stop(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Release all RTC Event Logger resources and close underlying HAL drivers.
 *
 * HAL drivers closed in reverse order: UART first, then RTC.
 * Control block is zeroed; p_ctrl may be reused in a subsequent Open().
 *
 * @param[in] p_ctrl  Pointer to RTC Event Logger control block.
 *
 * @retval FSP_SUCCESS        Closed; state -> CLOSED.
 * @retval FSP_ERR_NOT_OPEN   RTC Event Logger is not in OPEN or STOPPED state.
 * @retval FSP_ERR_INVALID_MODE RTC Event Logger is in STARTED state — call Stop() first.
 *
 * @note Valid in: OPEN, STOPPED states. Calling while STARTED returns
 *       FSP_ERR_INVALID_MODE (enforced). Stop() first, then Close().
 */
fsp_err_t rs_rtc_logger_Close(rs_rtc_logger_ctrl_t * p_ctrl);

/* ── Operational functions ────────────────────────────────────────────────── */

/**
 * @brief ISR-safe event capture entry point.
 *
 * Captures the current RTC timestamp immediately, composes a
 * (timestamp, seq_count, event_data) entry, and enqueues it into the
 * static circular buffer inside the control block. Returns without blocking.
 *
 * This is the primary call made by any application event source —
 * r_icu ISR, timer callback, accelerometer callback, or direct software call.
 * The RTC Event Logger has no knowledge of what generated the call.
 *
 * Example Project mode behavior (RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1):
 * - If RTC Event Logger is in CLOSED state on the first call: the event is enqueued to the
 *   circular buffer immediately (event not lost), an auto-init pending flag is
 *   set, and a deferred Open()/Start() sequence is posted. Returns FSP_SUCCESS.
 *   Once STARTED state is reached, the internal consumer task drains the buffer.
 *   (FR-121, FR-122)
 * - If RTC Event Logger is already in STARTED or STOPPED state: normal enqueue path.
 *
 * Expert mode behavior (RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 0):
 * - Application must call Open() and Start() before EventTrigger().
 * - If called while not in STARTED or STOPPED state: returns FSP_ERR_NOT_OPEN.
 *   (FR-125, FR-126)
 *
 * Overflow behavior is determined by rs_rtc_logger_cfg_t.overflow_policy.
 * If the buffer is full and policy is OVERWRITE_OLDEST, the oldest entry is
 * silently replaced. If DROP_NEWEST, the new event is dropped and
 * p_ctrl->events_dropped is incremented.
 *
 * @param[in] p_ctrl      RTC Event Logger control block.
 * @param[in] p_data      Pointer to event payload (ASCII, need not be NUL-terminated).
 * @param[in] length      Payload byte count. Truncated to RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN.
 *
 * @retval FSP_SUCCESS            Event enqueued (both modes).
 * @retval FSP_ERR_NOT_OPEN       Expert mode: RTC Event Logger not in STARTED or STOPPED state.
 * @retval FSP_ERR_OVERFLOW    Buffer full and overflow policy is DROP_NEWEST.
 *
 * @note ISR-safe. Non-blocking. Valid in: STARTED and STOPPED states (both modes).
 *       Example Project mode additionally handles CLOSED state on first call.
 * @note seq_count is incremented atomically on each successful enqueue.
 * @note N-12 — RTC carry ISR priority requirement: this function calls
 *       R_RTC_CalendarTimeGet() which spins waiting for the RTC carry ISR to
 *       fire.  The carry ISR MUST have higher priority (lower IPL number) than
 *       this calling ISR, or it can never preempt this ISR — causing an
 *       infinite loop (deadlock).  The preset config sets carry_ipl = 2 which
 *       is safely above the demo button ISR at ipl = 5.  When using the FSP
 *       Stack Configurator path, set the RTC carry interrupt priority to a
 *       value strictly less than the IPL of every ISR that calls this function.
 *       rs_rtc_logger_Open() checks this at init time via
 *       RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT (default 4).
 */
fsp_err_t rs_rtc_logger_EventTrigger(rs_rtc_logger_ctrl_t * p_ctrl,
                                      uint8_t const        * p_data,
                                      uint32_t               length);

/**
 * @brief Transmit a pre-composed timestamped line over UART (VCOM).
 *
 * Called by the async consumer task after dequeuing an entry from the circular
 * buffer. Formats the complete output line and submits it to the UART HAL (VCOM
 * path) for transmission. This function initiates a non-blocking UART write and
 * returns immediately. UART TX completion is signaled via the registered callback.
 *
 * In Example Project mode, this function is called internally by the RTC Event
 * Logger's own consumer task — it is not required from application code.
 * In Expert mode, the application consumer task calls this function explicitly.
 *
 * @param[in] p_ctrl   RTC Event Logger control block.
 * @param[in] p_entry  Pointer to dequeued circular buffer entry.
 *
 * @retval FSP_SUCCESS           Transmission initiated.
 * @retval FSP_ERR_NOT_OPEN      RTC Event Logger is not in STARTED state.
 * @retval FSP_ERR_INVALID_SIZE  Composed line exceeds RS_RTC_LOGGER_CFG_LINE_BUF_SIZE.
 * @retval (other)               Propagated from UART HAL write.
 *
 * @note Non-blocking. Valid in: STARTED state only.
 */
fsp_err_t rs_rtc_logger_Write(rs_rtc_logger_ctrl_t              * p_ctrl,
                               rs_rtc_logger_event_entry_t const * p_entry);

/**
 * @brief Convenience overload: write a direct NUL-terminated string with live RTC timestamp.
 *
 * Reads the RTC at call time, composes and transmits the line immediately
 * (bypassing the circular buffer). Use for non-ISR software-triggered log entries.
 *
 * @param[in] p_ctrl  RTC Event Logger control block.
 * @param[in] p_str   NUL-terminated ASCII string.
 *
 * @retval FSP_SUCCESS           Transmission initiated.
 * @retval FSP_ERR_NOT_OPEN      RTC Event Logger is not in STARTED state.
 * @retval FSP_ERR_INVALID_SIZE  Composed line exceeds RS_RTC_LOGGER_CFG_LINE_BUF_SIZE.
 * @retval (other)               Propagated from UART HAL write.
 *
 * @note Non-blocking. Valid in: STARTED state only.
 * @note N-13 — seq_count: WriteStr() uses the current seq_count WITHOUT
 *       incrementing it (unlike EventTrigger() which increments on every call).
 *       If WriteStr() is interleaved with EventTrigger() calls, the output
 *       will contain duplicate sequence numbers — this is intentional.
 *       WriteStr() is an out-of-band path that does not participate in the
 *       counted event sequence.  Use EventTrigger() if unique seq numbers
 *       are required for every output line.
 */
fsp_err_t rs_rtc_logger_WriteStr(rs_rtc_logger_ctrl_t * p_ctrl,
                                  char const           * p_str);

/**
 * @brief Dequeue the oldest event entry from the circular buffer.
 *
 * Provides Expert-mode consumer tasks with a single canonical dequeue path,
 * eliminating the need for application code to access buf_tail / buf_count
 * directly. Uses the same ISR-safe critical section as the internal EP-mode
 * consumer task (rs_rl_dequeue), so there is no TOCTOU exposure regardless
 * of the configured overflow policy.
 *
 * Typical Expert-mode consumer task usage:
 * @code
 *   rs_rtc_logger_event_entry_t entry;
 *   if (FSP_SUCCESS == rs_rtc_logger_EventDequeue(&g_logger_ctrl, &entry))
 *   {
 *       rs_rtc_logger_Write(&g_logger_ctrl, &entry);
 *   }
 * @endcode
 *
 * @param[in]  p_ctrl      RTC Event Logger control block.
 * @param[out] p_entry_out Caller-allocated entry; populated when FSP_SUCCESS returned.
 *
 * @retval FSP_SUCCESS       Entry dequeued; p_entry_out is valid.
 * @retval FSP_ERR_NOT_OPEN  RTC Event Logger is CLOSED.
 * @retval FSP_ERR_ASSERTION p_ctrl or p_entry_out is NULL.
 * @retval FSP_ERR_BUFFER_EMPTY Circular buffer was empty; p_entry_out not populated.
 *
 * @note Task context only — not ISR-safe. Valid in: OPEN, STARTED, STOPPED states.
 */
fsp_err_t rs_rtc_logger_EventDequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                                      rs_rtc_logger_event_entry_t * p_entry_out);

/* ── Configuration functions (hot-safe — valid in OPEN, STARTED, STOPPED) ── */

/**
 * @brief Change the timestamp format at runtime.
 *
 * Hot-safe: takes effect on the next Write() call. No interruption of the
 * current transmission path.
 *
 * @param[in] p_ctrl   Pointer to RTC Event Logger control block.
 * @param[in] format   New timestamp format selection.
 *
 * @retval FSP_SUCCESS          Format updated.
 * @retval FSP_ERR_NOT_OPEN     RTC Event Logger is CLOSED.
 * @retval FSP_ERR_ASSERTION    Invalid format value.
 */
fsp_err_t rs_rtc_logger_TimestampFormatSet(rs_rtc_logger_ctrl_t    * p_ctrl,
                                            rs_rtc_logger_ts_format_t format);

/**
 * @brief Set the RTC calendar date and time.
 *
 * Delegates to the underlying RTC HAL driver. Hot-safe.
 * Called by the reference application CLI task on startup.
 *
 * @param[in] p_ctrl  Pointer to RTC Event Logger control block.
 * @param[in] p_time  Pointer to RTC time structure to set.
 *
 * @retval FSP_SUCCESS      RTC time set.
 * @retval FSP_ERR_NOT_OPEN RTC Event Logger is CLOSED.
 * @retval (other)          Propagated from RTC HAL calendarTimeSet().
 */
fsp_err_t rs_rtc_logger_TimeSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                 rtc_time_t     const * p_time);

/**
 * @brief Explicitly validate (or invalidate) the RTC clock state after Open().
 *
 * rs_rtc_logger_Open() always initialises the internal rtc_time_set flag to
 * false.  Call this immediately after Open() to signal that the RTC holds a
 * user-confirmed time — typically on warm reset when a noinit magic sentinel
 * written by a previous successful TimeSet() call is still intact.
 *
 * Without this call a warm reset after a cold start where the user never set
 * the time would display the RTC default (2000-01-01 00:00:00) as a real
 * timestamp instead of [0000-00-00 00:00:00].
 *
 * @param[in] p_ctrl        Pointer to RTC Event Logger control block.
 * @param[in] time_is_valid true  = RTC holds a user-confirmed time.
 *                          false = RTC unset; events show [0000-00-00 00:00:00].
 *
 * @retval FSP_SUCCESS      Clock validity flag updated.
 * @retval FSP_ERR_NOT_OPEN RTC Event Logger is CLOSED.
 * @retval FSP_ERR_ASSERTION p_ctrl is NULL.
 */
fsp_err_t rs_rtc_logger_ClockValidate(rs_rtc_logger_ctrl_t * p_ctrl,
                                       bool                   time_is_valid);

/**
 * @brief Compensate for LOCO oscillator drift by updating the RFRL register.
 *
 * The RA4E2 RTC LOCO clock source uses the RFRL (Frequency Register Low)
 * register to calibrate the oscillator.  The RTC counts (RFRL + 1) LOCO
 * cycles per 128 Hz sub-tick; 128 sub-ticks make one second.
 *
 * R_RTC_ErrorAdjustmentSet() (RADJ register) is Sub-clock only and returns
 * FSP_ERR_UNSUPPORTED for LOCO clock sources.  This function provides the
 * correct LOCO drift-compensation path.
 *
 * Tuning guide (nominal RFRL = 255, one unit ≈ 3906 ppm ≈ 3.5 s per 15 min):
 * @code
 *   Clock fast 1 min per 15 min  →  trim = +17  →  rfrl_value = 272
 *   Clock slow 1 min per 15 min  →  trim = -17  →  rfrl_value = 238
 *   Clock fast 1 sec  per hour   →  trim = +1   →  rfrl_value = 256
 *   Clock slow 1 sec  per hour   →  trim = -1   →  rfrl_value = 254
 * @endcode
 *
 * The current time is read, the counter is stopped, RFRL is updated, and
 * the time is restored — no seconds are lost during the update.
 *
 * Valid RFRL range (FSP constraint): 8 ≤ rfrl_value ≤ 510.
 *
 * @param[in] p_ctrl      Pointer to RTC Event Logger control block.
 * @param[in] rfrl_value  New RFRL register value (8–510).
 *
 * @retval FSP_SUCCESS              RFRL updated; RTC counter restarted.
 * @retval FSP_ERR_NOT_OPEN         RTC Event Logger is CLOSED.
 * @retval FSP_ERR_INVALID_ARGUMENT rfrl_value outside valid range.
 * @retval (other)                  Propagated from RTC HAL.
 *
 * @note Hot-safe: valid in OPEN, STARTED, and STOPPED states.
 */
fsp_err_t rs_rtc_logger_LocoCalibrationSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                            uint16_t               rfrl_value);

/**
 * @brief Restore the event sequence counter after a warm reset.
 *
 * Called by the application after Open() when a warm reset is detected
 * (R_SYSTEM->RSTSR0 indicates non-POR reset). Allows the counter to
 * continue from its pre-reset value per FR-064.
 *
 * @param[in] p_ctrl      Pointer to RTC Event Logger control block.
 * @param[in] seq_count   Sequence counter value to restore.
 *
 * @retval FSP_SUCCESS      Counter restored.
 * @retval FSP_ERR_NOT_OPEN RTC Event Logger is CLOSED.
 *
 * @note Valid in: OPEN, STARTED, STOPPED states.
 *       STARTED is permitted because in Example Project mode the RTC Event Logger reaches
 *       STARTED (via auto-init) before the CLI task can call SeqCountSet().
 *       A 32-bit aligned store is atomic on Cortex-M33.
 * @note N-9: Call this function **before** arming any interrupt source that
 *       calls rs_rtc_logger_EventTrigger().  If an EventTrigger() ISR fires
 *       between the moment the restore value is chosen and this store
 *       executes, the ISR's seq_count++ will be overwritten by the restore
 *       value, silently creating a duplicate sequence number in the output.
 *       On a typical warm-reset path this is naturally satisfied because the
 *       application calls SeqCountSet() during initialisation before enabling
 *       the external IRQ.
 */
fsp_err_t rs_rtc_logger_SeqCountSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                     uint32_t               seq_count);

/* ── Query functions (valid in OPEN, STARTED, STOPPED) ───────────────────── */

/**
 * @brief Get a snapshot of RTC Event Logger runtime status and diagnostics.
 *
 * @param[in]  p_ctrl    Pointer to RTC Event Logger control block.
 * @param[out] p_status  Caller-allocated status structure, populated on return.
 *
 * @retval FSP_SUCCESS       Status populated.
 * @retval FSP_ERR_NOT_OPEN  RTC Event Logger is CLOSED.
 * @retval FSP_ERR_ASSERTION p_status is NULL.
 */
fsp_err_t rs_rtc_logger_StatusGet(rs_rtc_logger_ctrl_t   * p_ctrl,
                                   rs_rtc_logger_status_t * p_status);

/**
 * @brief Read the current RTC date and time directly.
 *
 * @param[in]  p_ctrl  Pointer to RTC Event Logger control block.
 * @param[out] p_time  Populated with current RTC calendar time value.
 *
 * @retval FSP_SUCCESS       Time populated.
 * @retval FSP_ERR_NOT_OPEN  RTC Event Logger is CLOSED.
 * @retval (other)           Propagated from RTC HAL calendarTimeGet().
 */
fsp_err_t rs_rtc_logger_TimeGet(rs_rtc_logger_ctrl_t * p_ctrl,
                                 rtc_time_t           * p_time);

/**
 * @brief Get the RTC Event Logger module version.
 *
 * Valid in any lifecycle state, including CLOSED.
 *
 * @param[out] p_version  Populated with MAJOR.MINOR.PATCH version fields.
 *
 * @retval FSP_SUCCESS always.
 */
fsp_err_t rs_rtc_logger_VersionGet(uint32_t * p_version);

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
/**
 * @brief Create the internal consumer task for Example Project mode.
 *
 * Call this from task context before the ICU external IRQ is enabled.
 * This ensures xTaskCreateStatic() is never called from ISR context.
 *
 * @param[in] p_ctrl  RTC Event Logger control block. p_cfg must already be set.
 *
 * @retval FSP_SUCCESS          Consumer task created (or already existed).
 * @retval FSP_ERR_ASSERTION    p_ctrl or p_cfg is NULL.
 * @retval FSP_ERR_OUT_OF_MEMORY Task creation failed.
 */
fsp_err_t rs_rtc_logger_EP_PrepareConsumerTask(rs_rtc_logger_ctrl_t * p_ctrl);
#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

#endif /* RS_RTC_LOGGER_H */
