/*
 * crt0_stub.c - Weak stubs for the firmware unit-test build.
 *
 * 1. _start — satisfies picolibc libc.a's reference to crt0.o.
 *    The FSP uses Reset_Handler as the entry point; _start is never called.
 *
 * 2. rs_rtc_logger_uart_callback — the UART TX-complete callback registered
 *    in ra_gen/common_data.c for g_uart0.  In the app build this is defined
 *    in blinky_thread_entry_ra4e2/ra6e2.c.  In the UT build that file is
 *    excluded, so this weak definition is used instead.
 *
 *    The UT build needs the callback to forward TX-complete events to
 *    unity_uart_callback() so that unity_port_sci_uart.c's spin-wait on
 *    g_unity_tx_done can complete.  Without this forwarding the first
 *    character is sent and the spin-wait hangs forever, producing only ':'
 *    on the serial terminal.
 */
#include "r_uart_api.h"

/* Declared in tests/target/unity_port_sci_uart.c */
extern void unity_uart_callback(uart_callback_args_t * p_args);

__attribute__((weak, noreturn)) void _start(void)
{
    /* Should never be called — FSP Reset_Handler is the entry point */
    while (1) { ; }
}

__attribute__((weak)) void rs_rtc_logger_uart_callback(
    uart_callback_args_t * p_args)
{
    /* Forward to Unity's TX-complete handler so the spin-wait in
     * unity_putc() unblocks after each byte is transmitted. */
    unity_uart_callback(p_args);
}
