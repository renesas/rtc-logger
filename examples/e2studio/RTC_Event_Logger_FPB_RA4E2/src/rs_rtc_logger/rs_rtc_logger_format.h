/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_format.h
 * @brief Internal timestamp formatting engine for the RTC Event Logger.
 *
 * This header is an internal RTC Event Logger interface — it is not part of the public API.
 * It declares the pure formatting function that converts rtc_time_t structures
 * to ASCII timestamp strings. No I/O, no side effects. Integer arithmetic only
 * (FR-014). Fully unit-testable in host isolation.
 */

#ifndef RS_RTC_LOGGER_FORMAT_H
#define RS_RTC_LOGGER_FORMAT_H

#include <rs_rtc_logger/rs_rtc_logger_types.h>

/***********************************************************************************************************************
 * Internal function prototypes
 **********************************************************************************************************************/

/**
 * @brief Format an rtc_time_t timestamp into a caller-supplied ASCII buffer.
 *
 * Converts the given RTC time structure into a human-readable ASCII string
 * according to the specified format and bracket options. The output is always
 * NUL-terminated. No heap allocation. Integer arithmetic only (FR-014).
 *
 * Supported formats:
 *   RS_RTC_LOGGER_TS_FORMAT_ISO8601   -> "[YYYY-MM-DD HH:MM:SS]" (22 chars + NUL)
 *   RS_RTC_LOGGER_TS_FORMAT_TIME_ONLY -> "[HH:MM:SS]"            (10 chars + NUL)
 *   RS_RTC_LOGGER_TS_FORMAT_COMPACT   -> "[YYYYMMDD-HHMMSS]"     (17 chars + NUL)
 *   RS_RTC_LOGGER_TS_FORMAT_SYSLOG    -> "[Mon DD HH:MM:SS]"     (17 chars + NUL)
 *
 * When ts_brackets is false, the leading '[' and trailing ']' are omitted.
 *
 * @param[in]  p_time      Pointer to RTC time structure to format.
 * @param[in]  format      Timestamp format selection.
 * @param[in]  ts_brackets If true, wrap the timestamp in '[' and ']'.
 * @param[out] p_buf       Caller-supplied output buffer.
 * @param[in]  buf_size    Size of p_buf in bytes (must be >= RS_RTC_LOGGER_CFG_TS_BUF_SIZE).
 *
 * @return Number of characters written (not including NUL), or 0 on error
 *         (e.g. buf_size too small or invalid format).
 */
uint32_t rs_rtc_logger_format_timestamp(rtc_time_t              const * p_time,
                                         rs_rtc_logger_ts_format_t       format,
                                         bool                            ts_brackets,
                                         char                          * p_buf,
                                         uint32_t                        buf_size);

/**
 * @brief Compose a complete formatted log line from a buffer entry.
 *
 * Combines timestamp, event payload, sequence number, and line terminator
 * into a single NUL-terminated ASCII line in p_buf, ready for UART TX.
 *
 * Output format (ISO8601, brackets, CRLF):
 *   "[YYYY-MM-DD HH:MM:SS] Event text #<seq>\r\n"
 *
 * @param[in]  p_entry    Pointer to circular buffer entry to format.
 * @param[in]  format     Timestamp format selection.
 * @param[in]  ts_brackets If true, wrap timestamp in '[' and ']'.
 * @param[in]  term       Line terminator selection.
 * @param[out] p_buf      Caller-supplied output buffer.
 * @param[in]  buf_size   Size of p_buf in bytes.
 *
 * @return Total number of characters written (not including NUL), or 0 on error.
 */
uint32_t rs_rtc_logger_format_line(rs_rtc_logger_event_entry_t const * p_entry,
                                    rs_rtc_logger_ts_format_t           format,
                                    bool                                ts_brackets,
                                    rs_rtc_logger_term_t                term,
                                    char                              * p_buf,
                                    uint32_t                            buf_size);

#endif /* RS_RTC_LOGGER_FORMAT_H */
