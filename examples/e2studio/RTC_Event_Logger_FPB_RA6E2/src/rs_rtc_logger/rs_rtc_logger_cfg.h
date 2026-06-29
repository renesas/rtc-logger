/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_cfg.h
 * @brief Build-time configuration constants for the RTC Event Logger.
 *
 * All constants use #ifndef guards so user project overrides take precedence.
 * Override any constant by defining it before including this header, or by
 * adding -D<CONSTANT>=<VALUE> to the compiler command line.
 */

#ifndef RS_RTC_LOGGER_CFG_H
#define RS_RTC_LOGGER_CFG_H

/***********************************************************************************************************************
 * Buffer sizing
 **********************************************************************************************************************/

/** Timestamp string buffer: enough for "[YYYY-MM-DD HH:MM:SS] " + NUL = 23 chars.
 *  DC-1 fix: the old RS_RTC_LOGGER_CFG_TS_BUF_SIZE macro was defined here but never
 *  referenced anywhere.  Replaced by a build-time assertion that LINE_BUF_SIZE is
 *  large enough to hold the longest timestamp prefix (23 chars) plus a minimal payload. */
#ifndef RS_RTC_LOGGER_CFG_TS_MIN_CHARS
  #define RS_RTC_LOGGER_CFG_TS_MIN_CHARS   (23U)
#endif

/**
 * Output line buffer: timestamp prefix + event payload + seq number + terminator + NUL.
 * Reduce for highly constrained targets if event payload is short.
 */
#ifndef RS_RTC_LOGGER_CFG_LINE_BUF_SIZE
  #define RS_RTC_LOGGER_CFG_LINE_BUF_SIZE         (256U)
#endif

/**
 * Circular event buffer depth — number of (timestamp, event_data) entries.
 * Default 8 handles short event bursts. Reduce to 4 for RAM-constrained targets.
 */
#ifndef RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH
  #define RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH       (8U)
#endif

/**
 * Maximum inline event payload per circular buffer entry (bytes, including NUL).
 * EventTrigger() truncates silently if caller supplies more than this.
 */
#ifndef RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN
  #define RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN    (32U)
#endif

/** Hard cap on control block RAM — build will fail if exceeded */
#ifndef RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES
  #define RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES  (1024U)
#endif

/***********************************************************************************************************************
 * Parameter checking
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * RTC carry interrupt priority guard
 **********************************************************************************************************************/

/**
 * @brief Maximum permitted RTC carry interrupt priority level (IPL).
 *
 * N-12: R_RTC_CalendarTimeGet() (called from rs_rtc_logger_EventTrigger() in
 * ISR context) spins waiting for the RTC carry ISR to set a flag.  If the
 * calling ISR has HIGHER priority (lower IPL number) than the carry ISR, the
 * carry ISR can never preempt the calling ISR — producing an infinite loop.
 *
 * FSP warning (r_rtc.c line ~411):
 *   "Do not call this function from a critical section or from an interrupt
 *    with higher priority than the carry interrupt."
 *
 * Required invariant:
 *   carry_ipl  <  ipl_of_any_ISR_that_calls_EventTrigger()
 *   (lower IPL number = higher preemption on Cortex-M)
 *
 * Default: 4 — the carry ISR must be configured at priority 1–4.
 * This is safely above the FreeRTOS max-syscall priority (1) and below the
 * demo button ISR (priority 5).
 *
 * The preset config (rs_rtc_logger_data_fpb_ra6e2.c) uses carry_ipl = 2. ✓
 * The FSP-generated hal_data.c uses carry_ipl = 12 which FAILS this check. ✗
 * Users adopting the FSP Stack Configurator path MUST set the RTC carry
 * interrupt priority to a value STRICTLY LESS THAN the priority of every ISR
 * that calls rs_rtc_logger_EventTrigger().
 *
 * Override at build time: -DRS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT=<value>
 */
#ifndef RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT
  #define RS_RTC_LOGGER_CFG_CARRY_IPL_LIMIT   (4U)
#endif

/***********************************************************************************************************************
 * LOCO oscillator drift compensation
 **********************************************************************************************************************/

/**
 * Default RFRL (Frequency Register Low) value written to the RTC on power-on reset.
 *
 * The RFRL register is the LOCO-mode calibration register.  The RTC counts
 * (RFRL + 1) LOCO cycles to generate each 128 Hz sub-tick; 128 sub-ticks make
 * one RTC second.  For a nominal 32.768 kHz LOCO:
 *
 *   RTC seconds per real second = 32768 / ((RFRL + 1) * 128)
 *
 * Nominal (RFRL = 255): 32768 / (256 * 128) = 1.000 000 Hz — perfect.
 *
 * Each +1 step makes the RTC slightly slower (fewer LOCO cycles are "missing"
 * per sub-tick), each -1 step makes it slightly faster:
 *
 *   ppm per step ≈ 1 / (RFRL + 1) × 10^6 ≈ 3906 ppm   (~3.5 s per 15 min)
 *
 * Practical tuning guide:
 *   Clock fast  1 min per 15 min → trim = +17 → RFRL = 272
 *   Clock slow  1 min per 15 min → trim = -17 → RFRL = 238
 *   Clock fast  1 sec per hour   → trim = +1  → RFRL = 256
 *   Clock slow  1 sec per hour   → trim = -1  → RFRL = 254
 *
 * Valid FSP range: 8 ≤ RFRL ≤ 510.
 *
 * Override at build time: -DRS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT=<value>
 * Override at runtime: rs_rtc_logger_LocoCalibrationSet().
 */
#ifndef RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT
  #define RS_RTC_LOGGER_CFG_LOCO_RFRL_DEFAULT  (255U)
#endif

/**
 * Enable/disable parameter checking (mirrors FSP convention).
 * When BSP_CFG_PARAM_CHECKING_ENABLE is not defined (e.g. host-based unit tests),
 * defaults to enabled (1) for maximum safety.
 */
#ifndef RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE
  #ifdef BSP_CFG_PARAM_CHECKING_ENABLE
    #define RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE  (BSP_CFG_PARAM_CHECKING_ENABLE)
  #else
    #define RS_RTC_LOGGER_CFG_PARAM_CHECKING_ENABLE  (1U)
  #endif
#endif


/***********************************************************************************************************************
 * Configuration path
 **********************************************************************************************************************/

/**
 * Configuration path selection.
 *
 * 1 = Preset configuration path (default):
 *     Include rs_rtc_logger_data_fpb_<board>.h and call
 *     rs_rtc_logger_ConfigGen() with _preset instances.
 *     No FSP Stack Configurator re-run required.
 *
 * 0 = FSP Stack Configurator path:
 *     Configure r_rtc and r_sci_uart in e2studio FSP tooling.
 *     Call rs_rtc_logger_ConfigGen() with FSP-generated instances.
 */
#ifndef RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
  #define RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE    (1U)
#endif

/***********************************************************************************************************************
 * Operating mode
 **********************************************************************************************************************/

/**
 * Operating mode selection
 *
 * 1 = Example Project mode (default):
 *     Application calls only rs_rtc_logger_EventTrigger().
 *     RTC Event Logger auto-manages Open()/Start() on the first call.
 *     RTC Event Logger owns an internal static FreeRTOS consumer task (FR-123).
 *     Write() is called internally — not required from application.
 *
 * 0 = Expert mode:
 *     Application owns the full lifecycle: Open() -> Start() -> EventTrigger()
 *     -> consumer task calls Write() -> Stop() -> Close().
 *     Supports advanced scenarios including firmware-update Stop/Start (FR-129).
 */
#ifndef RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
  #define RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE    (1U)
#endif

/***********************************************************************************************************************
 * Internal consumer task (Example Project mode only)
 **********************************************************************************************************************/

/**
 * Internal consumer task stack size in bytes (Example Project mode only).
 * Must be sufficient for timestamp formatting and VCOM write call depth.
 * No effect when RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 0.
 *
 * N-11 — Stack depth note: rs_rtc_logger_Open() previously stack-allocated
 * saved_event_buf[RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH] (~80 bytes × depth),
 * consuming 640 bytes at the default depth of 8.  This caused a hard fault
 * in Expert mode (CLI task stack = 1024 bytes) because fewer than 200 bytes
 * remained for R_RTC_Open() and R_SCI_UART_Open().
 *
 * FIXED: saved_event_buf is now declared static inside rs_rtc_logger_Open()
 * so it lives in BSS rather than on the stack.  The stack demand of Open()
 * is now ~64 bytes (saved scalars + frame overhead), leaving ample headroom.
 *
 * The static declaration is safe because Open() is never called re-entrantly:
 * Expert mode calls it once from the CLI task; EP mode calls it once from the
 * consumer task.  If EVENT_BUF_DEPTH is changed, the BSS footprint changes
 * (not stack), so this value does not need to track EVENT_BUF_DEPTH.
 */
#ifndef RS_RTC_LOGGER_CFG_TASK_STACK_SIZE
  #define RS_RTC_LOGGER_CFG_TASK_STACK_SIZE        (2048U) /* Note: increase from default for increased (high rate frequency) event workload */
#endif

/**
 * Internal consumer task FreeRTOS priority (Example Project mode only).
 * Set lower than time-critical tasks; must be high enough to drain the buffer
 * before it overflows under expected burst rates.
 * No effect when RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 0.
 */
#ifndef RS_RTC_LOGGER_CFG_TASK_PRIORITY
  #define RS_RTC_LOGGER_CFG_TASK_PRIORITY          (tskIDLE_PRIORITY + 1U)
#endif

/**
 * UART TX completion timeout in milliseconds for the internal consumer task
 * (Example Project mode only). Guards against hung TX without blocking indefinitely.
 * No effect when RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 0.
 */
#ifndef RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS
  #define RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS          (1000U)
#endif

/***********************************************************************************************************************
 * Software debounce for the button S1 ISR callback
 *
 * The r_icu hardware digital filter is set to PCLKB_div_1 (120 ns at 25 MHz)
 * which is far shorter than the 5-20 ms bounce period of a mechanical button.
 * Every bounce edge triggers the ISR, producing 7-13 spurious EventTrigger()
 * calls per physical button press.
 *
 * The debounce is implemented in demo_button_s1_callback() using the
 * FreeRTOS tick count (xTaskGetTickCountFromISR) to reject ISR firings that
 * arrive sooner than RS_RTC_LOGGER_CFG_DEBOUNCE_MS after the last accepted
 * event.  This covers typical button bounce while allowing rapid
 * intentional presses spaced apart.
 *
 * Set to 0 to disable software debounce (hardware filter only).
 **********************************************************************************************************************/
#ifndef RS_RTC_LOGGER_CFG_DEBOUNCE_MS
 #define RS_RTC_LOGGER_CFG_DEBOUNCE_MS    (150U)
#endif

#endif /* RS_RTC_LOGGER_CFG_H */
