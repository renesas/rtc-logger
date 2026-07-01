#include "unity_fixture.h"

static void RunAllTests(void)
{
    RUN_TEST_GROUP(RtcFormat);
    RUN_TEST_GROUP(RtcFormatLine);
    RUN_TEST_GROUP(RtcLoggerLifecycle);
    RUN_TEST_GROUP(RtcLoggerBuffer);
}

int main(int argc, const char * argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
