/***********************************************************************************************************************
 * @file syscalls_sci_uart.c
 * @brief Minimal newlib syscall stubs for the firmware unit-test build.
 *
 * Provides _write() so that printf() / puts() calls from Unity reach the
 * SCI UART VCOM.  All other syscalls return sensible no-op values to satisfy
 * the linker without pulling in a full semihosting implementation.
 *
 * Include in the firmware_ut_elf source list in firmware/meson.build.
 **********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>
#include "hal_data.h"

extern volatile bool g_unity_tx_done;  /* defined in unity_port_sci_uart.c */

/* ── _write ──────────────────────────────────────────────────────────────── */

int _write(int fd, char * ptr, int len)
{
    (void)fd;
    if ((NULL == ptr) || (len <= 0))
    {
        return 0;
    }

    for (int i = 0; i < len; i++)
    {
        g_unity_tx_done = false;
        uint8_t ch = (uint8_t)ptr[i];
        if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl, &ch, 1U))
        {
            return i;
        }
        while (!g_unity_tx_done)
        {
            __asm volatile ("nop");
        }
    }
    return len;
}

/* ── Minimal newlib stubs ────────────────────────────────────────────────── */

int _read(int fd, char * ptr, int len)   { (void)fd; (void)ptr; (void)len; return 0; }
int _close(int fd)                        { (void)fd; return -1; }
int _fstat(int fd, struct stat * st)      { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                       { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir)      { (void)fd; (void)ptr; (void)dir; return 0; }
void _exit(int status)                    { (void)status; while(1) { __asm volatile("nop"); } }
int _kill(int pid, int sig)               { (void)pid; (void)sig; return -1; }
int _getpid(void)                         { return 1; }
