/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_types.h
 * @brief Public type definitions and enumerations for the RTC Event Logger PES.
 *
 * This header is included by rs_rtc_logger.h and may be included directly
 * by application code that needs to reference PES types without including
 * the full public interface header.
 */

#ifndef RS_RTC_LOGGER_TYPES_H
#define RS_RTC_LOGGER_TYPES_H

#include <rs_rtc_logger/rs_rtc_logger_cfg.h>

/* Pull in FSP types when building on-target; stub types defined in test shim
 * for host-based unit tests (RS_RTC_LOGGER_TEST_HOST_BUILD). */
#ifndef RS_RTC_LOGGER_TEST_HOST_BUILD
 #include "bsp_api.h"
 #include "r_rtc_api.h"
 #include "r_uart_api.h"
 #if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
  #include "FreeRTOS.h"
  #include "task.h"
 #endif
#else
 #include "rs_rtc_logger_test_types.h"  /* host-build type stubs */
#endif

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/

/** PES lifecycle states — mirrors the standard PES Runtime-Control lifecycle model */
typedef enum
{
    RS_RTC_LOGGER_STATE_CLOSED  = 0,   ///< Resources not allocated
    RS_RTC_LOGGER_STATE_OPEN    = 1,   ///< Initialized; UART output path not yet active
    RS_RTC_LOGGER_STATE_STARTED = 2,   ///< UART output path active; EventTrigger() and Write() accepted
    RS_RTC_LOGGER_STATE_STOPPED = 3,   ///< Output suspended; configuration and buffer preserved
} rs_rtc_logger_state_t;

/** Selects the ASCII representation of the RTC timestamp */
typedef enum
{
    /** [YYYY-MM-DD HH:MM:SS]  e.g. [2024-07-15 14:30:05]  (default, ISO 8601-style) */
    RS_RTC_LOGGER_TS_FORMAT_ISO8601    = 0,

    /** [HH:MM:SS]  e.g. [14:30:05]  — date suppressed */
    RS_RTC_LOGGER_TS_FORMAT_TIME_ONLY  = 1,

    /** [YYYYMMDD-HHMMSS]  e.g. [20240715-143005]  — compact, no separators */
    RS_RTC_LOGGER_TS_FORMAT_COMPACT    = 2,

    /** [Mon DD HH:MM:SS]  e.g. [Jul 15 14:30:05]  — syslog style */
    RS_RTC_LOGGER_TS_FORMAT_SYSLOG     = 3,
} rs_rtc_logger_ts_format_t;

/** Selects the line terminator appended to each output line */
typedef enum
{
    RS_RTC_LOGGER_TERM_CRLF = 0,       ///< \r\n  (default)
    RS_RTC_LOGGER_TERM_LF   = 1,       ///< \n    (Unix-style)
    RS_RTC_LOGGER_TERM_CR   = 2,       ///< \r    (legacy)
} rs_rtc_logger_term_t;

/** Circular buffer overflow policy */
typedef enum
{
    RS_RTC_LOGGER_OVERFLOW_OVERWRITE_OLDEST = 0, ///< Overwrite oldest entry (default — preserves most recent events)
    RS_RTC_LOGGER_OVERFLOW_DROP_NEWEST      = 1, ///< Silently drop new event — preserves event ordering
} rs_rtc_logger_overflow_t;

/** Callback event identifiers */
typedef enum
{
    RS_RTC_LOGGER_EVENT_TX_COMPLETE    = 0,  ///< VCOM/UART write completed successfully
    RS_RTC_LOGGER_EVENT_TX_ERROR       = 1,  ///< UART transmission error
    RS_RTC_LOGGER_EVENT_BUFFER_FULL    = 2,  ///< Circular event buffer overflowed
} rs_rtc_logger_event_t;

/***********************************************************************************************************************
 * Structures
 **********************************************************************************************************************/

/** Arguments delivered to the optional application callback */
typedef struct
{
    rs_rtc_logger_event_t   event;       ///< Event that triggered the callback
    fsp_err_t               err;         ///< FSP error code (FSP_SUCCESS on TX_COMPLETE)
    uint32_t                seq_count;   ///< Sequence count of the event that triggered callback
    void                  * p_context;   ///< User-supplied context pointer from cfg
} rs_rtc_logger_callback_args_t;

/**
 * @brief Single entry in the circular event buffer.
 *
 * Timestamp is captured at ISR time inside rs_rtc_logger_EventTrigger().
 * event_data is a fixed-size inline copy — no pointer aliasing, ISR-safe.
 */
typedef struct
{
    rtc_time_t  timestamp;                                   ///< RTC time captured at event
    uint32_t    seq_count;                                   ///< Incremental sequence number
    uint8_t     event_data[RS_RTC_LOGGER_CFG_EVENT_DATA_MAX_LEN]; ///< Inline event payload (NUL-terminated)
    uint32_t    event_data_len;                              ///< Actual payload length in bytes
    bool        time_valid;                                  ///< false = time not yet set when event triggered; format as [0000-00-00 00:00:00]
} rs_rtc_logger_event_entry_t;

/**
 * @brief PES instance configuration — populated by FSP tooling from
 *        property selections in the IDE configurator.
 *
 * Must remain valid (const-correct, static storage) for the full
 * lifetime of the PES instance (until Close() is called).
 */
typedef struct
{
    /** HAL instance pointers — bound at configuration time */
    rtc_instance_t  const * p_rtc;              ///< RTC HAL instance (r_rtc)
    uart_instance_t const * p_uart;             ///< UART HAL instance (r_sci_b_uart)

    /** Timestamp formatting options */
    rs_rtc_logger_ts_format_t   ts_format;      ///< Timestamp format (default: ISO8601).
                                                 ///< @note N-10: Effective only at Open() time.
                                                 ///< To change the format at runtime after Open(),
                                                 ///< call rs_rtc_logger_TimestampFormatSet() —
                                                 ///< Write() reads from the ctrl shadow field,
                                                 ///< not directly from this cfg field.
    bool                        ts_brackets;    ///< Wrap timestamp in [ ] (default: true)

    /** Output formatting */
    rs_rtc_logger_term_t        line_term;      ///< Line terminator (default: CRLF)

    /** Circular event buffer policy */
    rs_rtc_logger_overflow_t    overflow_policy; ///< Buffer overflow behavior (default: OVERWRITE_OLDEST)

    /** Optional application callback (NULL if not used) */
    void (* p_callback)(rs_rtc_logger_callback_args_t * p_args);
    void  * p_context;                          ///< Passed back in callback args
} rs_rtc_logger_cfg_t;

/**
 * @brief PES instance control block.
 *
 * Declare as static in the application. Do not access fields directly —
 * all state transitions occur exclusively through PES public functions.
 *
 * Per FR-095: declared in public header rs_rtc_logger.h (upper layers may inspect).
 * Per FR-096: version and size are first two elements.
 * Per FR-097: both PES-owned DMI instance pointers are recorded here.
 *
 * RAM budget: verified at build time via static_assert against
 * RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES.
 *
 * In Example Project mode (RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1),
 * the internal consumer task TCB and stack are allocated as companion
 * static variables in rs_rtc_logger.c (not inside this structure, to
 * avoid inflating the public type with FreeRTOS-specific fields). The
 * task handle is stored in the internal_task_handle field below.
 */
typedef struct
{
    /* --- Versioning (FR-096 — must remain first two fields) -------------- */
    uint32_t    version;                        ///< PES module version (encoded MAJOR.MINOR.PATCH)
    uint32_t    ctrl_size;                      ///< sizeof(rs_rtc_logger_ctrl_t) for compatibility checking

    /* --- Lifecycle state ------------------------------------------------- */
    rs_rtc_logger_state_t   state;              ///< Current lifecycle state

    /* --- Bound configuration --------------------------------------------- */
    rs_rtc_logger_cfg_t const * p_cfg;          ///< Pointer to cfg passed at Open()

    /* --- Hot-mutable configuration shadow (avoids const-cast on p_cfg) --- */
    /**
     * Shadow of p_cfg->ts_format, kept here so rs_rtc_logger_TimestampFormatSet()
     * can update it without casting away const from p_cfg.  Initialised from
     * p_cfg->ts_format in Open() and used exclusively by Write() / WriteStr().
     */
    rs_rtc_logger_ts_format_t   ts_format;

    /* --- Circular event buffer (FR-110; static — no heap) ---------------- */
    rs_rtc_logger_event_entry_t event_buf[RS_RTC_LOGGER_CFG_EVENT_BUF_DEPTH];
    volatile uint32_t   buf_head;               ///< Write index (updated by EventTrigger ISR)
    volatile uint32_t   buf_tail;               ///< Read index  (updated by consumer task)
    volatile uint32_t   buf_count;              ///< Entries currently in buffer

    /* --- Sequence counter (FR-008; warm-reset preserved via app) --------- */
    volatile uint32_t   seq_count;              ///< Incremental event sequence counter

    /* --- Output line buffer (static; no heap) ---------------------------- */
    char    line_buf[RS_RTC_LOGGER_CFG_LINE_BUF_SIZE]; ///< Composed output line

    /* --- UART TX completion flag (set by HAL callback) ------------------- */
    volatile bool       tx_complete;            ///< Set by UART TX-complete callback; cleared before each write

    /* --- RTC time validity guard ----------------------------------------- */
    /**
     * Set false on every cold start (power-on reset) until rs_rtc_logger_TimeSet()
     * is called successfully.  Set true on warm reset (RTC retained its value).
     * Stamped into every event entry at enqueue time so that events logged
     * before the user sets the time display [0000-00-00 00:00:00] rather than
     * stale VBATT-retained RTC register values.
     */
    bool                rtc_time_set;

    /* --- Example Project mode internal state (FR-121-FR-123) ------------- */
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
    TaskHandle_t    internal_task_handle;       ///< Handle to PES-owned consumer task (NULL until created)
    volatile bool   auto_init_pending;          ///< True after first EventTrigger() before Open()/Start() complete
    volatile bool   auto_init_done;             ///< True once auto-Open()/Start() sequence has completed
#endif

    /* --- Diagnostics ----------------------------------------------------- */
    uint32_t    events_logged;                  ///< Total events successfully enqueued
    uint32_t    events_dropped;                 ///< Events lost due to buffer overflow
    uint32_t    tx_errors;                      ///< UART transmission errors
    rtc_time_t  last_timestamp;                 ///< Last timestamp captured via EventTrigger()
} rs_rtc_logger_ctrl_t;

/** Returned by rs_rtc_logger_StatusGet() — read-only snapshot */
typedef struct
{
    rs_rtc_logger_state_t   state;           ///< Current lifecycle state
    uint32_t                events_logged;   ///< Total events successfully enqueued
    uint32_t                events_dropped;  ///< Events dropped due to buffer overflow
    uint32_t                tx_errors;       ///< UART transmission errors
    uint32_t                seq_count;       ///< Current event sequence counter value
    uint32_t                buf_count;       ///< Events currently pending in circular buffer
    rtc_time_t              last_timestamp;  ///< Last timestamp captured via EventTrigger()
} rs_rtc_logger_status_t;

#endif /* RS_RTC_LOGGER_TYPES_H */
