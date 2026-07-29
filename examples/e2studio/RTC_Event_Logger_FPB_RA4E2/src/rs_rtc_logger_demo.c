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
 *   The shared VCOM UART (g_uart_jlob_vcom) is opened once by the Buffered
 *   I/O UART driver and never closed for the life of the application.
 *   Ordering between console output and log-line output is guaranteed by
 *   that driver's own internal serialization inside fflush().
 */

#include <rs_rtc_logger/rs_rtc_logger.h>
#include <rs_rtc_logger/rs_rtc_logger_cfg.h> /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE, RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */
#include "common_data.h"       /* g_external_irq9 */
#include "hal_data.h"          /* g_rtc0 (FSP-generated) */

/* Configuration path (Section 10.1):
 *   PRESET_CONFIG_USE = 1 (default): ConfigGen uses _preset instances from
 *     the board config file — no FSP Stack Configurator re-run required.
 *   PRESET_CONFIG_USE = 0: ConfigGen uses FSP-generated g_rtc0.
 *
 * rs_rtc_logger_ConfigGen()/ConfigCheck()/ConfigGetSize() live in
 * board config header (moved from rs_rtc_logger.h/.c) and are declared
 * there UNCONDITIONALLY, so the header is included here 
 * — regardless of RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE — while the
 * *_preset instance externs inside that header remain conditionally
 * compiled on that same flag. */
#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>

/* All console I/O below goes through rs_rtc_logger_stdio_console.c, which in
 * turn goes through the Buffered I/O UART driver's stdio stream
 * bound to g_uart_jlob_vcom. rs_rtc_logger_ConfigGen() takes only an RTC
 * instance (see the call sites below); this component never opens, writes,
 * or validates a uart_instance_t. The Buffered I/O UART driver validates
 * its own configuration independently
 * (RS_STDIO_UART_CheckApplicationConfiguration(), wired in through
 * rs_rtc_logger_console_init()). */
#include "rs_rtc_logger/rs_rtc_logger_stdio_console.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

/***********************************************************************************************************************
 * Configuration
 **********************************************************************************************************************/

/* CLI task needs stack headroom for rs_rtc_logger_Open() +
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
/* derive length from the string literal so it can never silently diverge. */
#define DEMO_EVENT_PAYLOAD_LEN          (sizeof(DEMO_EVENT_PAYLOAD) - 1U)

#define DEMO_INIT_PAYLOAD               "INIT"
#define DEMO_INIT_PAYLOAD_LEN           (sizeof(DEMO_INIT_PAYLOAD) - 1U)

/***********************************************************************************************************************
 * RTC Event Logger control block — declared first so all functions in this file can access it
 **********************************************************************************************************************/

static rs_rtc_logger_ctrl_t g_logger_ctrl;

/***********************************************************************************************************************
 * Configuration — non-instance fields set here; p_rtc is
 * populated at runtime by rs_rtc_logger_ConfigGen() (Section 10.1).
 **********************************************************************************************************************/

static rs_rtc_logger_cfg_t g_logger_cfg =
{
    /* p_rtc set by rs_rtc_logger_ConfigGen() */
    .p_rtc           = NULL,
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
 * Restore: demo_cli_task() after RTC Event Logger init, warm reset only
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
 * Console I/O — backed by the Buffered I/O over UART driver (vendored,
 * unmodified, in src/rs_stdio_uart_buffer_io/) via the thin non-blocking
 * adapter in rs_rtc_logger_stdio_console.c.
 **********************************************************************************************************************/

/** Non-blocking dequeue of one console byte. Task context only. */
static bool demo_rx_getc(uint8_t * p_c)
{
    return rs_rtc_logger_console_getc_nb(p_c);
}

/***********************************************************************************************************************
 * TX helper — routes CLI/status output through the Buffered I/O UART driver's
 * stdio stream (non-blocking submit; see rs_rtc_logger_console_puts()).
 **********************************************************************************************************************/

static void demo_uart_write(char const * p_str)
{
    rs_rtc_logger_console_puts(p_str);
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
            demo_uart_write("\r\n");   /* Echo newline */
            break;
        }

        if (c == '\b' || c == 0x7FU)           /* Backspace */
        {
            if (idx > 0U)
            {
                idx--;
                buf[idx] = '\0';
                demo_uart_write("\b \b");
            }
            continue;
        }

        if (idx < (buf_size - 1U))
        {
            buf[idx++] = (char)c;
            /* Echo the character */
            char echo[2] = { (char)c, '\0' };
            demo_uart_write(echo);
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
    demo_uart_write("\r\n*** RTC Event Logger Demo ***\r\n");

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    demo_uart_write("Mode: Example Project\r\n");
#else
    demo_uart_write("Mode: Expert\r\n");
#endif

    /* ── Mode-specific init ──────────────────────────────────────────────── */

#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE

    /* Bind HAL instances into the RTC Event Logger configuration */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
#else
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0);
#endif

    /* Validate the generated configuration before Open() — catches a bad
     * RTC/UART instance pointer or an out-of-range carry_ipl/enum field
     * early, with a clear diagnostic, instead of failing deeper inside
     * rs_rtc_logger_Open(). */
    fsp_err_t cfg_check_err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
    if (FSP_SUCCESS != cfg_check_err)
    {
        char msg[96];
        (void)snprintf(msg, sizeof(msg),
                        "ERROR: RTC Event Logger ConfigCheck failed (err=0x%08X).\r\n",
                        (unsigned)cfg_check_err);
        demo_uart_write(msg);
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }

    /* Report the RTC Event Logger control-block footprint (diagnostic only). */
    {
        size_t ctrl_size = 0U;
        (void)rs_rtc_logger_ConfigGetSize(&g_logger_ctrl, &ctrl_size);
        char msg[64];
        (void)snprintf(msg, sizeof(msg),
                        "RTC Event Logger control block: %u bytes.\r\n",
                        (unsigned)ctrl_size);
        demo_uart_write(msg);
    }

    g_logger_ctrl.p_cfg = &g_logger_cfg;

    /* Create the internal consumer task from task context — BEFORE the ICU
     * is enabled and before any EventTrigger() call from an ISR.
     * xTaskCreateStatic() is illegal in ISR context; this call moves task
     * creation to a safe point in the CLI task. */
    demo_uart_write("Creating RTC Event Logger consumer task...\r\n");
    fsp_err_t prep_err = rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);
    if (FSP_SUCCESS != prep_err)
    {
        demo_uart_write("ERROR: Failed to create RTC Event Logger consumer task.\r\n");
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }
    demo_uart_write("Consumer task ready.\r\n");

    /* Sequencing between console output and log-line output is guaranteed by
     * the vendored driver's own internal serialization (uart_flush()'s
     * tx_busy spin-wait) inside fflush(), regardless of call order.        */
    demo_uart_write("Triggering RTC Event Logger auto-init...\r\n");
    demo_uart_write("Waiting for RTC Event Logger auto-init to complete...\r\n");

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
            demo_uart_write(
                "\r\nERROR: RTC Event Logger auto-init timed out.\r\n"
                "Check RTC oscillator source and UART configuration.\r\n");
            for (;;) { vTaskDelay(portMAX_DELAY); }
        }
    }
    demo_uart_write("RTC Event Logger auto-init complete.\r\n");

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
        demo_uart_write("Warm reset: seq counter restored.\r\n");
    }

#else /* Expert mode */

    /* Expert mode: explicit lifecycle — Open() here, Start() after time is configured.
     *
     * The cold start date/time prompt, LOCO trim calibration, warm reset message,
     * and time-set guard monitoring loop are all in the SHARED section that follows
     * the #endif below — they apply identically to both EP and Expert modes.
     *
     * The shared UART is owned permanently by the Buffered I/O UART driver.
     * Open() is called synchronously here, after demo_uart_write() has
     * returned. */
    demo_uart_write("Opening RTC Event Logger...\r\n");
    /* Bind HAL instances into the RTC Event Logger configuration (ConfigGen pattern, Section 10.1). */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
#else
    (void)rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0);
#endif

    /* Validate the generated configuration before Open() (Section 10.1 —
     * ConfigCheck/ConfigGetSize). Catches a bad RTC/UART instance pointer or
     * an out-of-range carry_ipl/enum field early, with a clear diagnostic. */
    fsp_err_t cfg_check_err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
    if (FSP_SUCCESS != cfg_check_err)
    {
        char msg[96];
        (void)snprintf(msg, sizeof(msg),
                        "ERROR: RTC Event Logger ConfigCheck failed (err=0x%08X).\r\n",
                        (unsigned)cfg_check_err);
        demo_uart_write(msg);
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }

    /* Report the RTC Event Logger control-block footprint (diagnostic only). */
    {
        size_t ctrl_size = 0U;
        (void)rs_rtc_logger_ConfigGetSize(&g_logger_ctrl, &ctrl_size);
        char msg[64];
        (void)snprintf(msg, sizeof(msg),
                        "RTC Event Logger control block: %u bytes.\r\n",
                        (unsigned)ctrl_size);
        demo_uart_write(msg);
    }

    g_logger_cfg.p_callback = demo_logger_callback; /* FR-084: wake demo_consumer_task on TX_COMPLETE/TX_ERROR */
    fsp_err_t err = rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
    if (FSP_SUCCESS != err)
    {
        demo_uart_write("ERROR: rs_rtc_logger_Open failed.\r\n");
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }
    demo_uart_write("RTC Event Logger open OK.\r\n");

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
        demo_uart_write("Warm reset: seq counter restored.\r\n");
    }

#endif /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

    /* ── RTC time setup ──────────────────────────────────────────────────── */

    if (demo_is_power_on_reset())
    {
        demo_uart_write("Power-on reset detected.\r\n");

        /* First prompt — allow the user to skip (press Enter) and set the
         * time later via the per-event guard below.  An invalid format still
         * re-prompts immediately; only a blank line exits without setting.   */
        bool time_set = false;
        while (!time_set)
        {
            demo_uart_write("Enter date/time [YYYY-MM-DD HH:MM:SS] : ");
            demo_uart_readline(input_buf, sizeof(input_buf));

            if (input_buf[0] == '\0')
            {
                demo_uart_write(
                    "Time entry skipped.\r\n"
                    "Events will show [0000-00-00 00:00:00] until time is set.\r\n"
                    "You will be prompted to set the time after each event.\r\n");
                break;
            }

            if (!demo_parse_datetime(input_buf, &rtc_time))
            {
                demo_uart_write(
                    "ERROR: Invalid format. Expected YYYY-MM-DD HH:MM:SS"
                    " (e.g. 2024-07-15 14:30:00)\r\n");
                continue;   /* re-prompt */
            }

            fsp_err_t set_err = rs_rtc_logger_TimeSet(&g_logger_ctrl, &rtc_time);
            if (FSP_SUCCESS == set_err)
            {
                demo_uart_write("Time set OK.\r\n");
                g_time_set_magic = DEMO_TIME_SET_MAGIC; /* persist across warm resets */
                time_set = true;
            }
            else
            {
                /* HAL rejection (e.g. RTC not in OPEN state) — treat as a
                 * non-recoverable configuration fault rather than a user typo. */
                demo_uart_write("WARNING: TimeSet failed — check RTC state.\r\n");
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
        demo_uart_write(
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
                    demo_uart_write(msg);
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
                    demo_uart_write(msg);
                }
            }
            else
            {
                demo_uart_write(
                    "WARNING: Could not parse trim value. Skipped.\r\n");
            }
        }
        else
        {
            demo_uart_write("LOCO calibration skipped (default RFRL = 255).\r\n");
        }
    }
    else
    {
        /* Warm reset — distinguish between a retained user-set time and an
         * RTC that was never set (counter sat at 2000-01-01 since cold start). */
        if (DEMO_TIME_SET_MAGIC == g_time_set_magic)
        {
            demo_uart_write("Warm reset — RTC time retained.\r\n");
        }
        else
        {
            demo_uart_write(
                "Warm reset — clock was not set in previous session.\r\n"
                "Events will show [0000-00-00 00:00:00]. Press S1 to be prompted.\r\n");
        }
    }

    /* ── Expert mode: start RTC Event Logger after time is configured ────────────────── */

#if !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    demo_uart_write("Starting RTC Event Logger...\r\n");
    (void)rs_rtc_logger_Start(&g_logger_ctrl);
    demo_uart_write("RTC Event Logger started.\r\n");

    if (NULL != g_demo_consumer_task_handle)
    {
        xTaskNotifyGive(g_demo_consumer_task_handle);
    }
#endif

    /* ── Ready ───────────────────────────────────────────────────────────── */
    demo_uart_write("Ready. Press S1 to log an event.\r\n");

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

            demo_uart_write(
                "\r\nWARNING: event logged without a valid clock.\r\n"
                "Enter date/time [YYYY-MM-DD HH:MM:SS] : ");
            demo_uart_readline(input_buf, sizeof(input_buf));

            if (input_buf[0] == '\0')
            {
                demo_uart_write(
                    "Skipped — press S1 to log another event and be prompted again.\r\n");
                continue;
            }

            if (!demo_parse_datetime(input_buf, &rtc_time))
            {
                demo_uart_write(
                    "ERROR: Invalid format. Expected YYYY-MM-DD HH:MM:SS"
                    " (e.g. 2024-07-15 14:30:00)\r\n"
                    "Press S1 to be prompted again.\r\n");
                continue;
            }

            fsp_err_t guard_err = rs_rtc_logger_TimeSet(&g_logger_ctrl, &rtc_time);
            if (FSP_SUCCESS == guard_err)
            {
                demo_uart_write(
                    "Time set OK. All future events will have a valid timestamp.\r\n");
                g_time_set_magic = DEMO_TIME_SET_MAGIC; /* persist across warm resets */
                /* g_logger_ctrl.rtc_time_set is now true — loop exits */
            }
            else
            {
                demo_uart_write(
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
 * @brief RTC Event Logger application callback (FR-084).
 *
 * Registered via g_logger_cfg.p_callback (Open() call site above), but never
 * invoked: rs_rtc_logger_hal_uart_write() (rs_rtc_logger_hal.c) has no
 * asynchronous TX-complete event to report, since a write only returns once
 * it has already been safely submitted/serialized (see that function's
 * comment). demo_consumer_task() does not depend on this callback firing
 * (see the comment at its rs_rtc_logger_Write() call site below).
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
            /* use the canonical public dequeue function instead of
             * accessing buf_tail / buf_count directly.  rs_rtc_logger_EventDequeue()
             * wraps rs_rl_dequeue(), which copies the entry and updates the index
             * inside a single critical section — no TOCTOU exposure. */
            if (FSP_SUCCESS != rs_rtc_logger_EventDequeue(&g_logger_ctrl, &entry))
            {
                break; /* buffer empty or not open */
            }

            (void)rs_rtc_logger_Write(&g_logger_ctrl, &entry);

            /* rs_rtc_logger_hal_uart_write() only returns once the line has
             * been submitted AND any prior transmission has physically
             * finished (see rs_rtc_logger_hal.c), so it is already safe to
             * proceed to the next entry here. demo_logger_callback() above
             * is never invoked; nothing waits on it. */
        }
    }
}

#endif /* !RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE */

/***********************************************************************************************************************
 * DIAGNOSTIC — RTC-read check (button S1)
 *
 * Measures the millisecond gap since the previous ACCEPTED (post-debounce)
 * event on every press, unconditionally — this happens regardless of the
 * DEMO_RTC_DT_DIAGNOSTIC_ENABLE gate below, so ISR timing is identical
 * whether the gate is on or off. The gate controls DISPLAY only: whether
 * that measured delta gets encoded into the event payload (and therefore
 * shows up on the serial terminal next to the RTC-formatted [HH:MM:SS])
 * or the event logs as the plain "Event Detected" text.
 *
 * DISPLAY GATED OFF BY DEFAULT - Because measurement is
 * now always-on regardless of this gate, flipping it does not change ISR
 * timing — only what gets printed.
 *
 * To re-enable display: build with -DDEMO_RTC_DT_DIAGNOSTIC_ENABLE=1, or
 * flip the default below.
 **********************************************************************************************************************/
#ifndef DEMO_RTC_DT_DIAGNOSTIC_ENABLE
 #define DEMO_RTC_DT_DIAGNOSTIC_ENABLE  (0)
#endif

/***********************************************************************************************************************
 * Button S1 ISR callback
 **********************************************************************************************************************/

void demo_button_s1_callback(external_irq_callback_args_t * p_args)
{
    (void)p_args;

    /* DIAGNOSTIC — see block comment above. Always measured,
     * regardless of DEMO_RTC_DT_DIAGNOSTIC_ENABLE. Persistent across the
     * whole session (not reset per burst), so it reflects the gap between
     * consecutive ACCEPTED events — the same events visible in the UART log
     * when display is enabled. */
    static volatile TickType_t s_debug_last_tick = 0U;
    TickType_t const debug_now         = xTaskGetTickCountFromISR();
    TickType_t const debug_delta_ticks = debug_now - s_debug_last_tick;
    (void)debug_delta_ticks;   /* used below only when display is enabled */

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

    /* Always update the diagnostic baseline for events that survive
     * debounce, regardless of the display gate, so the measurement stream
     * is continuous and unaffected by whether display is on. */
    s_debug_last_tick = debug_now;

#if DEMO_RTC_DT_DIAGNOSTIC_ENABLE
    char           debug_payload[24];
    uint32_t const debug_delta_ms = (uint32_t)debug_delta_ticks * (uint32_t)portTICK_PERIOD_MS;
    int            debug_len      = snprintf(debug_payload, sizeof(debug_payload),
                                              "Event dt=%lums", (unsigned long)debug_delta_ms);
    if (debug_len < 0)
    {
        debug_len = 0;
    }
    else if ((size_t)debug_len >= sizeof(debug_payload))
    {
        debug_len = (int)sizeof(debug_payload) - 1;
    }

    (void)rs_rtc_logger_EventTrigger(&g_logger_ctrl,
                                     (uint8_t const *)debug_payload,
                                     (uint32_t)debug_len);
#else
    (void)rs_rtc_logger_EventTrigger(&g_logger_ctrl,
                                     (uint8_t const *)DEMO_EVENT_PAYLOAD,
                                     DEMO_EVENT_PAYLOAD_LEN);
#endif /* DEMO_RTC_DT_DIAGNOSTIC_ENABLE */

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
    /* cache the result on first call.  RSTSR0 is a sticky-flag register;
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
