/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger.c
 * @brief PES public function implementations for the RTC Event Logger.
 *
 * Implements:
 *   - Lifecycle FSM: Open / Start / Stop / Close with state gating
 *   - Circular buffer management (ISR-safe single-producer/single-consumer)
 *   - EventTrigger() ISR entry point
 *   - Write() / WriteStr() async UART output path
 *   - Configuration and query functions
 *   - Example Project mode: auto-lifecycle on first EventTrigger() + internal
 *     static FreeRTOS consumer task (compiled in only when
 *     RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1)
 *
 * Private functions are named rs_rl_<functionname> per Rule 6
 * (abbreviated module name: rl = rtc_logger).
 */

#include <rs_rtc_logger/rs_rtc_logger.h>
#include <rs_rtc_logger/rs_rtc_logger_format.h>
#include <rs_rtc_logger/rs_rtc_logger_hal.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
 #include "bsp_api.h"
#endif

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
 #include "FreeRTOS.h"
 #include "task.h"
#endif

/***********************************************************************************************************************
 * Static assertions
 **********************************************************************************************************************/

/* Verify control block fits within RAM budget (FR-091) */
_Static_assert(sizeof(rs_rtc_logger_ctrl_t) <= RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES,
               "rs_rtc_logger_ctrl_t exceeds RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES");

/* DC-1 fix: verify LINE_BUF_SIZE is large enough to hold the longest timestamp
 * prefix plus at least one payload character and a terminator. */
_Static_assert(RS_RTC_LOGGER_CFG_LINE_BUF_SIZE > RS_RTC_LOGGER_CFG_TS_MIN_CHARS + 4U,
               "RS_RTC_LOGGER_CFG_LINE_BUF_SIZE is too small for the timestamp prefix");

/* Fix 5: verify LOCO_RFRL_DEFAULT is within the hardware-enforced range 8–510.
 * A build-time override outside this range would silently pass rs_rtc_logger_hal_loco_freq_set()
 * validation at the HAL layer (which uses the same bounds) but the mismatch
 * would not be caught until runtime. */
_Static_assert(RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT >= 8U &&
               RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT <= 510U,
               "RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT is outside valid RFRL range (8-510)");

/***********************************************************************************************************************
 * Param-checking macro (mirrors FSP convention)
 **********************************************************************************************************************/

#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
 #define RS_RTC_LOGGER_ASSERT_RETURN(expr, err)  do { if (!(expr)) { return (err); } } while (0)
#else
 #define RS_RTC_LOGGER_ASSERT_RETURN(expr, err)  ((void)0)
#endif

/***********************************************************************************************************************
 * Example Project mode: static internal consumer task storage
 **********************************************************************************************************************/

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE

/** Static TCB for the PES-owned consumer task (FR-123 — no heap) */
static StaticTask_t g_rs_rl_internal_task_tcb;

/** Static stack for the PES-owned consumer task (FR-124) */
static StackType_t  g_rs_rl_internal_task_stack[RS_RTC_LOGGER_CFG_TASK_STACK_SIZE / sizeof(StackType_t)];

#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

static fsp_err_t rs_rl_enqueue(rs_rtc_logger_ctrl_t * p_ctrl,
                                rtc_time_t     const * p_timestamp,
                                uint8_t        const * p_data,
                                uint32_t               length);

/* DC-6 fix: rs_rl_dequeue compiled in both modes so rs_rtc_logger_EventDequeue()
 * can call it regardless of RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE. */
static bool rs_rl_dequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                           rs_rtc_logger_event_entry_t * p_entry_out);

static bool rs_rl_is_open(rs_rtc_logger_ctrl_t const * p_ctrl);

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static void rs_rtc_logger_internal_consumer_task(void * p_arg);
static fsp_err_t rs_rl_auto_init(rs_rtc_logger_ctrl_t * p_ctrl);
#endif

/***********************************************************************************************************************
 * Private helper implementations
 **********************************************************************************************************************/

/**
 * @brief Check if the PES is in any non-CLOSED state.
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @return true if OPEN, STARTED, or STOPPED; false if CLOSED or p_ctrl is NULL.
 */
static bool rs_rl_is_open(rs_rtc_logger_ctrl_t const * p_ctrl)
{
    if (NULL == p_ctrl)
    {
        return false;
    }
    return (p_ctrl->state != RS_RTC_LOGGER_STATE_CLOSED);
}

/**
 * @brief Enqueue a timestamped event entry into the circular buffer.
 *
 * ISR-safe: uses a critical section (portENTER/EXIT_CRITICAL_FROM_ISR equivalent
 * or __disable_irq/__enable_irq for non-FreeRTOS builds) to atomically update
 * buffer indices and count. Applies the configured overflow policy.
 *
 * @param[in] p_ctrl      PES control block.
 * @param[in] p_timestamp RTC time captured at the moment of the event.
 * @param[in] p_data      Event payload pointer.
 * @param[in] length      Event payload byte count.
 *
 * @retval FSP_SUCCESS         Event enqueued.
 * @retval FSP_ERR_OVERFLOW Buffer full and policy is DROP_NEWEST.
 */
static fsp_err_t rs_rl_enqueue(rs_rtc_logger_ctrl_t * p_ctrl,
                                rtc_time_t     const * p_timestamp,
                                uint8_t        const * p_data,
                                uint32_t               length)
{
    fsp_err_t err = FSP_SUCCESS;

    /* --- Critical section start ------------------------------------------ */
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
#endif

    uint32_t buf_count = p_ctrl->buf_count;

    if (buf_count >= RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH)
    {
        /* Buffer full — apply overflow policy */
        if (RS_RTC_LOGGER_OVERFLOW_DROP_NEWEST == p_ctrl->p_cfg->overflow_policy)
        {
            p_ctrl->events_dropped++;
            err = FSP_ERR_OVERFLOW;

#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
            if (0U == saved_primask) { __enable_irq(); }
#endif
            /* Callback for buffer-full event */
            if (NULL != p_ctrl->p_cfg->p_callback)
            {
                rs_rtc_logger_callback_args_t args =
                {
                    .event     = RS_RTC_LOGGER_EVENT_BUFFER_FULL,
                    .err       = FSP_ERR_OVERFLOW,
                    .seq_count = p_ctrl->seq_count,
                    .p_context = p_ctrl->p_cfg->p_context,
                };
                p_ctrl->p_cfg->p_callback(&args);
            }
            return err;
        }
        else
        {
            /* OVERWRITE_OLDEST — advance tail to make room */
            p_ctrl->buf_tail = (p_ctrl->buf_tail + 1U) % RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH;
            p_ctrl->buf_count--;
            p_ctrl->events_dropped++;
        }
    }

    /* --- Write entry at head --------------------------------------------- */
    uint32_t head  = p_ctrl->buf_head;
    rs_rtc_logger_event_entry_t * p_entry = &p_ctrl->event_buf[head];

    /* Copy timestamp */
    p_entry->timestamp = *p_timestamp;

    /* Assign and increment sequence counter atomically */
    p_ctrl->seq_count++;
    p_entry->seq_count = p_ctrl->seq_count;

    /* Copy payload — truncate to max, always NUL-terminate */
    uint32_t copy_len = length;
    if (copy_len > RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U)
    {
        copy_len = RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U;
    }
    if ((NULL != p_data) && (copy_len > 0U))
    {
        memcpy(p_entry->event_data, p_data, copy_len);
    }
    p_entry->event_data[copy_len] = '\0';
    p_entry->event_data_len       = copy_len;

    /* Stamp whether the RTC had a user-set time when this event was triggered.
     * Entries logged before rs_rtc_logger_TimeSet() on cold start carry
     * time_valid = false and are formatted as [0000-00-00 00:00:00]. */
    p_entry->time_valid = p_ctrl->rtc_time_set;

    /* Advance head */
    p_ctrl->buf_head  = (head + 1U) % RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH;
    p_ctrl->buf_count = p_ctrl->buf_count + 1U;

    /* Update diagnostics */
    p_ctrl->events_logged++;
    p_ctrl->last_timestamp = *p_timestamp;

    /* --- Critical section end -------------------------------------------- */
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
    if (0U == saved_primask) { __enable_irq(); }
#endif

    return FSP_SUCCESS;
}

/**
 * @brief Dequeue the oldest entry from the circular buffer.
 *
 * Called from the consumer task (not from ISR). Updates buf_tail and buf_count.
 * Compiled in both Example Project mode and Expert mode so that the public
 * rs_rtc_logger_EventDequeue() wrapper can call it in either configuration.
 *
 * @param[in]  p_ctrl      PES control block.
 * @param[out] p_entry_out Entry populated on success.
 *
 * @return true if an entry was dequeued; false if buffer was empty.
 */
static bool rs_rl_dequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                           rs_rtc_logger_event_entry_t * p_entry_out)
{
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
#endif

    if (0U == p_ctrl->buf_count)
    {
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
        if (0U == saved_primask) { __enable_irq(); }
#endif
        return false;
    }

    uint32_t tail = p_ctrl->buf_tail;
    *p_entry_out  = p_ctrl->event_buf[tail];

    p_ctrl->buf_tail  = (tail + 1U) % RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH;
    p_ctrl->buf_count = p_ctrl->buf_count - 1U;

#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
    if (0U == saved_primask) { __enable_irq(); }
#endif

    return true;
}

/***********************************************************************************************************************
 * Example Project mode: internal consumer task and auto-init
 **********************************************************************************************************************/

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE

/**
 * @brief Perform deferred auto-Open()/Start() in Example Project mode.
 *
 * Called from the internal consumer task after detecting auto_init_pending.
 * Guarantees the first EventTrigger() event (already enqueued) is not lost.
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS  Auto-init completed successfully.
 * @retval (other)      Propagated from Open() or Start().
 */
static fsp_err_t rs_rl_auto_init(rs_rtc_logger_ctrl_t * p_ctrl)
{
    fsp_err_t err = rs_rtc_logger_Open(p_ctrl, p_ctrl->p_cfg);
    if (FSP_ERR_ALREADY_OPEN == err)
    {
        err = FSP_SUCCESS; /* Already opened — not an error in this path */
    }
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = rs_rtc_logger_Start(p_ctrl);
    if (FSP_ERR_INVALID_MODE == err)
    {
        err = FSP_SUCCESS; /* Already started */
    }

    if (FSP_SUCCESS == err)
    {
        p_ctrl->auto_init_done    = true;
        p_ctrl->auto_init_pending = false;
    }

    return err;
}

/**
 * @brief PES-owned internal consumer task (Example Project mode, FR-123).
 *
 * Waits for task notifications from EventTrigger() (or the UART TX-complete
 * callback), then drains the circular buffer by calling rs_rtc_logger_Write()
 * for each pending entry. Uses a TX timeout guard per FR-124.
 *
 * @param[in] p_arg  Pointer to rs_rtc_logger_ctrl_t (cast from void *).
 */
static void rs_rtc_logger_internal_consumer_task(void * p_arg)
{
    rs_rtc_logger_ctrl_t * p_ctrl = (rs_rtc_logger_ctrl_t *)p_arg;
    rs_rtc_logger_event_entry_t entry;

    for (;;)
    {
        /* Wait for notification from EventTrigger() or TX-complete callback */
        (void)ulTaskNotifyTake(pdTRUE,
                               pdMS_TO_TICKS(RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS));

        /* Auto-init on first wakeup if triggered before Open()/Start() */
        if (p_ctrl->auto_init_pending && !p_ctrl->auto_init_done)
        {
            (void)rs_rl_auto_init(p_ctrl);
        }

        /* Drain the buffer — transmit one entry per loop iteration */
        while (RS_RTC_LOGGER_STATE_STARTED == p_ctrl->state)
        {
            if (!rs_rl_dequeue(p_ctrl, &entry))
            {
                break; /* Buffer empty */
            }

            /* Transmit — blocks here until TX_COMPLETE notification or timeout */
            fsp_err_t err = rs_rtc_logger_Write(p_ctrl, &entry);
            if (FSP_SUCCESS == err)
            {
                /* Wait for UART TX complete before sending the next entry.
                 *
                 * BUG FIX (TestIssue_02): rs_rtc_logger_EventTrigger() calls
                 * vTaskNotifyGiveFromISR() on this task handle whenever a new
                 * event arrives.  If a button press fires while we are waiting
                 * here for TX_COMPLETE, that EventTrigger notification wakes
                 * this ulTaskNotifyTake() prematurely — before the UART has
                 * finished transmitting the current line_buf content.
                 *
                 * The consumer then dequeues the next entry and overwrites
                 * line_buf via rs_rtc_logger_Write() while the UART hardware
                 * shift register is still reading from it, producing corrupted
                 * output (e.g. "[[2026-06-29 09:57:52] Event Detected #18")
                 * and silently dropping the intervening event.
                 *
                 * FIX: loop on ulTaskNotifyTake() until p_ctrl->tx_complete is
                 * actually set by the UART ISR callback.  tx_complete is
                 * cleared to false by rs_rtc_logger_hal_uart_write() before
                 * every write and set to true only by the UART_EVENT_TX_COMPLETE
                 * handler — so the condition unambiguously identifies genuine
                 * TX completion vs. a spurious EventTrigger wake-up. */
                do
                {
                    (void)ulTaskNotifyTake(pdTRUE,
                                           pdMS_TO_TICKS(RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS));
                } while (!p_ctrl->tx_complete);
            }
            /* On error, skip this entry and continue draining */
        }
    }
}

#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

/***********************************************************************************************************************
 * Public function implementations — Lifecycle
 **********************************************************************************************************************/

/**
 * @brief Validate configuration, open RTC and UART HAL drivers, prepare PES state.
 *
 * @param[in] p_ctrl  Pointer to caller-allocated control block.
 * @param[in] p_cfg   Pointer to configuration structure.
 *
 * @retval FSP_SUCCESS              Opened successfully; state -> OPEN.
 * @retval FSP_ERR_ASSERTION        p_ctrl or p_cfg or required HAL pointers are NULL.
 * @retval FSP_ERR_ALREADY_OPEN     PES is not in CLOSED state.
 * @retval FSP_ERR_NOT_INITIALIZED  RTC oscillator not running.
 * @retval (other)                  Propagated from HAL Open() calls.
 */

/***********************************************************************************************************************
 * rs_rtc_logger_ConfigGen
 **********************************************************************************************************************/

/**
 * @brief Assign HAL instance pointers to the PES configuration structure.
 *
 * This is the configuration bridge function for the two-path approach
 * (Section 10.1): both the preset path (RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE=1)
 * and the FSP Stack Configurator path (=0) call this function with the
 * appropriate instance pointers before calling rs_rtc_logger_Open().
 */
fsp_err_t rs_rtc_logger_ConfigGen (rs_rtc_logger_cfg_t   * p_cfg,
                                     rtc_instance_t  const * p_rtc,
                                     uart_instance_t const * p_uart)
{
#if RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_cfg,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_rtc,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_uart, FSP_ERR_ASSERTION);
#endif

    p_cfg->p_rtc  = p_rtc;
    p_cfg->p_uart = p_uart;

    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_Open(rs_rtc_logger_ctrl_t       * p_ctrl,
                              rs_rtc_logger_cfg_t  const * p_cfg)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl,         FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_cfg,          FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_cfg->p_rtc,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_cfg->p_uart, FSP_ERR_ASSERTION);

    if (RS_RTC_LOGGER_STATE_CLOSED != p_ctrl->state)
    {
        return FSP_ERR_ALREADY_OPEN;
    }

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* Open() is called from inside the Example Project mode auto-init
     * sequence (rs_rl_auto_init(), invoked by the internal consumer task
     * itself). The unconditional memset() below previously wiped
     * internal_task_handle back to NULL, which made rs_rtc_logger_Start()
     * believe the task had never been created and call xTaskCreateStatic()
     * again on top of the very stack the running task was using — smashing
     * its own live call frames. Save the fields that must survive this
     * re-init, and restore them after. */
    TaskHandle_t saved_internal_task_handle = p_ctrl->internal_task_handle;
    bool         saved_auto_init_pending    = p_ctrl->auto_init_pending;
    bool         saved_auto_init_done       = p_ctrl->auto_init_done;
#endif

    /* Preserve any events already buffered (e.g. the EventTrigger() call
     * that kicked off this very auto-init sequence) — they would otherwise
     * be silently discarded by the reset below before ever being drained.
     *
     * ROOT CAUSE FIX: saved_event_buf is declared static so it lives in BSS
     * rather than on the stack.  The stack allocation (640 bytes at default
     * EVENT_BUF_DEPTH=8) was consuming the majority of the CLI task's 1024-byte
     * stack, leaving fewer than 212 bytes for hal_rtc_open() + R_RTC_Open(),
     * causing a hard fault in Expert mode.
     *
     * Open() is never called re-entrantly: Expert mode calls it once from the
     * CLI task; EP mode calls it once from the consumer task via rs_rl_auto_init().
     * The two build configurations are mutually exclusive, so a static local is
     * safe here. */
    static rs_rtc_logger_event_entry_t saved_event_buf[RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH];
    memcpy(saved_event_buf, p_ctrl->event_buf, sizeof(saved_event_buf));
    uint32_t saved_buf_head     = p_ctrl->buf_head;
    uint32_t saved_buf_tail     = p_ctrl->buf_tail;
    uint32_t saved_buf_count    = p_ctrl->buf_count;
    /* FIX DC-7 (extended): preserve ALL diagnostic counters across a failed-then-
     * retried Open() so no data is silently lost.  seq_count and events_logged
     * were already saved (v02).  events_dropped may be non-zero if EventTrigger()
     * overflowed the buffer before Open() succeeded; tx_errors is always 0 in
     * CLOSED state but is saved for completeness. */
    uint32_t saved_seq_count     = p_ctrl->seq_count;
    uint32_t saved_events_logged = p_ctrl->events_logged;
    uint32_t saved_events_dropped = p_ctrl->events_dropped;
    uint32_t saved_tx_errors     = p_ctrl->tx_errors;

    /* Zero control block, preserve nothing from before */
    memset(p_ctrl, 0, sizeof(rs_rtc_logger_ctrl_t));

    /* Set versioning fields (FR-096) */
    p_ctrl->version    = RS_RTC_LOGGER_VERSION;
    p_ctrl->ctrl_size  = (uint32_t)sizeof(rs_rtc_logger_ctrl_t);

    /* Restore buffered events and counters */
    memcpy(p_ctrl->event_buf, saved_event_buf, sizeof(saved_event_buf));
    p_ctrl->buf_head        = saved_buf_head;
    p_ctrl->buf_tail        = saved_buf_tail;
    p_ctrl->buf_count       = saved_buf_count;
    p_ctrl->seq_count       = saved_seq_count;
    p_ctrl->events_logged   = saved_events_logged;
    p_ctrl->events_dropped  = saved_events_dropped;
    p_ctrl->tx_errors       = saved_tx_errors;

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* Restore the EP-mode task handle and auto-init flags */
    p_ctrl->internal_task_handle = saved_internal_task_handle;
    p_ctrl->auto_init_pending    = saved_auto_init_pending;
    p_ctrl->auto_init_done       = saved_auto_init_done;
#endif

    /* Bind configuration */
    p_ctrl->p_cfg     = p_cfg;
    p_ctrl->ts_format = p_cfg->ts_format;   /* DC-8: shadow avoids const-cast in TimestampFormatSet() */

    /* Open RTC (validates oscillator) */
    fsp_err_t err = rs_rtc_logger_hal_rtc_open(p_ctrl);
    if (FSP_SUCCESS != err)
    {
        memset(p_ctrl, 0, sizeof(rs_rtc_logger_ctrl_t));
        memcpy(p_ctrl->event_buf, saved_event_buf, sizeof(saved_event_buf));
        p_ctrl->buf_head        = saved_buf_head;
        p_ctrl->buf_tail        = saved_buf_tail;
        p_ctrl->buf_count       = saved_buf_count;
        p_ctrl->seq_count       = saved_seq_count;
        p_ctrl->events_logged   = saved_events_logged;
        p_ctrl->events_dropped  = saved_events_dropped;
        p_ctrl->tx_errors       = saved_tx_errors;
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
        /* Keep auto_init_pending true (this attempt failed, not abandoned)
         * so the next notify-timeout in the consumer task's loop retries —
         * but the task itself, and the handle that identifies it, must
         * still be the ones already created. Do NOT restore auto_init_done. */
        p_ctrl->internal_task_handle = saved_internal_task_handle;
        p_ctrl->auto_init_pending    = saved_auto_init_pending;
#endif
        return err;
    }

    /* Open UART */
    err = rs_rtc_logger_hal_uart_open(p_ctrl);
    if (FSP_SUCCESS != err)
    {
        (void)rs_rtc_logger_hal_rtc_close(p_ctrl);
        memset(p_ctrl, 0, sizeof(rs_rtc_logger_ctrl_t));
        memcpy(p_ctrl->event_buf, saved_event_buf, sizeof(saved_event_buf));
        p_ctrl->buf_head        = saved_buf_head;
        p_ctrl->buf_tail        = saved_buf_tail;
        p_ctrl->buf_count       = saved_buf_count;
        p_ctrl->seq_count       = saved_seq_count;
        p_ctrl->events_logged   = saved_events_logged;
        p_ctrl->events_dropped  = saved_events_dropped;
        p_ctrl->tx_errors       = saved_tx_errors;
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
        p_ctrl->internal_task_handle = saved_internal_task_handle;
        p_ctrl->auto_init_pending    = saved_auto_init_pending;
#endif
        return err;
    }

    p_ctrl->state = RS_RTC_LOGGER_STATE_OPEN;

    /* rtc_time_set starts false regardless of reset type.  The calling
     * application is responsible for calling rs_rtc_logger_ClockValidate()
     * immediately after Open() to signal that the RTC holds a user-confirmed
     * time (e.g. it was set in a previous session, proven by a noinit magic
     * sentinel that survived the warm reset).  This prevents timestamps from
     * a factory-default or never-set RTC (2000-01-01 00:00:00) from appearing
     * as real event times after a warm reset.                               */
    p_ctrl->rtc_time_set = false;

    return FSP_SUCCESS;
}

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
/**
 * @brief Create the internal consumer task for Example Project mode.
 *
 * MUST be called from task context before the first rs_rtc_logger_EventTrigger()
 * call that may arrive from an ISR. xTaskCreateStatic() is illegal in ISR
 * context; this function moves task creation to a safe call site.
 *
 * @param[in] p_ctrl  PES control block. p_cfg must already be set.
 *
 * @retval FSP_SUCCESS          Task created (or already existed).
 * @retval FSP_ERR_ASSERTION    p_ctrl or p_ctrl->p_cfg is NULL.
 * @retval FSP_ERR_OUT_OF_MEMORY xTaskCreateStatic returned NULL.
 */
fsp_err_t rs_rtc_logger_EP_PrepareConsumerTask(rs_rtc_logger_ctrl_t * p_ctrl)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl,         FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl->p_cfg,  FSP_ERR_ASSERTION);

    if (NULL != p_ctrl->internal_task_handle)
    {
        return FSP_SUCCESS; /* Already created */
    }

    p_ctrl->internal_task_handle =
        xTaskCreateStatic(rs_rtc_logger_internal_consumer_task,
                          "rtc_logger",
                          RS_RTC_LOGGER_CFG_TASK_STACK_SIZE / sizeof(StackType_t),
                          (void *)p_ctrl,
                          RS_RTC_LOGGER_CFG_TASK_PRIORITY,
                          g_rs_rl_internal_task_stack,
                          &g_rs_rl_internal_task_tcb);

    if (NULL == p_ctrl->internal_task_handle)
    {
        return FSP_ERR_OUT_OF_MEMORY;
    }

    return FSP_SUCCESS;
}
#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

/**
 * @brief Enable active UART output path.
 *
 * @param[in] p_ctrl  Pointer to PES control block.
 *
 * @retval FSP_SUCCESS           Started; state -> STARTED.
 * @retval FSP_ERR_NOT_OPEN      PES is not in OPEN or STOPPED state.
 * @retval FSP_ERR_INVALID_MODE  PES is already STARTED.
 */
fsp_err_t rs_rtc_logger_Start(rs_rtc_logger_ctrl_t * p_ctrl)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    if (RS_RTC_LOGGER_STATE_STARTED == p_ctrl->state)
    {
        return FSP_ERR_INVALID_MODE;
    }

    if ((RS_RTC_LOGGER_STATE_OPEN    != p_ctrl->state) &&
        (RS_RTC_LOGGER_STATE_STOPPED != p_ctrl->state))
    {
        return FSP_ERR_NOT_OPEN;
    }

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* N-7 fix: delegate task creation to EP_PrepareConsumerTask() instead of
     * duplicating the xTaskCreateStatic() call here.  Previously Start() held
     * an identical copy of the task-creation block; any parameter change had to
     * be made in two places and the copies could silently diverge. */
    fsp_err_t task_err = rs_rtc_logger_EP_PrepareConsumerTask(p_ctrl);
    if (FSP_SUCCESS != task_err)
    {
        return task_err;
    }
#endif

    p_ctrl->state = RS_RTC_LOGGER_STATE_STARTED;

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* Notify consumer task to drain any events buffered before Start() */
    if (NULL != p_ctrl->internal_task_handle)
    {
        xTaskNotifyGive(p_ctrl->internal_task_handle);
    }
#endif

    return FSP_SUCCESS;
}

/**
 * @brief Suspend UART output, preserving configuration and buffer.
 *
 * @param[in] p_ctrl  Pointer to PES control block.
 *
 * @retval FSP_SUCCESS       Stopped; state -> STOPPED.
 * @retval FSP_ERR_NOT_OPEN  PES is not in STARTED state.
 */
fsp_err_t rs_rtc_logger_Stop(rs_rtc_logger_ctrl_t * p_ctrl)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    /* FIX CD-1: distinguish "already stopped" (INVALID_MODE) from
     * "not open at all" (NOT_OPEN) so the caller can tell them apart. */
    if (RS_RTC_LOGGER_STATE_STOPPED == p_ctrl->state)
    {
        return FSP_ERR_INVALID_MODE;
    }

    if (RS_RTC_LOGGER_STATE_STARTED != p_ctrl->state)
    {
        return FSP_ERR_NOT_OPEN;
    }

    p_ctrl->state = RS_RTC_LOGGER_STATE_STOPPED;

    return FSP_SUCCESS;
}

/**
 * @brief Release all PES resources and close underlying HAL drivers.
 *
 * @param[in] p_ctrl  Pointer to PES control block.
 *
 * @retval FSP_SUCCESS       Closed; state -> CLOSED.
 * @retval FSP_ERR_NOT_OPEN  PES is not in OPEN or STOPPED state.
 */
fsp_err_t rs_rtc_logger_Close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    /* FIX CD-3: reject Close() while STARTED — the internal consumer task may
     * be executing Write() concurrently; a memset() over the live ctrl block
     * would cause an immediate NULL-dereference in hal_uart_write().
     * The documented contract says "call Stop() first"; enforce it here. */
    if (RS_RTC_LOGGER_STATE_STARTED == p_ctrl->state)
    {
        return FSP_ERR_INVALID_MODE;
    }

    if ((RS_RTC_LOGGER_STATE_OPEN    != p_ctrl->state) &&
        (RS_RTC_LOGGER_STATE_STOPPED != p_ctrl->state))
    {
        return FSP_ERR_NOT_OPEN;
    }

    /* Close drivers in reverse order: UART first, then RTC */
    (void)rs_rtc_logger_hal_uart_close(p_ctrl);
    (void)rs_rtc_logger_hal_rtc_close(p_ctrl);

    /* Zero the control block; allows reuse with a subsequent Open() */
    memset(p_ctrl, 0, sizeof(rs_rtc_logger_ctrl_t));
    /* State is now CLOSED (== 0 from the enum) */

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * Public function implementations — Operational
 **********************************************************************************************************************/

/**
 * @brief ISR-safe event capture entry point.
 *
 * @param[in] p_ctrl  PES control block.
 * @param[in] p_data  Event payload pointer.
 * @param[in] length  Payload byte count.
 *
 * @retval FSP_SUCCESS         Event enqueued.
 * @retval FSP_ERR_NOT_OPEN    Expert mode: PES not in STARTED or STOPPED state.
 * @retval FSP_ERR_OVERFLOW Buffer full and DROP_NEWEST policy.
 */
fsp_err_t rs_rtc_logger_EventTrigger(rs_rtc_logger_ctrl_t * p_ctrl,
                                      uint8_t const        * p_data,
                                      uint32_t               length)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* Example Project mode: handle CLOSED state on first call (FR-121, FR-122) */
    if (RS_RTC_LOGGER_STATE_CLOSED == p_ctrl->state)
    {
        /* The config pointer must have been set before EventTrigger() in EP mode.
         * If not, there is nothing we can do — return ERR_NOT_OPEN. */
        if (NULL == p_ctrl->p_cfg)
        {
            return FSP_ERR_NOT_OPEN;
        }

        /* Enqueue immediately so no event is lost */
        rtc_time_t zero_time = {0};
        (void)rs_rl_enqueue(p_ctrl, &zero_time, p_data, length);

        /* Mark auto-init as pending and notify the consumer task.
         * NOTE: xTaskCreateStatic() must NOT be called here — this code
         * runs in ISR context (r_icu_isr). The internal consumer task must
         * already have been created from task context (e.g. CLI task) by
         * calling rs_rtc_logger_EP_PrepareConsumerTask() before the first
         * EventTrigger() call from an ISR. */
        if (!p_ctrl->auto_init_pending)
        {
            p_ctrl->auto_init_pending = true;
        }

        /* Notify consumer task (ISR-safe) — it handles Open()/Start() */
        if (NULL != p_ctrl->internal_task_handle)
        {
            BaseType_t higher_prio_woken = pdFALSE;
            vTaskNotifyGiveFromISR(p_ctrl->internal_task_handle, &higher_prio_woken);
            portYIELD_FROM_ISR(higher_prio_woken);
        }

        return FSP_SUCCESS;
    }
#else
    /* Expert mode: must be STARTED or STOPPED */
    if ((RS_RTC_LOGGER_STATE_STARTED != p_ctrl->state) &&
        (RS_RTC_LOGGER_STATE_STOPPED != p_ctrl->state))
    {
        return FSP_ERR_NOT_OPEN;
    }
#endif

    /* Normal enqueue path (STARTED or STOPPED) */
    rtc_time_t ts;
    fsp_err_t  err = rs_rtc_logger_hal_rtc_time_get(p_ctrl, &ts);
    if (FSP_SUCCESS != err)
    {
        /* Fallback: use last known timestamp rather than losing the event */
        ts = p_ctrl->last_timestamp;
    }

    err = rs_rl_enqueue(p_ctrl, &ts, p_data, length);

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    /* Notify internal consumer task (from ISR context) */
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->internal_task_handle))
    {
        BaseType_t higher_prio_woken = pdFALSE;
        vTaskNotifyGiveFromISR(p_ctrl->internal_task_handle, &higher_prio_woken);
        portYIELD_FROM_ISR(higher_prio_woken);
    }
#endif

    return err;
}

/**
 * @brief Transmit a pre-composed timestamped line over UART (VCOM).
 *
 * @param[in] p_ctrl   PES control block.
 * @param[in] p_entry  Pointer to dequeued circular buffer entry.
 *
 * @retval FSP_SUCCESS           Transmission initiated.
 * @retval FSP_ERR_NOT_OPEN      PES is not in STARTED state.
 * @retval FSP_ERR_INVALID_SIZE  Composed line exceeds line_buf.
 * @retval (other)               Propagated from UART HAL write.
 */
fsp_err_t rs_rtc_logger_Write(rs_rtc_logger_ctrl_t              * p_ctrl,
                               rs_rtc_logger_event_entry_t const * p_entry)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl,  FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_entry, FSP_ERR_ASSERTION);

    if (RS_RTC_LOGGER_STATE_STARTED != p_ctrl->state)
    {
        return FSP_ERR_NOT_OPEN;
    }

    uint32_t len = rs_rtc_logger_format_line(p_entry,
                                              p_ctrl->ts_format,          /* DC-8: use shadow, not p_cfg cast */
                                              p_ctrl->p_cfg->ts_brackets,
                                              p_ctrl->p_cfg->line_term,
                                              p_ctrl->line_buf,
                                              RS_RTC_LOGGER_CFG_LINE_BUF_SIZE);
    if (0U == len)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    return rs_rtc_logger_hal_uart_write(p_ctrl,
                                        (uint8_t const *)p_ctrl->line_buf,
                                        len);
}

/**
 * @brief Convenience overload: write a NUL-terminated string with live RTC timestamp.
 *
 * @param[in] p_ctrl  PES control block.
 * @param[in] p_str   NUL-terminated ASCII string.
 *
 * @retval FSP_SUCCESS    Transmission initiated.
 * @retval (other)        Same as rs_rtc_logger_Write().
 */
fsp_err_t rs_rtc_logger_WriteStr(rs_rtc_logger_ctrl_t * p_ctrl,
                                  char const           * p_str)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_str,  FSP_ERR_ASSERTION);

    if (RS_RTC_LOGGER_STATE_STARTED != p_ctrl->state)
    {
        return FSP_ERR_NOT_OPEN;
    }

    /* Compose a temporary entry with live RTC timestamp */
    rs_rtc_logger_event_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    /* FIX B-1: stamp time validity so format_line() shows a real timestamp
     * when the RTC has been set, not the null sentinel [0000-00-00 00:00:00]. */
    entry.time_valid = p_ctrl->rtc_time_set;

    fsp_err_t err = rs_rtc_logger_hal_rtc_time_get(p_ctrl, &entry.timestamp);
    if (FSP_SUCCESS != err)
    {
        entry.timestamp = p_ctrl->last_timestamp;
    }

    entry.seq_count = p_ctrl->seq_count; /* Use current (not incremented) for WriteStr */

    uint32_t str_len = 0U;
    while ((str_len < RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U) &&
           ('\0' != p_str[str_len]))
    {
        entry.event_data[str_len] = (uint8_t)p_str[str_len];
        str_len++;
    }
    entry.event_data[str_len] = '\0';
    entry.event_data_len      = str_len;

    return rs_rtc_logger_Write(p_ctrl, &entry);
}

/**
 * @brief Dequeue the oldest event entry from the circular buffer.
 *
 * DC-6 fix: provides Expert-mode consumer tasks with a safe, canonical dequeue
 * path that uses the same critical-section logic as the internal EP-mode consumer
 * task.  Eliminates the need for application code to access buf_tail / buf_count
 * directly (which previously caused a TOCTOU race in demo_consumer_task()).
 *
 * @param[in]  p_ctrl      PES control block.
 * @param[out] p_entry_out Caller-allocated entry; populated on success.
 *
 * @retval FSP_SUCCESS       Entry dequeued into p_entry_out.
 * @retval FSP_ERR_NOT_OPEN  PES is CLOSED.
 * @retval FSP_ERR_ASSERTION p_ctrl or p_entry_out is NULL.
 * @retval FSP_ERR_BUFFER_EMPTY Buffer empty — no entry available.
 *
 * @note Task context only — not ISR-safe. Valid in: OPEN, STARTED, STOPPED.
 */
fsp_err_t rs_rtc_logger_EventDequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                                      rs_rtc_logger_event_entry_t * p_entry_out)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl,      FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_entry_out, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    if (!rs_rl_dequeue(p_ctrl, p_entry_out))
    {
        return FSP_ERR_BUFFER_EMPTY; /* FIX: was FSP_ERR_OVERFLOW (wrong semantic) */
    }

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * Public function implementations — Configuration
 **********************************************************************************************************************/

/**
 * @brief Change the timestamp format at runtime.
 *
 * @param[in] p_ctrl   Pointer to PES control block.
 * @param[in] format   New timestamp format.
 *
 * @retval FSP_SUCCESS       Format updated.
 * @retval FSP_ERR_NOT_OPEN  PES is CLOSED.
 * @retval FSP_ERR_ASSERTION Invalid format value.
 */
fsp_err_t rs_rtc_logger_TimestampFormatSet(rs_rtc_logger_ctrl_t    * p_ctrl,
                                            rs_rtc_logger_ts_format_t format)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(format <= RS_RTC_LOGGER_TS_FORMAT_SYSLOG, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    /* DC-8 fix: write to the ctrl-owned ts_format shadow instead of casting
     * away const from p_cfg.  p_cfg remains truly const throughout. */
    p_ctrl->ts_format = format;

    return FSP_SUCCESS;
}

/**
 * @brief Set the RTC calendar date and time.
 *
 * @param[in] p_ctrl  Pointer to PES control block.
 * @param[in] p_time  Pointer to RTC time to set.
 *
 * @retval FSP_SUCCESS   RTC time set.
 * @retval FSP_ERR_NOT_OPEN  PES is CLOSED.
 * @retval (other)       Propagated from RTC HAL.
 */
fsp_err_t rs_rtc_logger_TimeSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                 rtc_time_t     const * p_time)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_time, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    fsp_err_t err = rs_rtc_logger_hal_rtc_time_set(p_ctrl, (rtc_time_t *)p_time); /* cast: FSP calendarTimeSet takes non-const */

    if (FSP_SUCCESS == err)
    {
        /* Mark the RTC time as user-validated so subsequent events receive
         * a real timestamp instead of [0000-00-00 00:00:00]. */
        p_ctrl->rtc_time_set = true;
    }

    return err;
}

/**
 * @brief Explicitly validate (or invalidate) the RTC clock state.
 *
 * Called by the application immediately after rs_rtc_logger_Open() to
 * communicate whether the RTC currently holds a user-confirmed time.
 *
 * rs_rtc_logger_Open() always initialises rtc_time_set to false.  The
 * application should pass time_is_valid = true only when both conditions
 * hold:
 *   1. This is a warm reset (PORF == 0), so the RTC kept counting.
 *   2. A noinit magic sentinel set by a previous rs_rtc_logger_TimeSet()
 *      call survived the reset, proving the counter holds a real time.
 *
 * Without this call the first event on a warm reset after an unset cold
 * start would display the factory-default RTC value (2000-01-01 00:00:00)
 * instead of [0000-00-00 00:00:00].
 *
 * @param[in] p_ctrl        Pointer to PES control block.
 * @param[in] time_is_valid true  = RTC holds a user-confirmed time;
 *                          false = treat as unset (display null timestamp).
 *
 * @retval FSP_SUCCESS      Flag updated.
 * @retval FSP_ERR_NOT_OPEN PES is CLOSED.
 * @retval FSP_ERR_ASSERTION p_ctrl is NULL.
 */
fsp_err_t rs_rtc_logger_ClockValidate(rs_rtc_logger_ctrl_t * p_ctrl,
                                       bool                   time_is_valid)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    p_ctrl->rtc_time_set = time_is_valid;
    return FSP_SUCCESS;
}

/**
 * @brief Compensate for LOCO oscillator drift by updating the RFRL register.
 *
 * See rs_rtc_logger.h for full documentation and tuning guide.
 *
 * @retval FSP_SUCCESS              RFRL updated; RTC counter restarted.
 * @retval FSP_ERR_NOT_OPEN         PES is CLOSED.
 * @retval FSP_ERR_INVALID_ARGUMENT rfrl_value outside valid range (8–510).
 * @retval (other)                  Propagated from RTC HAL.
 */
fsp_err_t rs_rtc_logger_LocoCalibrationSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                            uint16_t               rfrl_value)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    return rs_rtc_logger_hal_loco_freq_set(p_ctrl, rfrl_value);
}

/**
 * @brief Restore the event sequence counter after a warm reset.
 *
 * @param[in] p_ctrl      Pointer to PES control block.
 * @param[in] seq_count   Sequence counter value to restore.
 *
 * @retval FSP_SUCCESS       Counter restored.
 * @retval FSP_ERR_NOT_OPEN  PES is not in OPEN or STOPPED state.
 */
fsp_err_t rs_rtc_logger_SeqCountSet(rs_rtc_logger_ctrl_t * p_ctrl,
                                     uint32_t               seq_count)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);

    /* Allow restore from any non-CLOSED state.
     * In Example Project mode the PES reaches STARTED before the CLI task
     * can call SeqCountSet() after auto-init, so OPEN and STOPPED alone are
     * too restrictive. A 32-bit aligned store is atomic on Cortex-M33. */
    if (RS_RTC_LOGGER_STATE_CLOSED == p_ctrl->state)
    {
        return FSP_ERR_NOT_OPEN;
    }

    p_ctrl->seq_count = seq_count;

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 * Public function implementations — Query
 **********************************************************************************************************************/

/**
 * @brief Get a snapshot of PES runtime status and diagnostics.
 *
 * @param[in]  p_ctrl    Pointer to PES control block.
 * @param[out] p_status  Caller-allocated status structure.
 *
 * @retval FSP_SUCCESS       Status populated.
 * @retval FSP_ERR_NOT_OPEN  PES is CLOSED.
 * @retval FSP_ERR_ASSERTION p_status is NULL.
 */
fsp_err_t rs_rtc_logger_StatusGet(rs_rtc_logger_ctrl_t   * p_ctrl,
                                   rs_rtc_logger_status_t * p_status)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl,   FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_status, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    p_status->state          = p_ctrl->state;
    p_status->events_logged  = p_ctrl->events_logged;
    p_status->events_dropped = p_ctrl->events_dropped;
    p_status->tx_errors      = p_ctrl->tx_errors;
    p_status->seq_count      = p_ctrl->seq_count;
    p_status->buf_count      = p_ctrl->buf_count;
    p_status->last_timestamp = p_ctrl->last_timestamp;

    return FSP_SUCCESS;
}

/**
 * @brief Read the current RTC date and time directly.
 *
 * @param[in]  p_ctrl  Pointer to PES control block.
 * @param[out] p_time  Populated with current RTC value.
 *
 * @retval FSP_SUCCESS   Time populated.
 * @retval FSP_ERR_NOT_OPEN  PES is CLOSED.
 * @retval (other)       Propagated from RTC HAL.
 */
fsp_err_t rs_rtc_logger_TimeGet(rs_rtc_logger_ctrl_t * p_ctrl,
                                 rtc_time_t           * p_time)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_ctrl, FSP_ERR_ASSERTION);
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_time, FSP_ERR_ASSERTION);

    if (!rs_rl_is_open(p_ctrl))
    {
        return FSP_ERR_NOT_OPEN;
    }

    return rs_rtc_logger_hal_rtc_time_get(p_ctrl, p_time);
}

/**
 * @brief Get PES module version.
 *
 * @param[out] p_version  Populated with version fields.
 *
 * @retval FSP_SUCCESS always.
 */
fsp_err_t rs_rtc_logger_VersionGet(uint32_t * p_version)
{
    RS_RTC_LOGGER_ASSERT_RETURN(NULL != p_version, FSP_ERR_ASSERTION);

    /* DC-3 fix: use the single source-of-truth macro rather than
     * duplicating the shift/OR encoding that already lives in rs_rtc_logger.h. */
    *p_version = RS_RTC_LOGGER_VERSION;

    return FSP_SUCCESS;
}
