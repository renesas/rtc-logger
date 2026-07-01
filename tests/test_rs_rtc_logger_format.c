/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_rtc_logger_format.c
 * @brief Unit tests for the RTC Event Logger pure formatting engine.
 *
 * Tests rs_rtc_logger_format_timestamp() and rs_rtc_logger_format_line()
 * (declared in rs_rtc_logger_format.h).  Both functions are pure — no I/O,
 * no FSP HAL calls, no FreeRTOS.  Compiled with RS_RTC_LOGGER_TEST_HOST_BUILD
 * so FSP/FreeRTOS headers are replaced by host stubs.
 *
 * Test groups:
 *   RtcFormat     — rs_rtc_logger_format_timestamp(): all four formats,
 *                   bracket variants, boundary times, invalid inputs.
 *   RtcFormatLine — rs_rtc_logger_format_line(): full log line composition,
 *                   all terminators, time-valid/invalid flag.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include "unity_fixture.h"

/* RS_RTC_LOGGER_TEST_HOST_BUILD is defined on the compiler command line
 * (-DRS_RTC_LOGGER_TEST_HOST_BUILD=1 in tests/meson.build host_test_cargs).
 * It instructs rs_rtc_logger_types.h to pull in the host stubs header instead
 * of bsp_api.h / r_rtc_api.h / r_uart_api.h / FreeRTOS. */

#include "rs_rtc_logger/rs_rtc_logger_format.h"
#include "rs_rtc_logger/rs_rtc_logger_types.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/** Build an rtc_time_t from human-readable fields.
 *  year_ad: calendar year (e.g. 2024); tm_year stored as year_ad - 1900.
 *  month_1based: 1..12 → stored as 0..11. */
static rtc_time_t make_time(uint32_t year_ad, uint32_t month_1based,
                             uint32_t day,     uint32_t hour,
                             uint32_t minute,  uint32_t second)
{
    rtc_time_t t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year_ad - 1900U;
    t.tm_mon  = month_1based - 1U;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    return t;
}

/** Build a minimal rs_rtc_logger_event_entry_t for line-format tests. */
static rs_rtc_logger_event_entry_t make_entry(rtc_time_t t, const char * payload,
                                               uint32_t seq, bool time_valid)
{
    rs_rtc_logger_event_entry_t e;
    memset(&e, 0, sizeof(e));
    e.timestamp  = t;
    e.seq_count  = seq;
    e.time_valid = time_valid;
    if (payload)
    {
        size_t len = strlen(payload);
        if (len >= RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN)
        {
            len = RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U;
        }
        memcpy(e.event_data, payload, len);
        e.event_data[len] = '\0';
        e.event_data_len  = (uint32_t)len;
    }
    return e;
}

/* ── Test group: RtcFormat ────────────────────────────────────────────────── */

TEST_GROUP(RtcFormat);

static char g_buf[64];
static rtc_time_t g_time;

TEST_SETUP(RtcFormat)
{
    memset(g_buf, 0xAA, sizeof(g_buf));
    g_time = make_time(2024U, 7U, 15U, 14U, 30U, 5U);
}

TEST_TEAR_DOWN(RtcFormat)
{
}

/* ISO8601 with brackets → "[2024-07-15 14:30:05]" */
TEST(RtcFormat, ISO8601WithBrackets)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[2024-07-15 14:30:05]", g_buf);
    TEST_ASSERT_EQUAL(strlen("[2024-07-15 14:30:05]"), n);
}

/* ISO8601 without brackets → "2024-07-15 14:30:05" */
TEST(RtcFormat, ISO8601NoBrackets)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, false, g_buf, sizeof(g_buf));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("2024-07-15 14:30:05", g_buf);
}

/* TIME_ONLY with brackets → "[14:30:05]" */
TEST(RtcFormat, TimeOnlyWithBrackets)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_TIME_ONLY, true, g_buf, sizeof(g_buf));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[14:30:05]", g_buf);
}

/* COMPACT with brackets → "[20240715-143005]" */
TEST(RtcFormat, CompactWithBrackets)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_COMPACT, true, g_buf, sizeof(g_buf));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[20240715-143005]", g_buf);
}

/* SYSLOG with brackets → "[Jul 15 14:30:05]" */
TEST(RtcFormat, SyslogWithBrackets)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_SYSLOG, true, g_buf, sizeof(g_buf));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[Jul 15 14:30:05]", g_buf);
}

/* SYSLOG — January (index 0); RFC 3164 space-pads single-digit days: " 1" */
TEST(RtcFormat, SyslogJanuary)
{
    g_time = make_time(2024U, 1U, 1U, 0U, 0U, 0U);
    rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_SYSLOG, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING("[Jan  1 00:00:00]", g_buf);
}

/* SYSLOG — December (index 11) */
TEST(RtcFormat, SyslogDecember)
{
    g_time = make_time(2024U, 12U, 31U, 23U, 59U, 59U);
    rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_SYSLOG, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING("[Dec 31 23:59:59]", g_buf);
}

/* Midnight / start-of-day boundary */
TEST(RtcFormat, MidnightBoundary)
{
    g_time = make_time(2000U, 1U, 1U, 0U, 0U, 0U);
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[2000-01-01 00:00:00]", g_buf);
}

/* End-of-day boundary */
TEST(RtcFormat, EndOfDayBoundary)
{
    g_time = make_time(2099U, 12U, 31U, 23U, 59U, 59U);
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL_STRING("[2099-12-31 23:59:59]", g_buf);
}

/* NULL pointer → must return 0 without crashing */
TEST(RtcFormat, NullPointerReturnsZero)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        NULL, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL(0U, n);
}

/* NULL output buffer → must return 0 without crashing */
TEST(RtcFormat, NullBufferReturnsZero)
{
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, NULL, 64U);
    TEST_ASSERT_EQUAL(0U, n);
}

/* Buffer too small → must return 0 (no partial write) */
TEST(RtcFormat, BufferTooSmallReturnsZero)
{
    char tiny[4];
    uint32_t n = rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL(0U, n);
}

/* Output is always NUL-terminated */
TEST(RtcFormat, OutputIsNulTerminated)
{
    memset(g_buf, 0xFF, sizeof(g_buf));
    rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));
    /* Scan for NUL within buffer */
    bool found_nul = false;
    for (size_t i = 0; i < sizeof(g_buf); i++)
    {
        if (g_buf[i] == '\0') { found_nul = true; break; }
    }
    TEST_ASSERT_TRUE(found_nul);
}

/* Single-digit day/month/hour/min/sec formatted with leading zero */
TEST(RtcFormat, LeadingZeroForSingleDigit)
{
    g_time = make_time(2024U, 1U, 5U, 3U, 7U, 9U);
    rs_rtc_logger_format_timestamp(
        &g_time, RS_RTC_LOGGER_TS_FORMAT_ISO8601, true, g_buf, sizeof(g_buf));
    TEST_ASSERT_EQUAL_STRING("[2024-01-05 03:07:09]", g_buf);
}

/* ── Test group runner ──────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(RtcFormat)
{
    RUN_TEST_CASE(RtcFormat, ISO8601WithBrackets);
    RUN_TEST_CASE(RtcFormat, ISO8601NoBrackets);
    RUN_TEST_CASE(RtcFormat, TimeOnlyWithBrackets);
    RUN_TEST_CASE(RtcFormat, CompactWithBrackets);
    RUN_TEST_CASE(RtcFormat, SyslogWithBrackets);
    RUN_TEST_CASE(RtcFormat, SyslogJanuary);
    RUN_TEST_CASE(RtcFormat, SyslogDecember);
    RUN_TEST_CASE(RtcFormat, MidnightBoundary);
    RUN_TEST_CASE(RtcFormat, EndOfDayBoundary);
    RUN_TEST_CASE(RtcFormat, NullPointerReturnsZero);
    RUN_TEST_CASE(RtcFormat, NullBufferReturnsZero);
    RUN_TEST_CASE(RtcFormat, BufferTooSmallReturnsZero);
    RUN_TEST_CASE(RtcFormat, OutputIsNulTerminated);
    RUN_TEST_CASE(RtcFormat, LeadingZeroForSingleDigit);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test group: RtcFormatLine
 * Tests rs_rtc_logger_format_line() — full log line composition.
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(RtcFormatLine);

static char g_line[256];
static rs_rtc_logger_event_entry_t g_entry;

TEST_SETUP(RtcFormatLine)
{
    memset(g_line, 0, sizeof(g_line));
    rtc_time_t t = make_time(2024U, 7U, 15U, 14U, 30U, 5U);
    g_entry = make_entry(t, "ButtonPress", 1U, true);
}

TEST_TEAR_DOWN(RtcFormatLine)
{
}

/* Basic CRLF line */
TEST(RtcFormatLine, CRLFTerminator)
{
    uint32_t n = rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    /* Must contain timestamp */
    TEST_ASSERT_NOT_NULL(strstr(g_line, "[2024-07-15 14:30:05]"));
    /* Must contain payload */
    TEST_ASSERT_NOT_NULL(strstr(g_line, "ButtonPress"));
    /* Must contain sequence number */
    TEST_ASSERT_NOT_NULL(strstr(g_line, "#1"));
    /* Must end with CRLF */
    TEST_ASSERT_EQUAL('\r', g_line[n - 2]);
    TEST_ASSERT_EQUAL('\n', g_line[n - 1]);
}

/* LF-only terminator */
TEST(RtcFormatLine, LFTerminator)
{
    uint32_t n = rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_LF,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL('\n', g_line[n - 1]);
    /* Must NOT end with \r\n */
    if (n >= 2U)
    {
        TEST_ASSERT_NOT_EQUAL('\r', g_line[n - 2]);
    }
}

/* CR-only terminator */
TEST(RtcFormatLine, CRTerminator)
{
    uint32_t n = rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CR,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_EQUAL('\r', g_line[n - 1]);
}

/* time_valid = false → timestamp shows all-zeros placeholder */
TEST(RtcFormatLine, TimeInvalidShowsZeroTimestamp)
{
    rtc_time_t t = make_time(2024U, 7U, 15U, 14U, 30U, 5U);
    rs_rtc_logger_event_entry_t e = make_entry(t, "Init", 0U, false);

    uint32_t n = rs_rtc_logger_format_line(
        &e,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    /* The implementation substitutes a zero timestamp when time_valid is false */
    TEST_ASSERT_NOT_NULL(strstr(g_line, "[0000-00-00 00:00:00]"));
}

/* Sequence number increments reflected in line */
TEST(RtcFormatLine, SequenceNumberInLine)
{
    rtc_time_t t = make_time(2024U, 1U, 1U, 0U, 0U, 0U);
    rs_rtc_logger_event_entry_t e = make_entry(t, "Tick", 42U, true);

    rs_rtc_logger_format_line(
        &e,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_LF,
        g_line, sizeof(g_line));

    TEST_ASSERT_NOT_NULL(strstr(g_line, "#42"));
}

/* Empty payload */
TEST(RtcFormatLine, EmptyPayload)
{
    rtc_time_t t = make_time(2024U, 6U, 1U, 12U, 0U, 0U);
    rs_rtc_logger_event_entry_t e = make_entry(t, "", 0U, true);

    uint32_t n = rs_rtc_logger_format_line(
        &e,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    /* Line should still have timestamp and terminator */
    TEST_ASSERT_NOT_NULL(strstr(g_line, "[2024-06-01 12:00:00]"));
    TEST_ASSERT_EQUAL('\r', g_line[n - 2]);
    TEST_ASSERT_EQUAL('\n', g_line[n - 1]);
}

/* Buffer too small → returns 0 */
TEST(RtcFormatLine, BufferTooSmallReturnsZero)
{
    char tiny[8];
    uint32_t n = rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        tiny, sizeof(tiny));

    TEST_ASSERT_EQUAL(0U, n);
}

/* NULL entry pointer → returns 0 */
TEST(RtcFormatLine, NullEntryReturnsZero)
{
    uint32_t n = rs_rtc_logger_format_line(
        NULL,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        g_line, sizeof(g_line));

    TEST_ASSERT_EQUAL(0U, n);
}

/* Output always NUL-terminated */
TEST(RtcFormatLine, OutputIsNulTerminated)
{
    memset(g_line, 0xFF, sizeof(g_line));
    rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_ISO8601,
        true,
        RS_RTC_LOGGER_TERM_CRLF,
        g_line, sizeof(g_line));

    bool found_nul = false;
    for (size_t i = 0; i < sizeof(g_line); i++)
    {
        if (g_line[i] == '\0') { found_nul = true; break; }
    }
    TEST_ASSERT_TRUE(found_nul);
}

/* TIME_ONLY format in a full line */
TEST(RtcFormatLine, TimeOnlyFormatInLine)
{
    uint32_t n = rs_rtc_logger_format_line(
        &g_entry,
        RS_RTC_LOGGER_TS_FORMAT_TIME_ONLY,
        true,
        RS_RTC_LOGGER_TERM_LF,
        g_line, sizeof(g_line));

    TEST_ASSERT_GREATER_THAN(0U, n);
    TEST_ASSERT_NOT_NULL(strstr(g_line, "[14:30:05]"));
    /* Date fields must NOT appear */
    TEST_ASSERT_NULL(strstr(g_line, "2024"));
}

/* ── Test group runner ─────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(RtcFormatLine)
{
    RUN_TEST_CASE(RtcFormatLine, CRLFTerminator);
    RUN_TEST_CASE(RtcFormatLine, LFTerminator);
    RUN_TEST_CASE(RtcFormatLine, CRTerminator);
    RUN_TEST_CASE(RtcFormatLine, TimeInvalidShowsZeroTimestamp);
    RUN_TEST_CASE(RtcFormatLine, SequenceNumberInLine);
    RUN_TEST_CASE(RtcFormatLine, EmptyPayload);
    RUN_TEST_CASE(RtcFormatLine, BufferTooSmallReturnsZero);
    RUN_TEST_CASE(RtcFormatLine, NullEntryReturnsZero);
    RUN_TEST_CASE(RtcFormatLine, OutputIsNulTerminated);
    RUN_TEST_CASE(RtcFormatLine, TimeOnlyFormatInLine);
}
