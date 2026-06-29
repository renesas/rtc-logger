/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_demo.c
 * @brief Reference application for the RTC Event Logger  — FPB-RA4E2.
 *
 * Mode selection (compile-time):
 *   RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1 (default) — Example Project mode
 *   RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 0           — Expert mode
 *
 * Expected VCOM output — Example Project mode (cold start):
 *   *** RTC Event Logger Demo ***
 *   Mode: Example Project
 *   Creating RTC Event Logger consumer task...
 *   Consumer task ready.
 *   Triggering RTC Event Logger auto-init...
 *   Waiting for RTC Event Logger auto-init to complete...
 *   [0000-00-00 00:00:00] INIT #1
 *   RTC Event Logger auto-init complete.
 *   Power-on reset detected.
 *   Enter date/time [YYYY-MM-DD HH:MM:SS] : 2024-07-15 14:30:00
 *   Time set OK.
 *   LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: +8
 *   LOCO calibration set: trim +8, RFRL = 263.
 *   Ready. Press S1 to log an event.
 *   [2024-07-15 14:30:05] Event Detected #2
 *
 * Expected VCOM output — Expert mode (cold start):
 *   *** RTC Event Logger Demo ***
 *   Mode: Expert
 *   Opening RTC Event Logger...
 *   RTC Event Logger open OK.
 *   Power-on reset detected.
 *   Enter date/time [YYYY-MM-DD HH:MM:SS] : 2024-07-15 14:30:00
 *   Time set OK.
 *   LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: +8
 *   LOCO calibration set: trim +8, RFRL = 263.
 *   Starting RTC Event Logger...
 *   RTC Event Logger started.
 *   Ready. Press S1 to log an event.
 *   [2024-07-15 14:30:05] Event Detected #1
 *
 * Features shared by both modes (located in the shared section of demo_cli_task):
 *   - Cold start date/time prompt (skippable; re-prompts on bad format)
 *   - LOCO oscillator trim calibration prompt
 *   - Warm reset message ("RTC time retained")
 *   - Time-set guard: monitors events_logged and re-prompts after each
 *     event posted before the clock is set; displays [0000-00-00 00:00:00]
 *   - "Ready. Press S1 to log an event."
 *
 * Log message completion notes:
 *   EP mode:    All status strings are printed (TX confirmed) BEFORE
 *               rs_rtc_logger_EventTrigger() is called.  EventTrigger()
 *               wakes the consumer task which calls UART close()+reopen();
 *               any in-flight TX would be aborted mid-string at that point.
 *   Expert mode: rs_rtc_logger_Open() is called synchronously from the CLI
 *               task, always after a demo_uart_write_blocking() that has
 *               already confirmed TX_COMPLETE — no race condition exists
 *               and no special ordering is required.
 */

#include <rs_rtc_logger/rs_rtc_logger.h>
#include <rs_rtc_logger/rs_rtc_logger_cfg.h> /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE, RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */
#include "common_data.h"       /* g_external_irq9 */
#include "hal_data.h"          /* g_rtc0, g_uart0 (FSP-generated) */
#include "r_sci_uart.h"        /* R_SCI_UART_Write */

/* Configuration path (Section 10.1):
 *   PRESET_CONFIG_USE = 1 (default): ConfigGen uses _preset instances from
 *     the board config file — no FSP Stack Configurator re-run required.
 *   PRESET_CONFIG_USE = 0: ConfigGen uses FSP-generated g_rtc0 / g_uart0.
 */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
 #include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>
#endif

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

/***********************************************************************************************************************
 * Configuration
 **********************************************************************************************************************/

/* ROOT CAUSE FIX (stack): CLI task needs headroom for rs_rtc_logger_Open() +
 * R_RTC_Open() + R_SCI_UART_Open() call depth.  The original 1024 bytes left
 * fewer than ~200 bytes by the time R_RTC_Open() entered, causing a hard fault
 * in Expert mode.  The saved_event_buf static fix is the primary remedy, but
 * 2048 bytes also provides safe margin for any future FSP function growth. */
#define DEMO_CLI_TASK_STACK_SIZE        (2048U)
#define DEMO_CLI_TASK_PRIORITY          (tskIDLE_PRIORITY + 2U)

/* Expert mode consumer task: 512 bytes was too tight for rs_rtc_logger_Write()
 * + rs_rtc_logger_format_line() + R_SCI_UART_Write() call depth. */
#define DEMO_CONSUMER_TASK_STACK_SIZE   (1024U)
#define DEMO_CONSUMER_TASK_PRIORITY     (tskIDLE_PRIORITY + 1U)

#define DEMO_EVENT_PAYLOAD              "Event Detected"
/* DC-4 fix: derive length from the string literal so it can never silently diverge. */
#define DEMO_EVENT_PAYLOAD_LEN          (sizeof(DEMO_EVENT_PAYLOAD) - 1U)

#define DEMO_INIT_PAYLOAD               "INIT"
#define DEMO_INIT_PAYLOAD_LEN           (sizeof(DEMO_INIT_PAYLOAD) - 1U)

/** RX line buffer — filled one byte at a time by UART_EVENT_RX_CHAR ISR */
#define DEMO_RX_BUF_SIZE                (64U)
#define DEMO_TX_TIMEOUT_MS              (2000U)

/***********************************************************************************************************************
 * RTC Event Logger control block — declared first so all functions in this file can access it
 **********************************************************************************************************************/

static rs_rtc_logger_ctrl_t g_logger_ctrl;

/***********************************************************************************************************************
 * PES configuration — non-instance fields set here; p_rtc and p_uart are
 * populated at runtime by rs_rtc_logger_ConfigGen() (Section 10.1).
 **********************************************************************************************************************/

static rs_rtc_logger_cfg_t g_logger_cfg =
{
    /* p_rtc and p_uart set by rs_rtc_logger_ConfigGen() */
    .p_rtc           = NULL,
    .p_uart          = NULL,
    .ts_format       = RS_RTC_LOGGER_TS_FORMAT_ISO8601,
    .ts_brackets     = true,
    .line_term       = RS_RTC_LOGGER_TERM_CRLF,
    .overflow_policy = RS_RTC_LOGGER_OVERFLOW_OVERWRITE_OLDEST,
    .p_callback      = NULL,
    .p_context       = NULL,
};


/***********************************************************************************************************************
 * Warm-reset-safe sequence counter backup — placed in .ram_noinit (FR-064).
 * The BSP startup never clears this section, so values survive warm resets.
 * A magic sentinel prevents treating uninitialised POR RAM as a valid backup.
 *
 * Save   : demo_button_s1_callback() after each EventTrigger()
 * Restore: demo_cli_task() after PES init, warm reset only
 **********************************************************************************************************************/

#define DEMO_SEQ_BACKUP_MAGIC  (0x5E91BA5EU)

BSP_PLACE_IN_SECTION(BSP_SECTION_NOINIT) static uint32_t g_seq_count_backup;
BSP_PLACE_IN_SECTION(BSP_SECTION_NOINIT) static uint32_t g_seq_count_magic;

/**
 * Noinit sentinel that proves the user set a valid RTC time in a previous
 * session.  Written alongside every successful rs_rtc_logger_TimeSet() call.
 * Checked on warm reset to decide whether the retained RTC value is trustworthy.
 *
 * Without this guard, a warm reset after a cold start where the user never
 * entered a time shows the RTC default [2000-01-01 00:00:00] instead of
 * [0000-00-00 00:00:00], because rs_rtc_logger_Open() can't distinguish a
 * user-set counter from a factory-default one via PORF alone.
 */
#define DEMO_TIME_SET_MAGIC    (0xC10C75E7U)

BSP_PLACE_IN_SECTION(BSP_SECTION_NOINIT) static uint32_t g_time_set_magic;

/***********************************************************************************************************************
 * RX ring buffer — written from UART ISR, read from CLI task
 **********************************************************************************************************************/

static volatile uint8_t  g_rx_buf[DEMO_RX_BUF_SIZE];
static volatile uint32_t g_rx_head = 0U;   /* written by ISR  */
static volatile uint32_t g_rx_tail = 0U;   /* read  by task   */

/** Called from rs_rtc_logger_uart_callback (ISR context) to enqueue one RX byte. */
void demo_uart_rx_char_enqueue(uint8_t c)
{
    uint32_t next = (g_rx_head + 1U) % DEMO_RX_BUF_SIZE;
    if (next != g_rx_tail)              /* drop if full */
    {
        g_rx_buf[g_rx_head] = c;
        g_rx_head = next;
    }
}

/**
 * @brief Called from the early UART callback in blinky_thread_entry.c (ISR context).
 *
 * The UART is opened by demo_banner_setup() before the RTC Event Logger is
 * initialised.  demo_uart_write_blocking() polls g_logger_ctrl.tx_complete to
 * detect TX completion.  Until the logger opens the UART and installs its own
 * callback, no one sets that flag — so all CLI task writes appeared to hang
 * (timeout) or silently fail.
 *
 * This function is called from demo_early_uart_callback() on every
 * UART_EVENT_TX_COMPLETE so that demo_uart_write_blocking() works correctly
 * from the moment the CLI task starts, before and after the logger takes
 * ownership of the UART.
 */
void demo_uart_early_tx_complete(void)
{
    g_logger_ctrl.tx_complete = true;
}

/** Dequeue one byte; returns false if buffer empty. Task context only. */
static bool demo_rx_getc(uint8_t * p_c)
{
    if (g_rx_head == g_rx_tail)
    {
        return false;
    }
    *p_c = g_rx_buf[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1U) % DEMO_RX_BUF_SIZE;
    return true;
}

/***********************************************************************************************************************
 * Blocking TX helper — uses the UART directly (bypasses RTC Event Logger log formatting)
 * for raw CLI output so the prompt appears even before the RTC Event Logger is open.
 **********************************************************************************************************************/

static void demo_uart_write_blocking(char const * p_str)
{
    if (NULL == p_str) { return; }
    uint32_t len = (uint32_t)strlen(p_str);
    if (0U == len) { return; }

    /* Clear the RTC Event Logger tx_complete flag before writing so we can detect
     * when the TX ISR fires for this specific transfer.              */
    g_logger_ctrl.tx_complete = false;

    fsp_err_t err = R_SCI_UART_Write(&g_uart0_ctrl,
                                     (uint8_t const *)p_str,
                                     len);
    if (FSP_SUCCESS != err)
    {
        /* UART not open yet or error — restore flag and return */
        g_logger_ctrl.tx_complete = true;
        return;
    }

    /* Wait for UART_EVENT_TX_COMPLETE (set by rs_rtc_logger_uart_callback) */
    uint32_t timeout = DEMO_TX_TIMEOUT_MS / 5U;
    while (!g_logger_ctrl.tx_complete && timeout-- > 0U)
    {
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}

/***********************************************************************************************************************
 * Blocking RX line input — reads until '\n' or '\r', echoes each character
 **********************************************************************************************************************/

/** Read a CR/LF-terminated line into buf (NUL-terminated, CR/LF stripped).
 *  Echoes each character to VCOM so the user can see what they type.
 *  Blocks indefinitely until a newline is received. */
static void demo_uart_readline(char * buf, uint32_t buf_size)
{
    uint32_t idx = 0U;
    memset(buf, 0, buf_size);

    while (1)
    {
        uint8_t c;
        if (!demo_rx_getc(&c))
        {
            vTaskDelay(pdMS_TO_TICKS(5U));
            continue;
        }

        if (c == '\r' || c == '\n')
        {
            demo_uart_write_blocking("\r\n");   /* Echo newline */
            break;
        }

        if (c == '\b' || c == 0x7FU)           /* Backspace */
        {
            if (idx > 0U)
            {
                idx--;
                buf[idx] = '\0';
                demo_uart_write_blocking("\b \b");
            }
            continue;
        }

        if (idx < (buf_size - 1U))
        {
            buf[idx++] = (char)c;
            /* Echo the character */
            char echo[2] = { (char)c, '\0' };
            demo_uart_write_blocking(echo);
        }
    }
}

/***********************************************************************************************************************
 * Static task storage
 **********************************************************************************************************************/

static StaticTask_t g_demo_cli_task_tcb;
static StackType_t  g_demo_cli_task_stack[DEMO_CLI_TASK_STACK_SIZE / sizeof(StackType_t)];

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static StaticTask_t g_demo_consumer_task_tcb;
static StackType_t  g_demo_consumer_task_stack[DEMO_CONSUMER_TASK_STACK_SIZE / sizeof(StackType_t)];
static TaskHandle_t g_demo_consumer_task_handle = NULL;

/* Expert mode uses g_logger_cfg declared above; p_callback overridden at Open() time. */
#endif

/***********************************************************************************************************************
 * Forward declarations
 **********************************************************************************************************************/

static void demo_cli_task(void * p_arg);

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static void demo_consumer_task(void * p_arg);
static void demo_logger_callback(rs_rtc_logger_callback_args_t * p_args);
#endif

static bool demo_parse_datetime(char const * p_str, rtc_time_t * p_time);
static bool demo_is_power_on_reset(void);

/***********************************************************************************************************************
 * Application entry point — called from blinky_thread_entry after ICU is armed
 **********************************************************************************************************************/

void rs_rtc_logger_demo_main(void)
{
    (void)xTaskCreateStatic(demo_cli_task,
                            "demo_cli",
                            DEMO_CLI_TASK_STACK_SIZE / sizeof(StackType_t),
                            NULL,
                            DEMO_CLI_TASK_PRIORITY,
                            g_demo_cli_task_stack,
                            &g_demo_cli_task_tcb);

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    g_demo_consumer_task_handle =
        xTaskCreateStatic(demo_consumer_task,
                          "demo_consumer",
                          DEMO_CONSUMER_TASK_STACK_SIZE / sizeof(StackType_t),
                          NULL,
                          DEMO_CONSUMER_TASK_PRIORITY,
                          g_demo_consumer_task_stack,
                          &g_demo_consumer_task_tcb);
#endif

    /* NOTE: vTaskStartScheduler() is called by main.c — do NOT call it here. */
}

/***********************************************************************************************************************
 * CLI task
 **********************************************************************************************************************/

static void demo_cli_task(void * p_arg)
{
    (void)p_arg;

    char       input_buf[32];
    rtc_time_t rtc_time = {0};

    /* On cold start (power-on reset) the USB-UART bridge must enumerate and
     * the PC terminal must reconnect to the COM port before any UART TX is
     * visible.  This typically takes 2–3 seconds from power-on.  Without a
     * long enough delay the early status messages are buffered inside the
     * bridge and only released when the terminal sends its first character
     * (e.g. an Enter key press), making the output appear to be "stuck."
     *
     * On warm reset the USB link is never broken and the terminal stays
     * connected, so a short settling delay is sufficient.               */
    if (demo_is_power_on_reset())
    {
        vTaskDelay(pdMS_TO_TICKS(3000U));   /* allow USB-UART bridge to enumerate */
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(500U));    /* warm reset — link already up */
    }

    /* ── Banner ─────────────────────────────────────────────────────────── */
    demo_uart_write_blocking("\r\n*** RTC Event Logger Demo ***\r\n");

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    demo_uart_write_blocking("Mode: Example Project\r\n");
#else
    demo_uart_write_blocking("Mode: Expert\r\n");
#endif

    /* ── Mode-specific init ──────────────────────────────────────────────── */

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE

    /* Bind HAL instances into the RTC Event Logger configuration */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
#else
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0, &g_uart0);
#endif
    g_logger_ctrl.p_cfg = &g_logger_cfg;

    /* Create the internal consumer task from task context — BEFORE the ICU
     * is enabled and before any EventTrigger() call from an ISR.
     * xTaskCreateStatic() is illegal in ISR context; this call moves task
     * creation to a safe point in the CLI task. */
    demo_uart_write_blocking("Creating RTC Event Logger consumer task...\r\n");
    fsp_err_t prep_err = rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);
    if (FSP_SUCCESS != prep_err)
    {
        demo_uart_write_blocking("ERROR: Failed to create RTC Event Logger consumer task.\r\n");
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }
    demo_uart_write_blocking("Consumer task ready.\r\n");

    /* ── Status messages MUST be printed and TX-confirmed BEFORE calling
     * EventTrigger().  Reason: EventTrigger() wakes the consumer task, which
     * runs rs_rtc_logger_hal_uart_open() and calls close()+reopen() to take
     * ownership of the UART.  If a demo_uart_write_blocking() TX is in flight
     * at that moment, close() aborts it mid-string and the consumer's INIT
     * event is appended directly to the truncated fragment — producing output
     * like "Waiting for[0000-00-00 00:00:00]".
     *
     * demo_uart_write_blocking() blocks until TX_COMPLETE before returning,
     * so printing both lines here guarantees the UART is idle when
     * EventTrigger() hands control to the consumer task.                    */
    demo_uart_write_blocking("Triggering RTC Event Logger auto-init...\r\n");
    demo_uart_write_blocking("Waiting for RTC Event Logger auto-init to complete...\r\n");

    (void)rs_rtc_logger_EventTrigger(&g_logger_ctrl,
                                     (uint8_t const *)DEMO_INIT_PAYLOAD,
                                     DEMO_INIT_PAYLOAD_LEN);

    /* Wait for auto_init_done — set by the internal consumer task once
     * Open() and Start() have completed and the INIT event has been written. */
    uint32_t wait_count = 0U;
    while (!g_logger_ctrl.auto_init_done)
    {
        vTaskDelay(pdMS_TO_TICKS(10U));
        wait_count++;
        if (wait_count > 1000U)        /* 10 s timeout — should never fire */
        {
            /* By this point the consumer task has attempted (and failed) to run
             * auto_init.  The UART is in logger-callback mode if Open() succeeded
             * before stalling, or still in early-callback mode if Open() itself
             * failed.  Either way tx_complete is being driven by whichever
             * callback is active, so demo_uart_write_blocking() will work.  */
            demo_uart_write_blocking(
                "\r\nERROR: RTC Event Logger auto-init timed out.\r\n"
                "Check RTC oscillator source and UART configuration.\r\n");
            for (;;) { vTaskDelay(portMAX_DELAY); }
        }
    }
    demo_uart_write_blocking("RTC Event Logger auto-init complete.\r\n");

    /* Validate clock state: Open() always starts with rtc_time_set = false.
     * Restore it to true only if the user confirmed a time in a previous
     * session (magic survives warm reset via noinit; never valid on POR).  */
    (void)rs_rtc_logger_ClockValidate(&g_logger_ctrl,
              !demo_is_power_on_reset() &&
              (DEMO_TIME_SET_MAGIC == g_time_set_magic));

    /* Warm reset: restore sequence counter from noinit backup (FR-064).
     * On POR the magic sentinel is uninitialised so this branch is correctly skipped. */
    if (!demo_is_power_on_reset() && (DEMO_SEQ_BACKUP_MAGIC == g_seq_count_magic))
    {
        (void)rs_rtc_logger_SeqCountSet(&g_logger_ctrl, g_seq_count_backup);
        demo_uart_write_blocking("Warm reset: seq counter restored.\r\n");
    }

#else /* Expert mode */

    /* Expert mode: explicit lifecycle — Open() here, Start() after time is configured.
     *
     * The cold start date/time prompt, LOCO trim calibration, warm reset message,
     * and time-set guard monitoring loop are all in the SHARED section that follows
     * the #endif below — they apply identically to both EP and Expert modes.
     *
     * Log message ordering: Open() is called synchronously below, always after
     * demo_uart_write_blocking() has confirmed TX_COMPLETE.  The UART close()+reopen()
     * inside rs_rtc_logger_hal_uart_open() therefore never interrupts an in-flight
     * write — no special pre-Open() ordering is needed here (contrast with the
     * EP mode path where EventTrigger() wakes the consumer task asynchronously). */
    demo_uart_write_blocking("Opening RTC Event Logger...\r\n");
    /* Bind HAL instances into the PES configuration (ConfigGen pattern, Section 10.1). */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset, &g_uart0_preset);
#else
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0, &g_uart0);
#endif
    g_logger_cfg.p_callback = demo_logger_callback; /* FR-084: wake demo_consumer_task on TX_COMPLETE/TX_ERROR */
    fsp_err_t err = rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
    if (FSP_SUCCESS != err)
    {
        demo_uart_write_blocking("ERROR: rs_rtc_logger_Open failed.\r\n");
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }
    demo_uart_write_blocking("RTC Event Logger open OK.\r\n");

    /* Validate clock state: Open() always starts with rtc_time_set = false.
     * Restore it to true only if the user confirmed a time in a previous
     * session (magic survives warm reset via noinit; never valid on POR).  */
    (void)rs_rtc_logger_ClockValidate(&g_logger_ctrl,
              !demo_is_power_on_reset() &&
              (DEMO_TIME_SET_MAGIC == g_time_set_magic));

    /* Warm reset: restore sequence counter from noinit backup (FR-064).
     * On POR the magic sentinel is uninitialised so this branch is correctly skipped. */
    if (!demo_is_power_on_reset() && (DEMO_SEQ_BACKUP_MAGIC == g_seq_count_magic))
    {
        (void)rs_rtc_logger_SeqCountSet(&g_logger_ctrl, g_seq_count_backup);
        demo_uart_write_blocking("Warm reset: seq counter restored.\r\n");
    }

#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

    /* ── RTC time setup ──────────────────────────────────────────────────── */

    if (demo_is_power_on_reset())
    {
        demo_uart_write_blocking("Power-on reset detected.\r\n");

        /* First prompt — allow the user to skip (press Enter) and set the
         * time later via the per-event guard below.  An invalid format still
         * re-prompts immediately; only a blank line exits without setting.   */
        bool time_set = false;
        while (!time_set)
        {
            demo_uart_write_blocking("Enter date/time [YYYY-MM-DD HH:MM:SS] : ");
            demo_uart_readline(input_buf, sizeof(input_buf));

            if (input_buf[0] == '\0')
            {
                demo_uart_write_blocking(
                    "Time entry skipped.\r\n"
                    "Events will show [0000-00-00 00:00:00] until time is set.\r\n"
                    "You will be prompted to set the time after each event.\r\n");
                break;
            }

            if (!demo_parse_datetime(input_buf, &rtc_time))
            {
                demo_uart_write_blocking(
                    "ERROR: Invalid format. Expected YYYY-MM-DD HH:MM:SS"
                    " (e.g. 2024-07-15 14:30:00)\r\n");
                continue;   /* re-prompt */
            }

            fsp_err_t set_err = rs_rtc_logger_TimeSet(&g_logger_ctrl, &rtc_time);
            if (FSP_SUCCESS == set_err)
            {
                demo_uart_write_blocking("Time set OK.\r\n");
                g_time_set_magic = DEMO_TIME_SET_MAGIC; /* persist across warm resets */
                time_set = true;
            }
            else
            {
                /* HAL rejection (e.g. RTC not in OPEN state) — treat as a
                 * non-recoverable configuration fault rather than a user typo. */
                demo_uart_write_blocking("WARNING: TimeSet failed — check RTC state.\r\n");
                time_set = true;
            }
        }

        /* ── LOCO drift calibration ───────────────────────────────────────
         * The LOCO oscillator can drift significantly (±several %).
         * The RFRL register controls how many LOCO cycles the RTC counts
         * per sub-tick.  Nominal value = 255 (32.768 kHz LOCO).
         *
         * Tuning guide (~3906 ppm / RFRL unit, ~3.5 s / 15 min / unit):
         *   Fast 1 min/15 min → trim +17  (RFRL 272)
         *   Slow 1 min/15 min → trim -17  (RFRL 238)
         *   Fast 1 sec/hour   → trim  +1  (RFRL 256)
         *   Slow 1 sec/hour   → trim  -1  (RFRL 254)
         *
         * Press Enter with no input to skip and use the default RFRL.     */
        demo_uart_write_blocking(
            "LOCO trim (fast=+, slow=-, Enter=skip) [e.g. +17 or -17]: ");
        demo_uart_readline(input_buf, sizeof(input_buf));

        if (input_buf[0] != '\0')
        {
            int trim = 0;
            if (1 == sscanf(input_buf, "%d", &trim))
            {
                int rfrl_new = (int)RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT + trim;

                if ((rfrl_new < 8) || (rfrl_new > 510))
                {
                    char msg[96];
                    (void)snprintf(msg, sizeof(msg),
                                   "WARNING: trim %+d gives RFRL %d"
                                   " -- out of range (8-510). Skipped.\r\n",
                                   trim, rfrl_new);
                    demo_uart_write_blocking(msg);
                }
                else
                {
                    fsp_err_t cal_err = rs_rtc_logger_LocoCalibrationSet(
                                            &g_logger_ctrl, (uint16_t)rfrl_new);
                    char msg[96];
                    if (FSP_SUCCESS == cal_err)
                    {
                        (void)snprintf(msg, sizeof(msg),
                                       "LOCO calibration set: trim %+d, RFRL = %d.\r\n",
                                       trim, rfrl_new);
                    }
                    else
                    {
                        (void)snprintf(msg, sizeof(msg),
                                       "WARNING: LocoCalibrationSet failed (err=0x%08X).\r\n",
                                       (unsigned)cal_err);
                    }
                    demo_uart_write_blocking(msg);
                }
            }
            else
            {
                demo_uart_write_blocking(
                    "WARNING: Could not parse trim value. Skipped.\r\n");
            }
        }
        else
        {
            demo_uart_write_blocking("LOCO calibration skipped (default RFRL = 255).\r\n");
        }
    }
    else
    {
        /* Warm reset — distinguish between a retained user-set time and an
         * RTC that was never set (counter sat at 2000-01-01 since cold start). */
        if (DEMO_TIME_SET_MAGIC == g_time_set_magic)
        {
            demo_uart_write_blocking("Warm reset — RTC time retained.\r\n");
        }
        else
        {
            demo_uart_write_blocking(
                "Warm reset — clock was not set in previous session.\r\n"
                "Events will show [0000-00-00 00:00:00]. Press S1 to be prompted.\r\n");
        }
    }

    /* ── Expert mode: start RTC Event Logger after time is configured ────────────────── */

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    demo_uart_write_blocking("Starting RTC Event Logger...\r\n");
    (void)rs_rtc_logger_Start(&g_logger_ctrl);
    demo_uart_write_blocking("RTC Event Logger started.\r\n");

    if (NULL != g_demo_consumer_task_handle)
    {
        xTaskNotifyGive(g_demo_consumer_task_handle);
    }
#endif

    /* ── Ready ───────────────────────────────────────────────────────────── */
    demo_uart_write_blocking("Ready. Press S1 to log an event.\r\n");

    /* ── Time-set guard — monitor for events posted before clock is set ──
     *
     * One zeroed-time event is always allowed through and displayed as
     * [0000-00-00 00:00:00] by the consumer task without interruption.
     * After each new event logged while the clock is still unset, this
     * loop wakes, prints a warning on the terminal, and re-prompts the
     * user to enter the date/time.
     *
     * - Press Enter at the prompt to skip and wait for the next event.
     * - Enter a valid date/time to set the clock; all subsequent events
     *   will carry a real timestamp.
     * - The loop (and this task) exits once rtc_time_set is confirmed.
     *
     * If time was already set during the initial prompt above, or this is
     * a warm reset, rtc_time_set is already true and the loop body never
     * executes — the task deletes itself immediately.                      */
    {
        uint32_t last_seen = g_logger_ctrl.events_logged; /* already displayed */

        while (!g_logger_ctrl.rtc_time_set)
        {
            vTaskDelay(pdMS_TO_TICKS(50U));

            uint32_t current = g_logger_ctrl.events_logged;
            if (current == last_seen)
            {
                continue;   /* no new events yet */
            }

            /* One or more new events have been logged with [0000-00-00 00:00:00].
             * Absorb all of them into last_seen in one go — a single prompt
             * covers however many S1 presses occurred during this window.   */
            last_seen = current;

            demo_uart_write_blocking(
                "\r\nWARNING: event logged without a valid clock.\r\n"
                "Enter date/time [YYYY-MM-DD HH:MM:SS] : ");
            demo_uart_readline(input_buf, sizeof(input_buf));

            if (input_buf[0] == '\0')
            {
                demo_uart_write_blocking(
                    "Skipped — press S1 to log another event and be prompted again.\r\n");
                continue;
            }

            if (!demo_parse_datetime(input_buf, &rtc_time))
            {
                demo_uart_write_blocking(
                    "ERROR: Invalid format. Expected YYYY-MM-DD HH:MM:SS"
                    " (e.g. 2024-07-15 14:30:00)\r\n"
                    "Press S1 to be prompted again.\r\n");
                continue;
            }

            fsp_err_t guard_err = rs_rtc_logger_TimeSet(&g_logger_ctrl, &rtc_time);
            if (FSP_SUCCESS == guard_err)
            {
                demo_uart_write_blocking(
                    "Time set OK. All future events will have a valid timestamp.\r\n");
                g_time_set_magic = DEMO_TIME_SET_MAGIC; /* persist across warm resets */
                /* g_logger_ctrl.rtc_time_set is now true — loop exits */
            }
            else
            {
                demo_uart_write_blocking(
                    "WARNING: TimeSet failed — check RTC state.\r\n"
                    "Press S1 to be prompted again.\r\n");
            }
        }
    }

    /* CLI task work is done — delete it to free its stack. */
    vTaskDelete(NULL);
}

/***********************************************************************************************************************
 * Expert mode consumer task
 **********************************************************************************************************************/

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE

/**
 * @brief RTC Event Logger application callback (FR-084) — wakes demo_consumer_task on TX completion.
 *
 * Registered via g_logger_cfg.p_callback (Open() call site above). Called
 * from rs_rtc_logger_uart_callback() in ISR context on both TX_COMPLETE and
 * TX_ERROR — mirroring the unconditional `tx_complete = true` set on both
 * outcomes in the RTC Event Logger HAL — so a TX error can't stall the drain loop either.
 * This replaces polling g_logger_ctrl.tx_complete with the same notification
 * pattern the RTC Event Logger already uses internally for its own task in Example mode.
 */
static void demo_logger_callback(rs_rtc_logger_callback_args_t * p_args)
{
    if ((RS_RTC_LOGGER_EVENT_TX_COMPLETE == p_args->event) ||
        (RS_RTC_LOGGER_EVENT_TX_ERROR == p_args->event))
    {
        if (NULL != g_demo_consumer_task_handle)
        {
            BaseType_t higher_priority_task_woken = pdFALSE;
            vTaskNotifyGiveFromISR(g_demo_consumer_task_handle, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }
}

static void demo_consumer_task(void * p_arg)
{
    (void)p_arg;
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    rs_rtc_logger_event_entry_t entry;

    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS));

        while (RS_RTC_LOGGER_STATE_STARTED == g_logger_ctrl.state)
        {
            /* DC-6 fix: use the canonical public dequeue function instead of
             * accessing buf_tail / buf_count directly.  rs_rtc_logger_EventDequeue()
             * wraps rs_rl_dequeue(), which copies the entry and updates the index
             * inside a single critical section — no TOCTOU exposure. */
            if (FSP_SUCCESS != rs_rtc_logger_EventDequeue(&g_logger_ctrl, &entry))
            {
                break; /* buffer empty or not open */
            }

            (void)rs_rtc_logger_Write(&g_logger_ctrl, &entry);

            /* Wait for demo_logger_callback() to notify us on TX_COMPLETE/
             * TX_ERROR (ISR context) instead of polling tx_complete.
             *
             * BUG FIX (TestIssue_02): same race as the Example Project mode
             * consumer task — an EventTrigger() ISR notification can wake
             * this ulTaskNotifyTake() before the UART TX is complete, causing
             * line_buf to be overwritten mid-transmission and producing
             * corrupted output with dropped events.
             *
             * FIX: loop until g_logger_ctrl.tx_complete is actually set by
             * the UART_EVENT_TX_COMPLETE handler in the ISR callback.
             * tx_complete is cleared before every write and set only on
             * genuine TX completion — so the condition is unambiguous. */
            do
            {
                (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS));
            } while (!g_logger_ctrl.tx_complete);
        }
    }
}

#endif /* !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

/***********************************************************************************************************************
 * Button S1 ISR callback
 **********************************************************************************************************************/

void demo_button_s1_callback(external_irq_callback_args_t * p_args)
{
    (void)p_args;

#if RS_RTC_LOGGER_CFG_DEBOUNCE_MS > 0
    /* Software debounce (see rs_rtc_logger_cfg.h).
     *
     * the r_icu hardware digital filter
     * is far shorter than the mechanical button bounce period (5-20 ms).
     * Every bounce edge triggers this ISR, producing 7-13 spurious calls
     * per physical press — observable as many events with identical second
     * timestamps.  The effect is worst for the FIRST press after a warm
     * reset or after a long idle period, when the button contact has not
     * been recently exercised.
     *
     * compare the current FreeRTOS tick count against the tick count
     * recorded for the last accepted press.  Reject the event if less than
     * RS_RTC_LOGGER_CFG_DEBOUNCE_MS ticks have elapsed.  TickType_t is an
     * unsigned type so the subtraction handles 32-bit tick counter wrap
     * correctly without special-casing.
     *
     * xTaskGetTickCountFromISR() is safe at this ISR priority (IPL=5 <=
     * configMAX_SYSCALL_INTERRUPT_PRIORITY). */
    static volatile TickType_t s_last_accepted_tick = 0U;
    TickType_t const now = xTaskGetTickCountFromISR();
    if ((now - s_last_accepted_tick) < pdMS_TO_TICKS(RS_RTC_LOGGER_CFG_DEBOUNCE_MS))
    {
        return;  /* Within debounce window — discard bounce edge */
    }
    s_last_accepted_tick = now;
#endif /* RS_RTC_LOGGER_CFG_DEBOUNCE_MS > 0 */

    (void)rs_rtc_logger_EventTrigger(&g_logger_ctrl,
                                     (uint8_t const *)DEMO_EVENT_PAYLOAD,
                                     DEMO_EVENT_PAYLOAD_LEN);

    /* Persist the current sequence counter to the warm-reset-safe noinit
     * backup so it can be restored after a warm reset (FR-064).
     * Written in ISR context — uint32_t write is atomic on Cortex-M33. */
    g_seq_count_backup = g_logger_ctrl.seq_count;
    g_seq_count_magic  = DEMO_SEQ_BACKUP_MAGIC;

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    if (NULL != g_demo_consumer_task_handle)
    {
        BaseType_t higher_prio_woken = pdFALSE;
        vTaskNotifyGiveFromISR(g_demo_consumer_task_handle, &higher_prio_woken);
        portYIELD_FROM_ISR(higher_prio_woken);
    }
#endif
}

/***********************************************************************************************************************
 * Utility: parse "YYYY-MM-DD HH:MM:SS"
 **********************************************************************************************************************/

static bool demo_parse_datetime(char const * p_str, rtc_time_t * p_time)
{
    if ((NULL == p_str) || (NULL == p_time)) { return false; }

    int year, mon, mday, hour, min, sec;
    int parsed = sscanf(p_str, "%d-%d-%d %d:%d:%d",
                        &year, &mon, &mday, &hour, &min, &sec);
    if (6 != parsed) { return false; }

    /* Range checks — RA4E2 RTC supports years 2000–2099 */
    if ((year < 2000) || (year > 2099)) { return false; }
    if ((mon  < 1)    || (mon  > 12))   { return false; }
    if ((mday < 1)    || (mday > 31))   { return false; }
    if ((hour < 0)    || (hour > 23))   { return false; }
    if ((min  < 0)    || (min  > 59))   { return false; }
    if ((sec  < 0)    || (sec  > 59))   { return false; }

    p_time->tm_year  = year - 1900;
    p_time->tm_mon   = mon  - 1;
    p_time->tm_mday  = mday;
    p_time->tm_hour  = hour;
    p_time->tm_min   = min;
    p_time->tm_sec   = sec;
    p_time->tm_wday  = 0;
    p_time->tm_yday  = 0;
    p_time->tm_isdst = 0;

    return true;
}

/***********************************************************************************************************************
 * Utility: detect power-on reset
 **********************************************************************************************************************/

static bool demo_is_power_on_reset(void)
{
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
    /* FIX: cache the result on first call.  RSTSR0 is a sticky-flag register;
     * if any driver or BSP code clears PORF between calls the result would
     * silently change mid-task, producing inconsistent warm/cold-start behaviour.
     * Reading once and caching makes all calls within this session consistent. */
    static bool s_cached = false;
    static bool s_result = false;
    if (!s_cached)
    {
        s_result = (1U == R_SYSTEM->RSTSR0_b.PORF);
        s_cached = true;
    }
    return s_result;
#else
    return true;
#endif
}
