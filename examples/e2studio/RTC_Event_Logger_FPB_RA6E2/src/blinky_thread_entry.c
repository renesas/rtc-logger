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
#include <stdio.h>          /* printf() — Buffered I/O over UART driver */
/* board_leds.h included transitively: blinky_thread.h -> bsp_api.h -> board_cfg.h -> board.h */

/* printf() below is the first stdio call in the application, so it
 * transparently triggers the vendored Buffered I/O UART driver's lazy
 * jlob_uart_init_once() (src/rs_stdio_uart_buffer_io/
 * syscalls_picolib_stdio_to_uart.c), which opens g_uart_jlob_vcom and
 * installs uart_jlob_vcom_callback as its ISR callback, once, for the life
 * of the application. The RTC Event Logger itself never opens or closes
 * this UART (see rs_rtc_logger_hal.c). */

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
    /* ── Step 1: Send sign-on banner ─────────────────────────────────────
     * printf() is the application's first stdio call, so it transparently
     * opens g_uart_jlob_vcom via the Buffered I/O UART driver's lazy init
     * (jlob_uart_init_once()) before anything else runs.                 */
    printf("\r\n");
    printf("=========================================\r\n");
    printf("  RTC Event Logger - FPB-RA6E2\r\n");
    printf("  FSP v6.5.0 / FreeRTOS\r\n");
    printf("=========================================\r\n");
    printf("UART OK. Starting ...\r\n");

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
