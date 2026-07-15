/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * blinky_thread_entry.c — FreeRTOS thread entry for rs_rtc_logger (FPB-RA6E2)
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

#include <rs_rtc_logger/rs_rtc_logger_cfg.h> /* RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE, RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE */
#include "blinky_thread.h"
#include "common_data.h"    /* g_external_irq9 */
#include "hal_data.h"       /* g_uart0 (FSP-generated) */
#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra6e2.h> /* g_uart0_preset (when PRESET_CONFIG_USE=1) */
#include "r_icu.h"          /* R_ICU_ExternalIrqOpen/Enable */
#include "r_sci_uart.h"     /* R_SCI_UART_Open, R_SCI_UART_Write */
#include <string.h>         /* strlen() */
/* board_leds.h included transitively: blinky_thread.h -> bsp_api.h -> board_cfg.h -> board.h */

/* Preset-mode aware UART instance for the pre-demo banner below.
 *
 * RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 1 (default): rs_rtc_logger_Open()
 * will bind g_uart0_preset — a standalone SCI0 config with fixed 115200-baud
 * register values (rs_rtc_logger_data_fpb_ra6e2.c), independent of whatever
 * the Stack Configurator currently has set for g_uart0. The banner below
 * MUST open/write through that same instance, or the "ALREADY_OPEN ->
 * close+reopen" handoff inside rs_rtc_logger_hal_uart_open() targets a
 * *different* control block than the one the banner opened — banner/CLI
 * output would then keep using whatever (possibly broken) baud rate is in
 * the Stack Configurator while the logger's own event writes use the
 * preset, a silent mismatch between demo output and logged events.
 *
 * RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE = 0: use the FSP-generated g_uart0
 * directly, matching what ConfigGen()/Open() bind in that mode.
 *
 * demo_uart_write_blocking() in rs_rtc_logger_demo.c defines the identical
 * macro for the same reason — keep both in sync if this changes. */
#if RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE
 #define DEMO_UART_INSTANCE  (&g_uart0_preset)
#else
 #define DEMO_UART_INSTANCE  (&g_uart0)
#endif

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

/* TX-complete flag set by rs_rtc_logger_uart_callback on UART_EVENT_TX_COMPLETE.
 * Declared extern so the banner write below can poll it without a FreeRTOS delay
 * (the scheduler may not be fully up when this flag is first checked). */
extern volatile bool g_blinky_uart_tx_done;
volatile bool g_blinky_uart_tx_done = false;

/* Set by rs_rtc_logger_demo.c: drives g_logger_ctrl.tx_complete so that
 * demo_uart_write_blocking() (which polls that flag) works before the
 * RTC Event Logger has opened the UART with its own callback.           */
extern void demo_uart_early_tx_complete(void);

/* Minimal UART callback used for the pre-demo banner write AND for all
 * demo_uart_write_blocking() calls in the CLI task that run before the
 * RTC Event Logger re-opens the UART with its own callback.
 * Sets both the local banner flag and the logger ctrl tx_complete flag. */
static void demo_early_uart_callback(uart_callback_args_t * p_args)
{
    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        g_blinky_uart_tx_done = true;
        demo_uart_early_tx_complete(); /* keep g_logger_ctrl.tx_complete in sync */
    }
}

/* Send a NUL-terminated string over VCOM and spin-wait for TX complete.\n * Called before the RTC Event Logger is open, so no FreeRTOS delays are used. */
static void demo_banner_write(char const * p_str)
{
    /* DC-5 fix: use strlen() — <string.h> is already included. */
    uint32_t len = (uint32_t)strlen(p_str);
    if (0U == len) { return; }

    g_blinky_uart_tx_done = false;
    R_SCI_UART_Write(DEMO_UART_INSTANCE->p_ctrl, (uint8_t const *)p_str, len);

    /* Spin until TX ISR sets the flag — typically <1 ms at 115200 baud */
    uint32_t timeout = 200000U;
    while (!g_blinky_uart_tx_done && timeout-- > 0U) { __NOP(); }
}

/* Setup Demo Banner and User Button S1 Interrupt **/
static void demo_banner_setup(void)
{
    /* ── Step 1: Open UART with a temporary callback for the banner ─────── *
     * We open with a modified cfg that points to our local callback.
     * The RTC Event Logger will re-open the UART later (Close + Open) with its own
     * callback, so there is no conflict. Open/write through DEMO_UART_INSTANCE
     * (see its definition above) so this matches whichever instance
     * rs_rtc_logger_Open() will bind — preset or FSP-generated.           */

    uart_cfg_t early_cfg = *(DEMO_UART_INSTANCE->p_cfg);
    early_cfg.p_callback = demo_early_uart_callback;
    fsp_err_t err = R_SCI_UART_Open(DEMO_UART_INSTANCE->p_ctrl, &early_cfg);
    if (FSP_SUCCESS != err)
    {
        /* UART failed — halt visibly; LEDs will not blink */
        BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
    }

    /* ── Step 2: Send sign-on banner ────────────────────────────────────── */
    demo_banner_write("\r\n");
    demo_banner_write("=========================================\r\n");
    demo_banner_write("  RTC Event Logger — FPB-RA6E2\r\n");
    demo_banner_write("  FSP v6.5.0 / FreeRTOS\r\n");
    demo_banner_write("=========================================\r\n");
    demo_banner_write("UART OK. Starting ...\r\n");

    /* UART is intentionally left open here.  The CLI task uses
     * demo_uart_write_blocking() before the RTC Event Logger opens the UART.
     * rs_rtc_logger_hal_uart_open() detects FSP_ERR_ALREADY_OPEN, closes, and
     * reopens with the logger's own callback — no gap in console output.     */

    /* ── Step 3: Open and enable user button S1 (P304 / IRQ9) ───────────── */
    err = R_ICU_ExternalIrqOpen(g_external_irq9.p_ctrl, g_external_irq9.p_cfg);
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
