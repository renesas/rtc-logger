/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_rtc_logger_core.c
 * @brief Host-based unit tests for the RTC Event Logger lifecycle and circular
 *        event buffer, using a lightweight mock HAL shim.
 *
 * Strategy
 * --------
 * rs_rtc_logger.c and rs_rtc_logger_hal.c depend on FSP HAL function pointers
 * (p_rtc->p_api->open, p_uart->p_api->write, etc.) and on FreeRTOS
 * (xTaskCreate, xTaskGetTickCountFromISR) when Example Project mode is enabled.
 *
 * This file avoids linking those implementations at all.  Instead:
 *   - RS_RTC_LOGGER_TEST_HOST_BUILD disables FSP/FreeRTOS includes in the
 *     public type headers (rs_rtc_logger_types.h).
 *   - RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0 is forced via the host stub
 *     header below, disabling FreeRTOS task fields in the control block.
 *   - A thin mock_hal shim (this file, below the test groups) provides fake
 *     implementations of every rs_rtc_logger_hal_* function so the core
 *     (rs_rtc_logger.c) can link cleanly on the host.
 *   - rs_rtc_logger_EventTrigger() is called directly (bypassing ISR context)
 *     to drive the circular buffer.
 *
 * Test groups:
 *   RtcLoggerLifecycle — Open/Start/Stop/Close state-machine transitions.
 *   RtcLoggerBuffer    — EventTrigger() enqueue, dequeue, overflow policies.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

/* RS_RTC_LOGGER_TEST_HOST_BUILD=1 and RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=0
 * are both defined on the compiler command line in tests/meson.build
 * host_test_cargs.  Do NOT redefine them here — a bare #define after a
 * valued command-line -D produces a -Wmacro-redefined warning with clang-cl. */

#include "rs_rtc_logger/rs_rtc_logger.h"
#include "rs_rtc_logger/rs_rtc_logger_hal.h"

/* ── Mock HAL shim ───────────────────────────────────────────────────────── */
/*
 * These mock implementations replace rs_rtc_logger_hal.c in the host test
 * build (see tests/meson.build — hal.c is excluded; mock_hal_impl is compiled
 * instead).  They record call counts so tests can verify HAL interaction.
 */

static uint32_t mock_rtc_open_count;
static uint32_t mock_rtc_close_count;
static uint32_t mock_uart_open_count;
static uint32_t mock_uart_close_count;
static uint32_t mock_uart_write_count;
static fsp_err_t mock_rtc_open_ret;
static fsp_err_t mock_uart_open_ret;

/* Fake RTC time returned by time_get */
static rtc_time_t mock_rtc_time;

void mock_hal_reset(void)
{
    mock_rtc_open_count  = 0U;
    mock_rtc_close_count = 0U;
    mock_uart_open_count  = 0U;
    mock_uart_close_count = 0U;
    mock_uart_write_count = 0U;
    mock_rtc_open_ret  = FSP_SUCCESS;
    mock_uart_open_ret = FSP_SUCCESS;
    memset(&mock_rtc_time, 0, sizeof(mock_rtc_time));
    mock_rtc_time.tm_year = 124U; /* 2024 */
    mock_rtc_time.tm_mon  = 6U;
    mock_rtc_time.tm_mday = 15U;
}

/* Implementations compiled in place of rs_rtc_logger_hal.c on host */
fsp_err_t rs_rtc_logger_hal_rtc_open(rs_rtc_logger_ctrl_t * p_ctrl)
{
    (void)p_ctrl;
    mock_rtc_open_count++;
    return mock_rtc_open_ret;
}

fsp_err_t rs_rtc_logger_hal_rtc_close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    (void)p_ctrl;
    mock_rtc_close_count++;
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_hal_rtc_time_get(rs_rtc_logger_ctrl_t * p_ctrl,
                                           rtc_time_t           * p_time)
{
    (void)p_ctrl;
    *p_time = mock_rtc_time;
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_hal_rtc_time_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                           rtc_time_t           * p_time)
{
    (void)p_ctrl;
    mock_rtc_time = *p_time;
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_hal_loco_freq_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                            uint16_t               rfrl_value)
{
    (void)p_ctrl;
    /* Validate FSP range */
    if ((rfrl_value < 8U) || (rfrl_value > 510U))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_hal_uart_open(rs_rtc_logger_ctrl_t * p_ctrl)
{
    (void)p_ctrl;
    mock_uart_open_count++;
    return mock_uart_open_ret;
}

fsp_err_t rs_rtc_logger_hal_uart_close(rs_rtc_logger_ctrl_t * p_ctrl)
{
    (void)p_ctrl;
    mock_uart_close_count++;
    return FSP_SUCCESS;
}

fsp_err_t rs_rtc_logger_hal_uart_write(rs_rtc_logger_ctrl_t * p_ctrl,
                                         uint8_t        const * p_data,
                                         uint32_t               length)
{
    (void)p_ctrl;
    (void)p_data;
    (void)length;
    mock_uart_write_count++;
    /* Simulate immediate TX-complete by setting the flag the core polls. */
    p_ctrl->tx_complete = true;
    return FSP_SUCCESS;
}

/* ── Shared test fixtures ────────────────────────────────────────────────── */

static rs_rtc_logger_ctrl_t g_ctrl;
static rs_rtc_logger_cfg_t  g_cfg;

/* Dummy HAL instance stubs — only their addresses matter; Open() checks
 * NULL != p_cfg->p_rtc and NULL != p_cfg->p_uart before calling any API.
 * The actual HAL calls are intercepted by the mock functions below. */
static const rtc_instance_t  g_dummy_rtc  = {0};
static const uart_instance_t g_dummy_uart = {0};

/** Open the PES with a minimal valid configuration.
 *  p_rtc and p_uart point to the dummy instances above — non-NULL so the
 *  param check passes; the mock HAL functions ignore the pointer contents. */
static void helper_open_default(void)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    memset(&g_cfg,  0, sizeof(g_cfg));
    g_cfg.ts_format       = RS_RTC_LOGGER_TS_FORMAT_ISO8601;
    g_cfg.ts_brackets     = true;
    g_cfg.line_term       = RS_RTC_LOGGER_TERM_CRLF;
    g_cfg.overflow_policy = RS_RTC_LOGGER_OVERFLOW_OVERWRITE_OLDEST;
    g_cfg.p_callback      = NULL;
    g_cfg.p_context       = NULL;
    g_cfg.p_rtc           = &g_dummy_rtc;   /* non-NULL — passes assertion */
    g_cfg.p_uart          = &g_dummy_uart;  /* non-NULL — passes assertion */

    fsp_err_t err = rs_rtc_logger_Open(&g_ctrl, &g_cfg);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_OPEN, g_ctrl.state);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: RtcLoggerLifecycle
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(RtcLoggerLifecycle);

TEST_SETUP(RtcLoggerLifecycle)
{
    mock_hal_reset();
}

TEST_TEAR_DOWN(RtcLoggerLifecycle)
{
    /* Best-effort cleanup — ignore error if already closed */
    (void)rs_rtc_logger_Close(&g_ctrl);
}

/* Open on a zeroed control block succeeds; state becomes OPEN */
TEST(RtcLoggerLifecycle, OpenSucceeds)
{
    helper_open_default();
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_OPEN, g_ctrl.state);
}

/* Opening twice returns ALREADY_OPEN; state unchanged */
TEST(RtcLoggerLifecycle, OpenTwiceReturnsAlreadyOpen)
{
    helper_open_default();
    fsp_err_t err = rs_rtc_logger_Open(&g_ctrl, &g_cfg);
    TEST_ASSERT_EQUAL(FSP_ERR_ALREADY_OPEN, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_OPEN, g_ctrl.state);
}

/* Open with NULL ctrl returns FSP_ERR_ASSERTION */
TEST(RtcLoggerLifecycle, OpenNullCtrlReturnsAssertion)
{
    fsp_err_t err = rs_rtc_logger_Open(NULL, &g_cfg);
    TEST_ASSERT_EQUAL(FSP_ERR_ASSERTION, err);
}

/* Open → Start transitions state to STARTED */
TEST(RtcLoggerLifecycle, StartAfterOpenSucceeds)
{
    helper_open_default();
    fsp_err_t err = rs_rtc_logger_Start(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_STARTED, g_ctrl.state);
}

/* Start when already STARTED returns INVALID_MODE */
TEST(RtcLoggerLifecycle, StartTwiceReturnsInvalidMode)
{
    helper_open_default();
    rs_rtc_logger_Start(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_Start(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_MODE, err);
}

/* Start when CLOSED returns NOT_OPEN */
TEST(RtcLoggerLifecycle, StartWhenClosedReturnsNotOpen)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    fsp_err_t err = rs_rtc_logger_Start(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_ERR_NOT_OPEN, err);
}

/* STARTED → Stop → STOPPED */
TEST(RtcLoggerLifecycle, StopAfterStartSucceeds)
{
    helper_open_default();
    rs_rtc_logger_Start(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_Stop(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_STOPPED, g_ctrl.state);
}

/* Stop when already STOPPED returns INVALID_MODE */
TEST(RtcLoggerLifecycle, StopTwiceReturnsInvalidMode)
{
    helper_open_default();
    rs_rtc_logger_Start(&g_ctrl);
    rs_rtc_logger_Stop(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_Stop(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_MODE, err);
}

/* Close from OPEN state succeeds; state becomes CLOSED */
TEST(RtcLoggerLifecycle, CloseFromOpenSucceeds)
{
    helper_open_default();
    fsp_err_t err = rs_rtc_logger_Close(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_CLOSED, g_ctrl.state);
}

/* Close while STARTED returns INVALID_MODE (Stop() required first) */
TEST(RtcLoggerLifecycle, CloseWhileStartedReturnsInvalidMode)
{
    helper_open_default();
    rs_rtc_logger_Start(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_Close(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_ERR_INVALID_MODE, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_STARTED, g_ctrl.state);
}

/* Close when already CLOSED returns NOT_OPEN */
TEST(RtcLoggerLifecycle, CloseWhenClosedReturnsNotOpen)
{
    memset(&g_ctrl, 0, sizeof(g_ctrl));
    fsp_err_t err = rs_rtc_logger_Close(&g_ctrl);
    TEST_ASSERT_EQUAL(FSP_ERR_NOT_OPEN, err);
}

/* Full round-trip: Open → Start → Stop → Close */
TEST(RtcLoggerLifecycle, FullRoundTrip)
{
    helper_open_default();
    TEST_ASSERT_EQUAL(FSP_SUCCESS, rs_rtc_logger_Start(&g_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, rs_rtc_logger_Stop(&g_ctrl));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, rs_rtc_logger_Close(&g_ctrl));
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_CLOSED, g_ctrl.state);
}

/* VersionGet returns encoded version matching header macros */
TEST(RtcLoggerLifecycle, VersionGetMatchesHeaderMacros)
{
    uint32_t  ver  = 0U;
    fsp_err_t err  = rs_rtc_logger_VersionGet(&ver);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_VERSION, ver);
}

/* TimestampFormatSet rejects invalid format value */
TEST(RtcLoggerLifecycle, TimestampFormatSetInvalidReturnsAssertion)
{
    helper_open_default();
    fsp_err_t err = rs_rtc_logger_TimestampFormatSet(
        &g_ctrl, (rs_rtc_logger_ts_format_t)0xFFU);
    TEST_ASSERT_EQUAL(FSP_ERR_ASSERTION, err);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(RtcLoggerLifecycle)
{
    RUN_TEST_CASE(RtcLoggerLifecycle, OpenSucceeds);
    RUN_TEST_CASE(RtcLoggerLifecycle, OpenTwiceReturnsAlreadyOpen);
    RUN_TEST_CASE(RtcLoggerLifecycle, OpenNullCtrlReturnsAssertion);
    RUN_TEST_CASE(RtcLoggerLifecycle, StartAfterOpenSucceeds);
    RUN_TEST_CASE(RtcLoggerLifecycle, StartTwiceReturnsInvalidMode);
    RUN_TEST_CASE(RtcLoggerLifecycle, StartWhenClosedReturnsNotOpen);
    RUN_TEST_CASE(RtcLoggerLifecycle, StopAfterStartSucceeds);
    RUN_TEST_CASE(RtcLoggerLifecycle, StopTwiceReturnsInvalidMode);
    RUN_TEST_CASE(RtcLoggerLifecycle, CloseFromOpenSucceeds);
    RUN_TEST_CASE(RtcLoggerLifecycle, CloseWhileStartedReturnsInvalidMode);
    RUN_TEST_CASE(RtcLoggerLifecycle, CloseWhenClosedReturnsNotOpen);
    RUN_TEST_CASE(RtcLoggerLifecycle, FullRoundTrip);
    RUN_TEST_CASE(RtcLoggerLifecycle, VersionGetMatchesHeaderMacros);
    RUN_TEST_CASE(RtcLoggerLifecycle, TimestampFormatSetInvalidReturnsAssertion);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: RtcLoggerBuffer
 * Exercises EventTrigger() enqueue, EventDequeue(), overflow policies,
 * sequence counter, and diagnostics counters.
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(RtcLoggerBuffer);

TEST_SETUP(RtcLoggerBuffer)
{
    mock_hal_reset();
    helper_open_default();
    rs_rtc_logger_Start(&g_ctrl);
    /* Mark time as valid so buffer entries have a real timestamp */
    rs_rtc_logger_ClockValidate(&g_ctrl, true);
}

TEST_TEAR_DOWN(RtcLoggerBuffer)
{
    rs_rtc_logger_Stop(&g_ctrl);
    rs_rtc_logger_Close(&g_ctrl);
}

/* Single enqueue: buf_count increases */
TEST(RtcLoggerBuffer, SingleEnqueueIncrementsCount)
{
    fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"A", 1U);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(1U, g_ctrl.buf_count);
}

/* Single enqueue → dequeue: entry payload matches */
TEST(RtcLoggerBuffer, DequeueReturnsEnqueuedPayload)
{
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"Hello", 5U);
    rs_rtc_logger_event_entry_t out;
    memset(&out, 0, sizeof(out));
    fsp_err_t err = rs_rtc_logger_EventDequeue(&g_ctrl, &out);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL_STRING("Hello", (char*)out.event_data);
    TEST_ASSERT_EQUAL(5U, out.event_data_len);
}

/* Dequeue on empty buffer returns BUFFER_EMPTY */
TEST(RtcLoggerBuffer, DequeueEmptyReturnsBufferEmpty)
{
    rs_rtc_logger_event_entry_t out;
    fsp_err_t err = rs_rtc_logger_EventDequeue(&g_ctrl, &out);
    TEST_ASSERT_EQUAL(FSP_ERR_BUFFER_EMPTY, err);
}

/* Fill buffer to depth; all entries must succeed */
TEST(RtcLoggerBuffer, FillToDepthSucceeds)
{
    for (uint32_t i = 0U; i < RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH; i++)
    {
        fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"X", 1U);
        TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    }
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH, g_ctrl.buf_count);
}

/* Overflow with OVERWRITE_OLDEST: new entry is accepted; oldest is dropped */
TEST(RtcLoggerBuffer, OverwriteOldestPolicyOnFull)
{
    g_cfg.overflow_policy = RS_RTC_LOGGER_OVERFLOW_OVERWRITE_OLDEST;
    /* Fill buffer */
    for (uint32_t i = 0U; i < RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH; i++)
    {
        rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"old", 3U);
    }
    /* Overflow — should succeed under OVERWRITE_OLDEST */
    fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"new", 3U);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    /* Count stays at depth */
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH, g_ctrl.buf_count);
    /* events_dropped records the lost entry */
    TEST_ASSERT_GREATER_THAN(0U, g_ctrl.events_dropped);
}

/* Overflow with DROP_NEWEST: new entry is rejected; returns FSP_ERR_OVERFLOW */
TEST(RtcLoggerBuffer, DropNewestPolicyOnFull)
{
    g_cfg.overflow_policy = RS_RTC_LOGGER_OVERFLOW_DROP_NEWEST;
    /* Fill buffer */
    for (uint32_t i = 0U; i < RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH; i++)
    {
        rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"keep", 4U);
    }
    fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"lost", 4U);
    TEST_ASSERT_EQUAL(FSP_ERR_OVERFLOW, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH, g_ctrl.buf_count);
    TEST_ASSERT_GREATER_THAN(0U, g_ctrl.events_dropped);
}

/* Sequence counter increments monotonically */
TEST(RtcLoggerBuffer, SequenceCounterIncrements)
{
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"e1", 2U);
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"e2", 2U);

    rs_rtc_logger_event_entry_t a, b;
    rs_rtc_logger_EventDequeue(&g_ctrl, &a);
    rs_rtc_logger_EventDequeue(&g_ctrl, &b);

    TEST_ASSERT_EQUAL(a.seq_count + 1U, b.seq_count);
}

/* events_logged counter tracks successful enqueues */
TEST(RtcLoggerBuffer, EventsLoggedCounterTracksEnqueues)
{
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"x", 1U);
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"y", 1U);
    TEST_ASSERT_EQUAL(2U, g_ctrl.events_logged);
}

/* EventTrigger in STOPPED state — Expert mode accepts STARTED and STOPPED
 * so events can be buffered while output is paused.  Only CLOSED and OPEN
 * (not yet started) return FSP_ERR_NOT_OPEN. */
TEST(RtcLoggerBuffer, TriggerWhenStoppedAcceptsEvent)
{
    rs_rtc_logger_Stop(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"z", 1U);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    /* Event is buffered even while stopped */
    TEST_ASSERT_EQUAL(1U, g_ctrl.buf_count);
    /* Restart for tear-down */
    rs_rtc_logger_Start(&g_ctrl);
}

/* EventTrigger when CLOSED returns NOT_OPEN */
TEST(RtcLoggerBuffer, TriggerWhenClosedReturnsNotOpen)
{
    rs_rtc_logger_Stop(&g_ctrl);
    rs_rtc_logger_Close(&g_ctrl);
    fsp_err_t err = rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"z", 1U);
    TEST_ASSERT_EQUAL(FSP_ERR_NOT_OPEN, err);
    /* Re-open for tear-down */
    rs_rtc_logger_Open(&g_ctrl, &g_cfg);
    rs_rtc_logger_Start(&g_ctrl);
}

/* Payload truncation: data longer than max is stored truncated */
TEST(RtcLoggerBuffer, LongPayloadTruncatedSafely)
{
    uint8_t long_payload[RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN + 16U];
    memset(long_payload, 'A', sizeof(long_payload));

    fsp_err_t err = rs_rtc_logger_EventTrigger(
        &g_ctrl, long_payload, (uint32_t)sizeof(long_payload));
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);

    rs_rtc_logger_event_entry_t out;
    rs_rtc_logger_EventDequeue(&g_ctrl, &out);

    /* Stored length must not exceed the configured maximum */
    TEST_ASSERT_LESS_OR_EQUAL(RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U,
                               out.event_data_len);
    /* Must still be NUL-terminated */
    TEST_ASSERT_EQUAL('\0', out.event_data[out.event_data_len]);
}

/* StatusGet reflects live state */
TEST(RtcLoggerBuffer, StatusGetReflectsState)
{
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"s", 1U);

    rs_rtc_logger_status_t status;
    fsp_err_t err = rs_rtc_logger_StatusGet(&g_ctrl, &status);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    TEST_ASSERT_EQUAL(RS_RTC_LOGGER_STATE_STARTED, status.state);
    TEST_ASSERT_EQUAL(1U, status.events_logged);
    TEST_ASSERT_EQUAL(1U, status.buf_count);
}

/* SeqCountSet restores counter (warm-reset scenario) */
TEST(RtcLoggerBuffer, SeqCountSetRestoresCounter)
{
    fsp_err_t err = rs_rtc_logger_SeqCountSet(&g_ctrl, 999U);
    TEST_ASSERT_EQUAL(FSP_SUCCESS, err);
    rs_rtc_logger_EventTrigger(&g_ctrl, (uint8_t*)"r", 1U);
    TEST_ASSERT_EQUAL(1000U, g_ctrl.seq_count);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(RtcLoggerBuffer)
{
    RUN_TEST_CASE(RtcLoggerBuffer, SingleEnqueueIncrementsCount);
    RUN_TEST_CASE(RtcLoggerBuffer, DequeueReturnsEnqueuedPayload);
    RUN_TEST_CASE(RtcLoggerBuffer, DequeueEmptyReturnsBufferEmpty);
    RUN_TEST_CASE(RtcLoggerBuffer, FillToDepthSucceeds);
    RUN_TEST_CASE(RtcLoggerBuffer, OverwriteOldestPolicyOnFull);
    RUN_TEST_CASE(RtcLoggerBuffer, DropNewestPolicyOnFull);
    RUN_TEST_CASE(RtcLoggerBuffer, SequenceCounterIncrements);
    RUN_TEST_CASE(RtcLoggerBuffer, EventsLoggedCounterTracksEnqueues);
    RUN_TEST_CASE(RtcLoggerBuffer, TriggerWhenStoppedAcceptsEvent);
    RUN_TEST_CASE(RtcLoggerBuffer, TriggerWhenClosedReturnsNotOpen);
    RUN_TEST_CASE(RtcLoggerBuffer, LongPayloadTruncatedSafely);
    RUN_TEST_CASE(RtcLoggerBuffer, StatusGetReflectsState);
    RUN_TEST_CASE(RtcLoggerBuffer, SeqCountSetRestoresCounter);
}
