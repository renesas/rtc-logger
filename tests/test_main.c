#include "unity_fixture.h"

static void RunAllTests(void)
{
    RUN_TEST_GROUP(picolibc_stdio_to_sci_uart_init);
    RUN_TEST_GROUP(picolibc_stdio_to_sci_uart_rxtx);
}

int main(int argc, const char * argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}