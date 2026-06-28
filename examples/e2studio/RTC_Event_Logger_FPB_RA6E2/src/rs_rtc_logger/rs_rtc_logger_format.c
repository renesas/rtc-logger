/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_format.c
 * @brief Pure timestamp formatting engine for the RTC Event Logger PES.
 *
 * This module is intentionally pure: no I/O, no side effects, no FreeRTOS calls,
 * no FSP HAL calls. All arithmetic is integer-only (FR-014). The public surface
 * is the three functions declared in rs_rtc_logger_format.h; all helpers are
 * file-scoped static, named with the rm_rlf_ private prefix per Rule 6.
 */

#include <rs_rtc_logger/rs_rtc_logger_format.h>
#include <string.h>
#include <stdint.h>

/***********************************************************************************************************************
 * Private constants
 **********************************************************************************************************************/

/** Month abbreviations for syslog format (index 0-11 -> Jan-Dec) */
static const char * const rm_rlf_month_abbr[12] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/***********************************************************************************************************************
 * Private helper functions
 **********************************************************************************************************************/

/* Forward declaration — format_terminator is file-local (PG-3 fix: removed from public header) */
static uint32_t rs_rtc_logger_format_terminator(rs_rtc_logger_term_t term,
                                                  char               * p_buf,
                                                  uint32_t             offset,
                                                  uint32_t             buf_size);

/**
 * @brief Write a 2-digit decimal integer (with leading zero) into buf at offset.
 *
 * @param[out] p_buf    Output buffer.
 * @param[in]  offset   Starting byte offset.
 * @param[in]  value    Value to format (0-99).
 *
 * @return New offset (offset + 2).
 */
static uint32_t rm_rlf_write_u2(char * p_buf, uint32_t offset, uint32_t value)
{
    p_buf[offset]     = (char)('0' + (value / 10U));
    p_buf[offset + 1] = (char)('0' + (value % 10U));
    return offset + 2U;
}

/**
 * @brief Write a 4-digit decimal year into buf at offset.
 *
 * rtc_time_t.tm_year is years since 1900 (POSIX convention used by FSP).
 *
 * @param[out] p_buf    Output buffer.
 * @param[in]  offset   Starting byte offset.
 * @param[in]  year     Year value (years since 1900).
 *
 * @return New offset (offset + 4).
 */
static uint32_t rm_rlf_write_year(char * p_buf, uint32_t offset, uint32_t year)
{
    uint32_t full_year = year + 1900U;
    p_buf[offset]     = (char)('0' + (full_year / 1000U));
    p_buf[offset + 1] = (char)('0' + ((full_year / 100U) % 10U));
    p_buf[offset + 2] = (char)('0' + ((full_year / 10U) % 10U));
    p_buf[offset + 3] = (char)('0' + (full_year % 10U));
    return offset + 4U;
}

/**
 * @brief Write a uint32_t as a decimal string (variable width) into buf at offset.
 *
 * @param[out] p_buf    Output buffer.
 * @param[in]  buf_size Total size of p_buf.
 * @param[in]  offset   Starting byte offset.
 * @param[in]  value    Value to format.
 *
 * @return New offset after written digits, or offset if insufficient space.
 */
static uint32_t rm_rlf_write_uint(char * p_buf, uint32_t buf_size, uint32_t offset, uint32_t value)
{
    /* Count digits */
    uint32_t tmp     = value;
    uint32_t ndigits = 1U;
    while (tmp >= 10U)
    {
        tmp /= 10U;
        ndigits++;
    }

    if (offset + ndigits >= buf_size)
    {
        return offset; /* insufficient space */
    }

    /* Write digits right-to-left */
    uint32_t pos = offset + ndigits;
    tmp          = value;
    while (pos > offset)
    {
        pos--;
        p_buf[pos] = (char)('0' + (tmp % 10U));
        tmp        /= 10U;
    }

    return offset + ndigits;
}

/**
 * @brief Write a fixed-length string from p_src into p_buf at offset.
 *
 * @param[out] p_buf    Output buffer.
 * @param[in]  offset   Starting byte offset.
 * @param[in]  p_src    Source string (need not be NUL-terminated).
 * @param[in]  length   Number of bytes to copy.
 *
 * @return New offset (offset + length).
 */
static uint32_t rm_rlf_write_str(char * p_buf, uint32_t offset, const char * p_src, uint32_t length)
{
    for (uint32_t i = 0U; i < length; i++)
    {
        p_buf[offset + i] = p_src[i];
    }
    return offset + length;
}

/***********************************************************************************************************************
 * Public function implementations
 **********************************************************************************************************************/

/**
 * @brief Format an rtc_time_t timestamp into a caller-supplied ASCII buffer.
 *
 * @param[in]  p_time      Pointer to RTC time structure to format.
 * @param[in]  format      Timestamp format selection.
 * @param[in]  ts_brackets If true, wrap the timestamp in '[' and ']'.
 * @param[out] p_buf       Caller-supplied output buffer.
 * @param[in]  buf_size    Size of p_buf in bytes.
 *
 * @return Number of characters written (not including NUL), or 0 on error.
 */
uint32_t rs_rtc_logger_format_timestamp(rtc_time_t              const * p_time,
                                         rs_rtc_logger_ts_format_t       format,
                                         bool                            ts_brackets,
                                         char                          * p_buf,
                                         uint32_t                        buf_size)
{
    if ((NULL == p_time) || (NULL == p_buf) || (buf_size < 2U))
    {
        return 0U;
    }

    uint32_t offset = 0U;

    /* --- Opening bracket ------------------------------------------------- */
    if (ts_brackets && (offset < buf_size - 1U))
    {
        p_buf[offset++] = '[';
    }

    /* --- Timestamp body --------------------------------------------------- */
    /* N-8 fix: record offset after the opening bracket so we can detect the
     * "buffer too small" case after the switch.  If no body was written
     * (offset did not advance), fall through to return 0 rather than emitting
     * a malformed truncated string such as "[]" that passes the non-zero
     * length check in rs_rtc_logger_format_line(). */
    uint32_t offset_body_start = offset;

    switch (format)
    {
        case RS_RTC_LOGGER_TS_FORMAT_ISO8601:
        {
            /* [YYYY-MM-DD HH:MM:SS] — requires 22 chars including brackets */
            if (offset + 19U >= buf_size)
            {
                break;
            }
            offset = rm_rlf_write_year(p_buf, offset, (uint32_t)p_time->tm_year);
            p_buf[offset++] = '-';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)(p_time->tm_mon + 1));
            p_buf[offset++] = '-';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_mday);
            p_buf[offset++] = ' ';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_hour);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_min);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_sec);
            break;
        }

        case RS_RTC_LOGGER_TS_FORMAT_TIME_ONLY:
        {
            /* [HH:MM:SS] — requires 8 chars excluding brackets */
            if (offset + 8U >= buf_size)
            {
                break;
            }
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_hour);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_min);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_sec);
            break;
        }

        case RS_RTC_LOGGER_TS_FORMAT_COMPACT:
        {
            /* [YYYYMMDD-HHMMSS] — requires 15 chars excluding brackets */
            if (offset + 15U >= buf_size)
            {
                break;
            }
            offset = rm_rlf_write_year(p_buf, offset, (uint32_t)p_time->tm_year);
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)(p_time->tm_mon + 1));
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_mday);
            p_buf[offset++] = '-';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_hour);
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_min);
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_sec);
            break;
        }

        case RS_RTC_LOGGER_TS_FORMAT_SYSLOG:
        {
            /* [Mon DD HH:MM:SS] — requires 15 chars excluding brackets */
            if (offset + 15U >= buf_size)
            {
                break;
            }
            uint32_t mon = (uint32_t)p_time->tm_mon;
            if (mon > 11U)
            {
                mon = 0U; /* clamp to valid range */
            }
            offset = rm_rlf_write_str(p_buf, offset, rm_rlf_month_abbr[mon], 3U);
            p_buf[offset++] = ' ';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_mday);
            p_buf[offset++] = ' ';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_hour);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_min);
            p_buf[offset++] = ':';
            offset = rm_rlf_write_u2(p_buf, offset, (uint32_t)p_time->tm_sec);
            break;
        }

        default:
            /* Unknown format — return 0 */
            p_buf[0] = '\0';
            return 0U;
    }

    /* N-8 fix: if offset didn't advance past where the body should start, the
     * buffer was too small for the chosen format.  Return 0 so callers that
     * check for a zero return (e.g. rs_rtc_logger_format_line → Write →
     * FSP_ERR_INVALID_SIZE) correctly reject the output rather than
     * transmitting a malformed "[]" or "[Mon DD]" fragment. */
    if (offset == offset_body_start)
    {
        p_buf[0] = '\0';
        return 0U;
    }

    /* --- Closing bracket ------------------------------------------------- */
    if (ts_brackets && (offset < buf_size - 1U))
    {
        p_buf[offset++] = ']';
    }

    /* Always NUL-terminate */
    if (offset < buf_size)
    {
        p_buf[offset] = '\0';
    }
    else
    {
        p_buf[buf_size - 1U] = '\0';
        offset = buf_size - 1U;
    }

    return offset;
}

/**
 * @brief Append the line terminator string to a caller-supplied buffer.
 *
 * @param[in]  term      Line terminator selection.
 * @param[out] p_buf     Caller-supplied buffer.
 * @param[in]  offset    Byte offset into p_buf at which to begin writing.
 * @param[in]  buf_size  Total size of p_buf in bytes.
 *
 * @return Number of terminator characters written (1 or 2), or 0 if insufficient space.
 */
static uint32_t rs_rtc_logger_format_terminator(rs_rtc_logger_term_t   term,
                                          char                 * p_buf,
                                          uint32_t               offset,
                                          uint32_t               buf_size)
{
    if ((NULL == p_buf) || (offset >= buf_size))
    {
        return 0U;
    }

    uint32_t written = 0U;

    switch (term)
    {
        case RS_RTC_LOGGER_TERM_CRLF:
            if (offset + 2U < buf_size)
            {
                p_buf[offset]     = '\r';
                p_buf[offset + 1] = '\n';
                p_buf[offset + 2] = '\0';
                written           = 2U;
            }
            break;

        case RS_RTC_LOGGER_TERM_LF:
            if (offset + 1U < buf_size)
            {
                p_buf[offset]     = '\n';
                p_buf[offset + 1] = '\0';
                written           = 1U;
            }
            break;

        case RS_RTC_LOGGER_TERM_CR:
            if (offset + 1U < buf_size)
            {
                p_buf[offset]     = '\r';
                p_buf[offset + 1] = '\0';
                written           = 1U;
            }
            break;

        default:
            break;
    }

    return written;
}

/**
 * @brief Compose a complete formatted log line from a buffer entry.
 *
 * @param[in]  p_entry     Pointer to circular buffer entry to format.
 * @param[in]  format      Timestamp format selection.
 * @param[in]  ts_brackets If true, wrap timestamp in '[' and ']'.
 * @param[in]  term        Line terminator selection.
 * @param[out] p_buf       Caller-supplied output buffer.
 * @param[in]  buf_size    Size of p_buf in bytes.
 *
 * @return Total number of characters written (not including NUL), or 0 on error.
 */
uint32_t rs_rtc_logger_format_line(rs_rtc_logger_event_entry_t const * p_entry,
                                    rs_rtc_logger_ts_format_t           format,
                                    bool                                ts_brackets,
                                    rs_rtc_logger_term_t                term,
                                    char                              * p_buf,
                                    uint32_t                            buf_size)
{
    if ((NULL == p_entry) || (NULL == p_buf) || (buf_size < 4U))
    {
        return 0U;
    }

    uint32_t offset = 0U;

    /* 1. Format timestamp — or substitute a null marker if the RTC had not
     *    been set when this event was triggered (cold start, pre-TimeSet()).
     *    Stale VBATT-retained register values are never shown to the user. */
    if (!p_entry->time_valid)
    {
        /* Null timestamp: [0000-00-00 00:00:00] — respects ts_brackets.
         * The date/time digits are fixed zeros so it is unambiguously
         * "not set" regardless of what the RTC hardware registers held. */
        if (ts_brackets)
        {
            static const char null_ts[] = "[0000-00-00 00:00:00]";
            offset = rm_rlf_write_str(p_buf, offset, null_ts, sizeof(null_ts) - 1U);
        }
        else
        {
            static const char null_ts[] = "0000-00-00 00:00:00";
            offset = rm_rlf_write_str(p_buf, offset, null_ts, sizeof(null_ts) - 1U);
        }
    }
    else
    {
        uint32_t ts_len = rs_rtc_logger_format_timestamp(&p_entry->timestamp,
                                                          format,
                                                          ts_brackets,
                                                          p_buf,
                                                          buf_size);
        if (0U == ts_len)
        {
            return 0U;
        }
        offset = ts_len;
    }

    /* 2. Space separator */
    if (offset + 1U >= buf_size)
    {
        return 0U;
    }
    p_buf[offset++] = ' ';

    /* 3. Event payload */
    uint32_t payload_len = p_entry->event_data_len;
    if (payload_len > RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U)
    {
        payload_len = RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN - 1U;
    }

    if (payload_len > 0U)
    {
        if (offset + payload_len >= buf_size)
        {
            return 0U;
        }
        offset = rm_rlf_write_str(p_buf, offset,
                                   (const char *)p_entry->event_data,
                                   payload_len);
    }

    /* 4. " #<seq>" suffix */
    if (offset + 3U >= buf_size)
    {
        return 0U;
    }
    p_buf[offset++] = ' ';
    p_buf[offset++] = '#';
    offset = rm_rlf_write_uint(p_buf, buf_size, offset, p_entry->seq_count);

    /* 5. Line terminator */
    uint32_t term_written = rs_rtc_logger_format_terminator(term, p_buf, offset, buf_size);
    offset               += term_written;

    return offset;
}
