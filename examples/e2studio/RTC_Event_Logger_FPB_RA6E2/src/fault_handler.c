/* src/fault_handler.c
 *
 * Overrides the weak Default_Handler aliases declared in startup.c, plus the
 * FreeRTOS stack-overflow hook now required by configCHECK_FOR_STACK_OVERFLOW.
 *
 * Drop this file under src/ in the e2 studio project, then:
 *   - Right-click the project -> Refresh (so the new file is picked up)
 *   - Project -> Clean, then rebuild
 *   - Check the build console: fault_handler.c should appear in the compile list
 *   - After rebuild, grep the .map file for "HardFault_Handler" / "UsageFault_Handler"
 *     etc. and confirm they resolve to fault_handler.o, not Default_Handler.
 */

#include "hal_data.h"
#include "FreeRTOS.h"
#include "task.h"

/* Snapshot of fault status registers — inspect these in the debugger
 * (Memory view or Expressions) when halted at the breakpoint below. */
volatile uint32_t g_fault_cfsr;   /* Configurable Fault Status Register */
volatile uint32_t g_fault_hfsr;   /* HardFault Status Register          */
volatile uint32_t g_fault_mmfar;  /* MemManage Fault Address Register   */
volatile uint32_t g_fault_bfar;   /* BusFault Address Register          */
volatile uint32_t * g_fault_psp;  /* Process stack pointer at fault time */

static void fault_capture_and_halt(void)
{
    g_fault_cfsr  = SCB->CFSR;
    g_fault_hfsr  = SCB->HFSR;
    g_fault_mmfar = SCB->MMFAR;
    g_fault_bfar  = SCB->BFAR;

    __asm volatile ("MRS %0, psp" : "=r" (g_fault_psp));

    /* Halts here with a debugger attached. Without one, this falls through
     * to the infinite loop below instead of re-faulting. */
    __asm volatile ("bkpt #0");

    for (;;)
    {
        /* Stay here for inspection — do not let execution continue
         * past a fault with corrupted state. */
    }
}

void HardFault_Handler(void)
{
    fault_capture_and_halt();
}

void MemManage_Handler(void)
{
    fault_capture_and_halt();
}

void BusFault_Handler(void)
{
    fault_capture_and_halt();
}

void UsageFault_Handler(void)
{
    fault_capture_and_halt();
}

void SecureFault_Handler(void)
{
    fault_capture_and_halt();
}

/* Required now that configCHECK_FOR_STACK_OVERFLOW is non-zero.
 * pcTaskName tells you exactly which task overflowed — this fires
 * BEFORE any fault occurs, from inside the tick/PendSV path.
 *
 * Globals (not locals) on purpose: inspect these in the debugger's
 * Expressions/Memory view after the breakpoint trips, even if the
 * call stack itself looks unreliable by this point. */
volatile TaskHandle_t g_overflow_task_handle;
volatile char         g_overflow_task_name[configMAX_TASK_NAME_LEN];

void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    g_overflow_task_handle = xTask;

    /* Copy defensively, one byte at a time, with a hard bound —
     * pcTaskName may itself point into memory the overflow already
     * clobbered, so don't trust strlen()/strcpy() on it. */
    uint32_t i = 0U;
    for (; (i < (configMAX_TASK_NAME_LEN - 1U)) && ('\0' != pcTaskName[i]); i++)
    {
        g_overflow_task_name[i] = pcTaskName[i];
    }
    g_overflow_task_name[i] = '\0';

    /* This runs in interrupt context (tick/PendSV) — do not call
     * vTaskDelay(), and avoid spin-waiting on UART completion flags
     * here; those depend on ISR priorities you haven't audited.
     * If you want a UART message too, an unconditional, non-blocking
     * R_SCI_UART_Write(&g_uart0_ctrl, ...) call is safe to add right
     * here (it only pokes registers and arms an interrupt — it
     * doesn't wait), but the global above is the reliable source of
     * truth regardless of whether that write makes it out. */

    __asm volatile ("bkpt #0");

    for (;;)
    {
        /* Halt here. Check g_overflow_task_name in the debugger. */
    }
}
