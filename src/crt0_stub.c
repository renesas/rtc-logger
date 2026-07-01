/*
 * crt0_stub.c - Weak stub to satisfy picolibc libc.a reference to crt0.o
 *
 * picolibc's libc.a references _start from crt0. The FSP uses Reset_Handler
 * as the entry point (set via ENTRY() in fsp_gen.lld), so _start is never
 * called. This weak stub satisfies the linker without providing any code.
 */
#include <stdnoreturn.h>

__attribute__((weak)) noreturn void _start(void)
{
    /* Should never be called — FSP Reset_Handler is the entry point */
    while (1) { ; }
}

/* Weak stub for UART callback — defined in blinky_thread_entry_ra4e2.c
 * in the app build. The UT build doesn't include the blinky thread entry,
 * so this weak stub satisfies hal_data.c's reference. */
#include "r_uart_api.h"
__attribute__((weak)) void rs_rtc_logger_uart_callback(
    uart_callback_args_t * p_args)
{
    (void) p_args;
}
