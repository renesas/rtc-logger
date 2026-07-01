/***********************************************************************************************************************
 * @file unity_port_sci_uart.c
 * @brief Unity output transport for the RTC Event Logger firmware test build.
 *
 * Implements unity_putc() by writing one byte at a time through the FSP SCI
 * UART HAL (g_uart0), which is connected to the USB-VCOM bridge on both the
 * FPB-RA4E2 and FPB-RA6E2 evaluation boards.
 *
 * Porting notes:
 *   - g_uart0 must be declared and initialised in ra_gen/common_data.c.
 *   - The UART callback (rs_rtc_logger_uart_callback or a dedicated one) must
 *     set g_unity_tx_done = true on RM_COMMS_EVENT_TX_OPERATION_COMPLETE.
 *   - This file is compiled only in the firmware unit-test build
 *     (firmware_ut_elf target in firmware/meson.build).
 **********************************************************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "hal_data.h"     /* declares g_uart0 */

volatile bool g_unity_tx_done = false;

/* UART TX-complete callback — wire this to your FSP uart callback,
 * or call it directly from rs_rtc_logger_uart_callback(). */
void unity_uart_callback(uart_callback_args_t * p_args)
{
    if (p_args && (UART_EVENT_TX_COMPLETE == p_args->event))
    {
        g_unity_tx_done = true;
    }
}

static bool g_uart_initialized = false;

static void uart_init_once(void)
{
    if (g_uart_initialized)
    {
        return;
    }
    /* Open UART with the generated configuration and a local TX-complete callback. */
    fsp_err_t err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
    if (FSP_SUCCESS == err)
    {
        g_uart_initialized = true;
    }
}

void unity_putc(int c)
{
    uart_init_once();

    if (!g_uart_initialized)
    {
        return;
    }

    uint8_t ch = (uint8_t)c;
    g_unity_tx_done = false;

    if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl, &ch, 1U))
    {
        return;
    }

    /* Spin-wait for TX complete (Unity output is low-frequency; polling is acceptable). */
    while (!g_unity_tx_done)
    {
        __asm volatile ("nop");
    }
}
