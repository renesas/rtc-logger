/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_rtc_logger_test_types.h
 * @brief Host-build type stubs for the RTC Event Logger unit tests.
 *
 * Replaces bsp_api.h, r_rtc_api.h, r_uart_api.h, and FreeRTOS headers
 * when RS_RTC_LOGGER_TEST_HOST_BUILD is defined.  Provides only the
 * minimum typedefs needed to compile rs_rtc_logger_format.c and the
 * unit-under-test files on a host (x86/x64) without any embedded BSP.
 *
 * Do NOT include FSP headers here — this file is intentionally self-contained.
 **********************************************************************************************************************/

#ifndef RS_RTC_LOGGER_TEST_TYPES_H
#define RS_RTC_LOGGER_TEST_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ── FSP error codes (mirrors r_fsp_error.h subset) ─────────────────────── */

typedef enum e_fsp_err
{
    FSP_SUCCESS                  = 0,
    FSP_ERR_ASSERTION            = 1,
    FSP_ERR_INVALID_ARGUMENT     = 3,
    FSP_ERR_NOT_OPEN             = 7,
    FSP_ERR_OUT_OF_MEMORY        = 9,
    FSP_ERR_ALREADY_OPEN         = 14,
    FSP_ERR_BUFFER_EMPTY         = 36,
    FSP_ERR_NOT_INITIALIZED      = 10,
    FSP_ERR_INVALID_MODE         = 11,
    FSP_ERR_OVERFLOW             = 22,
    FSP_ERR_INVALID_HW_CONDITION = 24,
    FSP_ERR_INVALID_SIZE         = 31,
} fsp_err_t;

/* ── RTC time structure (mirrors r_rtc_api.h rtc_time_t) ────────────────── */

typedef struct st_rtc_time
{
    uint32_t tm_sec;    ///< Seconds  [0, 59]
    uint32_t tm_min;    ///< Minutes  [0, 59]
    uint32_t tm_hour;   ///< Hours    [0, 23]
    uint32_t tm_mday;   ///< Day      [1, 31]
    uint32_t tm_mon;    ///< Month    [0, 11]  (0 = January)
    uint32_t tm_year;   ///< Year since 1900 (e.g. 124 → 2024)
    uint32_t tm_wday;   ///< Weekday  [0, 6]   (0 = Sunday)
} rtc_time_t;

/* ── Minimal UART callback args (not used in format tests) ──────────────── */

typedef struct st_uart_callback_args
{
    uint32_t    event;
    uint8_t     data;
    void      * p_context;
} uart_callback_args_t;

/* ── Opaque HAL instance stubs ───────────────────────────────────────────
 * Declared as minimal structs (not void) so that test code can zero-
 * initialise dummy instances with {0} and take their address.
 * Open() only checks NULL != p_rtc / NULL != p_uart; it never dereferences
 * the internal fields on the host build (all HAL calls are mocked). */

typedef struct { int _unused; } rtc_instance_t;
typedef struct { int _unused; } uart_instance_t;

/* ── FSP_HEADER / FSP_FOOTER macros (no-ops on host) ───────────────────── */

#ifndef FSP_HEADER
  #define FSP_HEADER
#endif
#ifndef FSP_FOOTER
  #define FSP_FOOTER
#endif

#endif /* RS_RTC_LOGGER_TEST_TYPES_H */
