/***********************************************************************************************************************
 * On-target Unity firmware entry point.
 *
 * This file replaces the RASC-generated main.c in the unit-test firmware build.
 * It is linked instead of ra_gen/main.c so that the FreeRTOS scheduler is still
 * started (via hal_entry.c → R_BSP_WarmStart → vTaskStartScheduler) but the
 * blinky_thread_entry is replaced by the Unity test runner task defined here.
 *
 * The Unity output is routed through the SCI UART (VCOM) via the
 * unity_putc() implementation in tests/target/unity_port_sci_uart.c.
 **********************************************************************************************************************/

#include "unity_fixture.h"

static void RunAllTests(void)
{
    RUN_TEST_GROUP(RtcFormat);
    RUN_TEST_GROUP(RtcFormatLine);
    RUN_TEST_GROUP(RtcLoggerLifecycle);
    RUN_TEST_GROUP(RtcLoggerBuffer);
}

/* Argv passed to UnityMain — "-v" enables verbose per-test output over VCOM. */
static const char * run_argv[] = {
    __FILE__,
    "-v"
};

int main(void)
{
    UnityPrint("===== UNITY LOG BEGIN ======"); UNITY_PRINT_EOL();
    int code = UnityMain(2, run_argv, RunAllTests);
    UnityPrint("===== UNITY LOG END ======"); UNITY_PRINT_EOL();
    return code;
}
