/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * blinky_thread_entry.c — FreeRTOS thread entry for rs_rtc_logger
 *
 * Execution order:
 *   1. Open UART so the banner can be sent before anything else runs.
 *   2. Send sign-on banner over VCOM.
 *   3. Open and enable r_icu for user button S1 (P304 / IRQ9).
 *   4. Call rs_rtc_logger_demo_main() — creates CLI task, returns immediately.
 *   5. Blink LED1 and LED2 at 1 Hz forever to show the scheduler is alive.
 *
 * NOTE: vTaskStartScheduler() is already called by main.c before this runs.
 * NOTE: vApplicationIdleHook is NOT overridden here — the FSP default (WFI
 *       inside the idle task) is correct because the blinky loop's 500 ms
 *       vTaskDelay keeps a non-idle task pending every half-second, which
 *       prevents the scheduler from being suspended indefinitely.
 **********************************************************************************************************************/

#include "blinky_thread.h"
#include "common_data.h"    /* g_external_irq9 */
#include "hal_data.h"       /* g_uart_jlob_vcom (FSP-generated; opened by the Buffered I/O UART driver) */
#include "r_icu.h"          /* R_ICU_ExternalIrqOpen/Enable */
/* Console adapter — routes to UART (backend 0) or RTT (backend 1) as chosen by
 * RS_RTC_LOGGER_CFG_RTT_CONSOLE_ENABLE.  All banner output goes here so that the
 * RTT build has no picolibc stdio dependency from this file. */
#include "rs_rtc_logger/rs_rtc_logger_stdio_console.h"
/* board_leds.h included transitively: blinky_thread.h -> bsp_api.h -> board_cfg.h -> board.h */

extern void rs_rtc_logger_demo_main(void);

/* Override FSP weak vApplicationIdleHook with a plain WFI.
 *
 * N-14: The FSP default implementation (rm_freertos_port/port.c) calls
 * __disable_irq() + vTaskSuspendAll() + rm_freertos_port_sleep_preserving_lpm()
 * + __enable_irq(). This is a proper LPM-aware sleep pattern: vTaskSuspendAll()
 * prevents context switches DURING sleep entry/exit (not permanently), and
 * xTaskResumeAll() is called inside rm_freertos_port_sleep_preserving_lpm()
 * before returning. It does NOT permanently halt.
 *
 * The override below replaces that with a plain WFI, which is correct for
 * this demo because no LPM peripheral is configured. If LPM (Software Standby,
 * Snooze) is added, revert to the FSP default to preserve LPM state correctly. */
void vApplicationIdleHook(void)
{
    __WFI();
}
extern bsp_leds_t g_bsp_leds;

/* Setup Demo Banner and User Button S1 Interrupt **/
static void demo_banner_setup(void)
{
    /* ── Step 1: Initialise console transport and send sign-on banner ───────────
     * rs_rtc_logger_console_init() opens the selected transport (UART or RTT)
     * and is idempotent — calling it early here guarantees the transport is
     * ready before the RTC Event Logger or the button ISR runs.           */
    (void)rs_rtc_logger_console_init();

    rs_rtc_logger_console_puts("\r\n");
    rs_rtc_logger_console_puts("=========================================\r\n");
    rs_rtc_logger_console_puts("  RTC Event Logger - FPB-RA4E2\r\n");
    rs_rtc_logger_console_puts("  FSP v6.5.0 / FreeRTOS\r\n");
    rs_rtc_logger_console_puts("=========================================\r\n");
    rs_rtc_logger_console_puts("Console OK. Starting ...\r\n");

    /* ── Step 2: Open and enable user button S1 (P304 / IRQ9) ───────────── */
    fsp_err_t err = R_ICU_ExternalIrqOpen(g_external_irq9.p_ctrl, g_external_irq9.p_cfg);
    if (FSP_SUCCESS != err)
    {
        BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
    }
    R_ICU_ExternalIrqEnable(g_external_irq9.p_ctrl);
}

void blinky_thread_entry(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    /* Step 1 : Start Demo Banner and setup User Button S1 */
    demo_banner_setup();

    /* Step 2 : Start RTC Event Logger Demo : CLI task (returns immediately) ─────────────── */
    rs_rtc_logger_demo_main();

    /* ── Blink LED1 and LED2 at 1 Hz ────────────────────────────── */
    bsp_leds_t leds = g_bsp_leds;

    if (0U == leds.led_count)
    {
        for (;;) { vTaskDelay(portMAX_DELAY); }
    }

    bsp_io_level_t level = BSP_IO_LEVEL_HIGH;

    for (;;)
    {
        R_BSP_PinAccessEnable();
        for (uint32_t i = 0U; i < leds.led_count; i++)
        {
            R_BSP_PinWrite((bsp_io_port_pin_t)leds.p_leds[i], level);
        }
        R_BSP_PinAccessDisable();

        level = (BSP_IO_LEVEL_HIGH == level) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH;
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}
