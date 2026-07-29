# Functions Guide — Public and Private Functions

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).
> For call sequences and how these functions fit together in an application, see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).

This guide documents every function in the `rs_rtc_logger` — both the **public** functions an application calls directly, and the **private/internal** functions used only inside the module. Each function is explicitly tagged:

| Tag | Meaning |
|---|---|
| 🟢 **PUBLIC** | Declared in `rs_rtc_logger.h` or a board config header (`rs_rtc_logger_data_fpb_<board>.h`). Part of the supported application-facing interface. Naming: `rs_rtc_logger_OperationName()`. |
| 🔒 **INTERNAL** | Declared in an internal header (`rs_rtc_logger_format.h`, `rs_rtc_logger_hal.h`) marked "not part of the public API." Has external (non-`static`) linkage so it can be called from other files inside the module, but is not intended for application use. |
| ⚫ **PRIVATE** | `static` — file-scoped, callable only from within the `.c` file that defines it. Never referenced outside the module. |

Only 🟢 **PUBLIC** functions are safe to call from application code. 🔒 **INTERNAL** and ⚫ **PRIVATE** functions are documented here for engineers reading, debugging, or extending the module's source — calling them directly from application code voids the safety guarantees (state gating, ISR-safety, hot-safe semantics) described for the public functions.

---

## Table of Contents

1. [Start Here: The Entry Point](#1-start-here-the-entry-point)
2. [Configuration Bridge (call before Open)](#2-configuration-bridge-call-before-open)
3. [Lifecycle Functions](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode)
4. [Operational Functions](#4-operational-functions)
5. [Configuration and Query Functions](#5-configuration-and-query-functions)
6. [Example Project Mode Setup Helper](#6-example-project-mode-setup-helper)
7. [Complete Public Function Reference Table](#7-complete-public-function-reference-table)
8. [Internal Functions — `rs_rtc_logger.c`](#8-internal-functions--rs_rtc_loggerc)
9. [Internal Functions — `rs_rtc_logger_format.c` / `.h`](#9-internal-functions--rs_rtc_logger_formatc--h)
10. [Internal Functions — `rs_rtc_logger_hal.c` / `.h`](#10-internal-functions--rs_rtc_logger_halc--h)
11. [Internal Functions — `rs_rtc_logger_stdio_console.c` / `.h`](#11-internal-functions--rs_rtc_logger_stdio_consolec--h)
12. [Complete Internal Function Reference Table](#12-complete-internal-function-reference-table)

---

# Public Functions

> **What "hot-safe" means:** A function marked hot-safe can be called while the module is in the STARTED state (actively transmitting) without disrupting the current transmission. The change takes effect on the next `Write()` call. Functions that are not hot-safe must only be called before the module is started or after it is stopped.

## 1. Start Here: The Entry Point

```c
// ISR-safe — the only function the application ISR needs to call:
rs_rtc_logger_EventTrigger(&g_logger_ctrl, (uint8_t const *)"Event Detected", 14U);
```

In **Example Project mode**, this is the only function the application needs to call after initial setup. In **Expert mode**, the application also manages the lifecycle functions described below.

---

## 2. Configuration Bridge (call before Open)

```c
// Bind the RTC HAL instance — call once before rs_rtc_logger_Open()
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
```

`rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, and `rs_rtc_logger_ConfigGetSize()` are declared in the **board-specific preset configuration header** (`rs_rtc_logger_data_fpb_ra4e2.h` / `rs_rtc_logger_data_fpb_ra6e2.h`), alongside the `*_preset` instance data they are most commonly paired with. They are declared and defined **unconditionally** (regardless of `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE`), so the same three functions remain available whether the caller binds the `*_preset` RTC instance or the FSP Stack Configurator-generated `g_rtc0`.

>  Boards are using a Buffered I/O UART driver (FPB-RA4E2 and FPB-RA6E2 — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)), this component never opens, writes, or validates a `uart_instance_t` at all. `rs_rtc_logger_ConfigGen()` takes **only** an RTC instance parameter. The shared UART is owned and independently validated by the vendored Buffered I/O UART driver.

### 🟢 `rs_rtc_logger_ConfigGen()`

Assigns the RTC HAL instance pointer to the RTC Event Logger configuration structure. Called once during application initialization, before `rs_rtc_logger_Open()`.

```c
fsp_err_t rs_rtc_logger_ConfigGen(rs_rtc_logger_cfg_t   * p_cfg,
                                   rtc_instance_t  const * p_rtc);
```

`p_rtc` accepts either the pre-set `g_rtc0_preset` (when `RS_RTC_LOGGER_CFG_PRESET_CONFIG_USE == 1`) or the FSP Stack Configurator-generated `g_rtc0` (when `== 0`). Non-instance fields (`ts_format`, `ts_brackets`, `line_term`, `overflow_policy`, `p_callback`, `p_context`) must already be set in `p_cfg` by the caller; this function does not modify them.

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Instance pointer assigned |
| `FSP_ERR_ASSERTION` | `p_cfg` or `p_rtc` is NULL |

*Valid in any RTC Event Logger lifecycle state. Not ISR-safe.*

### 🟢 `rs_rtc_logger_ConfigCheck()`

Validates a configuration structure before `rs_rtc_logger_Open()`. Verifies that the required RTC HAL instance (and its configuration block) is present, that the RTC carry interrupt priority satisfies the constraint documented in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings), and that the enumerated configuration fields (`ts_format`, `line_term`, `overflow_policy`) are in range.

```c
fsp_err_t rs_rtc_logger_ConfigCheck(rs_rtc_logger_cfg_t const * p_cfg);
```

Call any time after `rs_rtc_logger_ConfigGen()` and before `rs_rtc_logger_Open()`, so misconfiguration is caught early. This function validates the **RTC instance only** — UART configuration is validated independently by the Buffered I/O UART driver's own `RS_STDIO_UART_CheckApplicationConfiguration()` (wired in through `rs_rtc_logger_console_init()`, called from `rs_rtc_logger_hal_uart_open()` during `Open()` — see [Section 10](#10-internal-functions--rs_rtc_logger_halc--h)).

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Configuration is valid |
| `FSP_ERR_ASSERTION` | `p_cfg` is NULL |
| `FSP_ERR_NOT_INITIALIZED` | Required RTC instance/config is missing |
| `FSP_ERR_INVALID_HW_CONDITION` | RTC `carry_ipl`/`carry_irq` is disabled or has no NVIC vector allocated |
| `FSP_ERR_INVALID_ARGUMENT` | `ts_format`, `line_term`, or `overflow_policy` is out of range |

*Valid in any lifecycle state. Not ISR-safe.*

### 🟢 `rs_rtc_logger_ConfigGetSize()`

Returns the size, in bytes, of the RTC Event Logger control block structure. Useful for confirming control-block sizing (e.g. against `RS_RTC_LOGGER_CFG_CTRL_RAM_LIMIT_BYTES`) without requiring `rs_rtc_logger_Open()` to have been called first.

```c
fsp_err_t rs_rtc_logger_ConfigGetSize(rs_rtc_logger_ctrl_t const * p_ctrl,
                                       size_t                      * p_size);
```

| Return Value | Meaning |
|---|---|
| `FSP_SUCCESS` | Size returned in `*p_size` |
| `FSP_ERR_ASSERTION` | `p_ctrl` or `p_size` is NULL |

*Valid in any lifecycle state. Not ISR-safe. `p_ctrl` may point to caller-allocated storage that has not yet been opened — it is only used for `sizeof(*p_ctrl)`.*

Example combining all three:

```c
#include <rs_rtc_logger/config/rs_rtc_logger_data_fpb_ra4e2.h>
...
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
fsp_err_t err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
if (FSP_SUCCESS != err) { /* handle bad configuration */ }
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);
```

---

## 3. Lifecycle Functions (Expert Mode / also used internally in Example Project Mode)

| Function | Valid States | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_Open(p_ctrl, p_cfg)` | CLOSED | Initialize; open RTC + UART HAL drivers in order |
| 🟢 `rs_rtc_logger_Start(p_ctrl)` | OPEN, STOPPED | Enable UART output path; activate consumer task |
| 🟢 `rs_rtc_logger_Stop(p_ctrl)` | STARTED | Suspend TX; buffer continues accepting events |
| 🟢 `rs_rtc_logger_Close(p_ctrl)` | OPEN, STOPPED | Release all resources; close UART then RTC |

---

## 4. Operational Functions

| Function | Valid States | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_EventTrigger(p_ctrl, p_data, length)` | STARTED, STOPPED (Example Project: also CLOSED on first call) | ISR-safe capture; enqueues to circular buffer |
| 🟢 `rs_rtc_logger_EventDequeue(p_ctrl, p_entry_out)` | OPEN, STARTED, STOPPED | Dequeue oldest buffer entry into caller's struct (Expert mode consumer task; safe canonical path) |
| 🟢 `rs_rtc_logger_Write(p_ctrl, p_entry)` | STARTED | Format and transmit one buffer entry (called internally in Example Project mode) |
| 🟢 `rs_rtc_logger_WriteStr(p_ctrl, p_str)` | STARTED | Convenience: write a string with live RTC timestamp, bypassing the circular buffer |

---

## 5. Configuration and Query Functions

| Function | Hot-Safe | Description |
|---|---|---|
| 🟢 `rs_rtc_logger_TimestampFormatSet(p_ctrl, format)` | Yes | Switch timestamp format at runtime |
| 🟢 `rs_rtc_logger_TimeSet(p_ctrl, p_time)` | Yes | Set RTC calendar date and time |
| 🟢 `rs_rtc_logger_ClockValidate(p_ctrl, time_is_valid)` | Yes | Mark RTC time as valid or invalid (used on warm reset) |
| 🟢 `rs_rtc_logger_LocoCalibrationSet(p_ctrl, rfrl_value)` | Yes | Adjust LOCO oscillator drift via RFRL register (valid range 8–510) |
| 🟢 `rs_rtc_logger_SeqCountSet(p_ctrl, seq_count)` | No | Restore sequence counter after warm reset (OPEN or STOPPED only) |
| 🟢 `rs_rtc_logger_StatusGet(p_ctrl, p_status)` | — | Snapshot of state, counters, and last timestamp |
| 🟢 `rs_rtc_logger_TimeGet(p_ctrl, p_time)` | — | Read current RTC calendar time |
| 🟢 `rs_rtc_logger_VersionGet(p_version)` | — | Module version (valid in any state including CLOSED) |

---

## 6. Example Project Mode Setup Helper

```c
// Call from task context BEFORE enabling the external IRQ:
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);
```

🟢 This creates the internal static FreeRTOS consumer task. It must be called from task context (not ISR context) because `xTaskCreateStatic()` must not be called from within an ISR. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

---

## 7. Complete Public Function Reference Table

| Function | Category | State Gating | See |
|---|---|---|---|
| `rs_rtc_logger_ConfigGen()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_ConfigCheck()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_ConfigGetSize()` | Configuration bridge | Any state | [Section 2](#2-configuration-bridge-call-before-open) |
| `rs_rtc_logger_Open()` | Lifecycle | CLOSED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Start()` | Lifecycle | OPEN, STOPPED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Stop()` | Lifecycle | STARTED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_Close()` | Lifecycle | OPEN, STOPPED | [Section 3](#3-lifecycle-functions-expert-mode--also-used-internally-in-example-project-mode) |
| `rs_rtc_logger_EventTrigger()` | Operational | STARTED, STOPPED (+CLOSED in EP mode) | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_EventDequeue()` | Operational | OPEN, STARTED, STOPPED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_Write()` | Operational | STARTED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_WriteStr()` | Operational | STARTED | [Section 4](#4-operational-functions) |
| `rs_rtc_logger_TimestampFormatSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_TimeSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_ClockValidate()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_LocoCalibrationSet()` | Config/Query (hot-safe) | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_SeqCountSet()` | Config/Query | OPEN, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_StatusGet()` | Config/Query | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_TimeGet()` | Config/Query | OPEN, STARTED, STOPPED | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_VersionGet()` | Config/Query | Any state | [Section 5](#5-configuration-and-query-functions) |
| `rs_rtc_logger_EP_PrepareConsumerTask()` | Setup helper | Any state, task context only | [Section 6](#6-example-project-mode-setup-helper) |

For the full state-gating matrix (which states return which error codes), see [ARCHITECTURE.md](ARCHITECTURE.md#2-public-function-state-gating).

---

# Internal Functions

The functions below are **not part of the supported application interface**. They are documented so that engineers debugging, reviewing, or extending `rs_rtc_logger` can understand the internal call graph without re-deriving it from source. Signatures may change between releases without notice, unlike the public functions above.

## 8. Internal Functions — `rs_rtc_logger.c`

All five are ⚫ **PRIVATE** (`static`, file-scoped).

### ⚫ `rs_rl_is_open()`

```c
static bool rs_rl_is_open(rs_rtc_logger_ctrl_t const * p_ctrl);
```

Returns `true` if the RTC Event Logger is in any non-CLOSED state (OPEN, STARTED, or STOPPED); `false` if CLOSED or `p_ctrl` is NULL. Used internally by the public lifecycle functions to check state before transitioning.

### ⚫ `rs_rl_enqueue()`

```c
static fsp_err_t rs_rl_enqueue(rs_rtc_logger_ctrl_t * p_ctrl,
                                rtc_time_t     const * p_timestamp,
                                uint8_t        const * p_data,
                                uint32_t               length);
```

Enqueues a timestamped event entry into the circular buffer. ISR-safe — uses a critical section (`__disable_irq()`/`__enable_irq()`, or the FreeRTOS-from-ISR equivalent) to atomically update buffer indices and count, and applies the configured overflow policy (`OVERWRITE_OLDEST` or `DROP_NEWEST`). This is the function that actually implements the buffering behavior the public `rs_rtc_logger_EventTrigger()` exposes.

Returns `FSP_SUCCESS` on enqueue, or `FSP_ERR_OVERFLOW` if the buffer is full and the policy is `DROP_NEWEST`.

### ⚫ `rs_rl_dequeue()`

```c
static bool rs_rl_dequeue(rs_rtc_logger_ctrl_t        * p_ctrl,
                           rs_rtc_logger_event_entry_t * p_entry_out);
```

Dequeues the oldest entry from the circular buffer. Called from task context only (not ISR-safe by design — no ISR-side dequeue path exists). Updates `buf_tail` and `buf_count`. Compiled in for **both** Example Project mode and Expert mode so the public `rs_rtc_logger_EventDequeue()` wrapper can call it in either configuration. Returns `true` if an entry was dequeued, `false` if the buffer was empty.

### ⚫ `rs_rl_auto_init()`

```c
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static fsp_err_t rs_rl_auto_init(rs_rtc_logger_ctrl_t * p_ctrl);
#endif
```

Performs the deferred auto-`Open()`/`Start()` sequence in Example Project mode. Called from the internal consumer task after it detects `auto_init_pending`. Guarantees the first `EventTrigger()` event (already enqueued before `Open()`/`Start()` ran) is not lost. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

### ⚫ `rs_rtc_logger_internal_consumer_task()`

```c
#if RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE
static void rs_rtc_logger_internal_consumer_task(void * p_arg);
#endif
```

The RTC Event Logger-owned FreeRTOS task body used in Example Project mode. Waits for task notifications from `EventTrigger()` (or the UART TX-complete callback), then drains the circular buffer by calling the public `rs_rtc_logger_Write()` for each pending entry, guarded by a TX timeout (`RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS`). This is the task created by the public `rs_rtc_logger_EP_PrepareConsumerTask()`. Only compiled in when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1`.

---

## 9. Internal Functions — `rs_rtc_logger_format.c` / `.h`

`rs_rtc_logger_format.h` is explicitly marked in its own file header as *"an internal RTC Event Logger interface — it is not part of the public API."* It declares two 🔒 **INTERNAL** functions with external linkage (callable from `rs_rtc_logger.c`), backed by six ⚫ **PRIVATE** helpers local to `rs_rtc_logger_format.c`.

### 🔒 `rs_rtc_logger_format_timestamp()` — INTERNAL

```c
uint32_t rs_rtc_logger_format_timestamp(rtc_time_t              const * p_time,
                                         rs_rtc_logger_ts_format_t       format,
                                         bool                            ts_brackets,
                                         char                          * p_buf,
                                         uint32_t                        buf_size);
```

Formats an `rtc_time_t` timestamp into a caller-supplied ASCII buffer, per the selected format (ISO 8601, time-only, compact, or syslog) and bracket option. Output is always NUL-terminated; integer arithmetic only; no heap allocation. Returns the number of characters written (not including NUL), or `0` on error (buffer too small or invalid format). Called by `rs_rtc_logger_format_line()` and, indirectly, by `rs_rtc_logger_Write()` / `rs_rtc_logger_WriteStr()`.

### 🔒 `rs_rtc_logger_format_line()` — INTERNAL

```c
uint32_t rs_rtc_logger_format_line(rs_rtc_logger_event_entry_t const * p_entry,
                                    rs_rtc_logger_ts_format_t           format,
                                    bool                                ts_brackets,
                                    rs_rtc_logger_term_t                term,
                                    char                              * p_buf,
                                    uint32_t                            buf_size);
```

Composes a complete formatted log line (timestamp + event payload + sequence number + line terminator) from a circular buffer entry into a single NUL-terminated ASCII line, ready for UART TX. Returns total characters written, or `0` on error.

### ⚫ `rs_rtc_logger_format_terminator()` — PRIVATE

```c
static uint32_t rs_rtc_logger_format_terminator(rs_rtc_logger_term_t term,
                                                 char               * p_buf,
                                                 uint32_t             offset,
                                                 uint32_t             buf_size);
```

Appends the line terminator string (`\r\n`, `\n`, or `\r`) to a caller-supplied buffer at the given offset. Returns the number of terminator characters written (1 or 2), or `0` if insufficient space. Deliberately kept out of the public/internal header — it was removed from there as part of a prior cleanup so it cannot be called from outside this file.

### ⚫ `rm_rlf_write_u2()` / `rm_rlf_write_d2()` / `rm_rlf_write_year()` / `rm_rlf_write_uint()` / `rm_rlf_write_str()` — PRIVATE

Low-level ASCII-writing primitives shared by `rs_rtc_logger_format_timestamp()` and `rs_rtc_logger_format_line()`. All are pure, integer-only, and named with the `rm_rlf_` private prefix (module-local formatting helpers, distinct from the `rs_rtc_logger_` / `rs_rl_` prefixes used elsewhere):

| Function | Purpose |
|---|---|
| `rm_rlf_write_u2()` | Writes a 2-digit, zero-padded decimal integer (e.g. month, hour) |
| `rm_rlf_write_d2()` | Writes a 2-character, **space**-padded decimal integer — used for the syslog day field, since RFC 3164 requires a space-padded single-digit day (` 1`, not `01`) |
| `rm_rlf_write_year()` | Writes a 4-digit year, converting `tm_year` (years since 1900) to the full calendar year |
| `rm_rlf_write_uint()` | Writes a `uint32_t` as a variable-width decimal string (used for the sequence number) |
| `rm_rlf_write_str()` | Copies a fixed-length string (need not be NUL-terminated) into the output buffer |

---

## 10. Internal Functions — `rs_rtc_logger_hal.c` / `.h`

`rs_rtc_logger_hal.h` is explicitly marked *"Internal header — not part of the public API."* It declares seven 🔒 **INTERNAL** HAL-wrapper functions, backed by two ⚫ **PRIVATE** helpers local to `rs_rtc_logger_hal.c`.

### 🔒 RTC wrappers — INTERNAL

| Function | Purpose |
|---|---|
| `rs_rtc_logger_hal_rtc_open(p_ctrl)` | Wraps `p_rtc->p_api->open()`. Validates the RTC oscillator is running by probing with a `calendarTimeGet()` call after open. Returns `FSP_ERR_NOT_INITIALIZED` if the oscillator is not running. |
| `rs_rtc_logger_hal_rtc_close(p_ctrl)` | Wraps `p_rtc->p_api->close()`. |
| `rs_rtc_logger_hal_rtc_time_get(p_ctrl, p_time)` | Direct calendar-time read — see the "seconds-sandwich" implementation note in the source; deliberately does **not** call `R_RTC_CalendarTimeGet()` (the carry-interrupt method) to avoid the ISR-priority deadlock risk documented in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings). |
| `rs_rtc_logger_hal_rtc_time_set(p_ctrl, p_time)` | Wraps `p_rtc->p_api->calendarTimeSet()`. |
| `rs_rtc_logger_hal_loco_freq_set(p_ctrl, rfrl_value)` | Stops the RTC counter, writes a new RFRL value, reads back the retained calendar time, and restarts the counter — no seconds are lost. Backs the public `rs_rtc_logger_LocoCalibrationSet()`. Valid RFRL range: 8–510. |

### 🔒 UART wrappers — INTERNAL

| Function | Purpose |
|---|---|
| `rs_rtc_logger_hal_uart_open(p_ctrl)` | Calls `rs_rtc_logger_console_init()` (see [Section 11](#11-internal-functions--rs_rtc_logger_stdio_consolec--h)), which ensures the shared Buffered I/O UART driver is initialized and strictly validated against the board reference configuration. It is safe to call even if the driver already opened by the UART earlier (e.g. via the reference application's sign-on banner's first `printf()`). |
| `rs_rtc_logger_hal_uart_close(p_ctrl)` | A no-op that always returns `FSP_SUCCESS`. The shared UART is owned by the Buffered I/O UART driver and is never closed for the life of the application. |
| `rs_rtc_logger_hal_uart_write(p_ctrl, p_data, length)` | Writes via `fwrite()` + `fflush(stdout)` on the same stdio stream the Buffered I/O UART driver owns. Returns once the write has been safely submitted/serialized (the driver's own `uart_flush()` spin-waits briefly on its internal busy flag if a previous transmission is still in flight). Increments `p_ctrl->tx_errors` and returns `FSP_ERR_WRITE_FAILED` if `fwrite()`/`fflush()` do not complete cleanly. |

### ⚫ `rs_rl_bcd_to_dec()` — PRIVATE

```c
static uint8_t rs_rl_bcd_to_dec(uint8_t bcd);
```

Converts a packed BCD byte to a decimal integer (e.g. `0x21` → `21`). Used exclusively by `rs_rtc_logger_hal_rtc_time_get()` to decode RTC calendar registers. Integer arithmetic only, no division.

---

## 11. Internal Functions — `rs_rtc_logger_stdio_console.c` / `.h`

Uses Buffered I/O over UART (see [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update)). This file physically ships inside the `rs_rtc_logger/` core folder, but its three functions split across the public/private line differently than the name suggests — see the table below. It is a thin, non-blocking bridge between this component (and the reference CLI) and the vendored Buffered I/O UART driver (`src/rs_stdio_uart_buffer_io/`, used verbatim/unmodified).

| Function | Tag | Caller | Purpose |
|---|---|---|---|
| `rs_rtc_logger_console_init()` | 🔒 INTERNAL (core dependency) | `rs_rtc_logger_hal_uart_open()` — i.e. every `rs_rtc_logger_Open()` call | Forces deterministic initialization of the vendored driver (the driver otherwise initializes lazily on first `printf()`/`getchar()`/`fflush()`) and runs the driver's own public `RS_STDIO_UART_CheckApplicationConfiguration()` exactly once, caching the result. A validation failure is returned on every subsequent call too, since misconfiguration cannot be fixed by retrying — it propagates through `rs_rtc_logger_hal_uart_open()` and `rs_rtc_logger_Open()` the same way any other `Open()` failure does. |
| `rs_rtc_logger_console_getc_nb(p_c)` | Demo-only convenience | Reference CLI task (`demo_rx_getc()`) | Non-blocking single-character read. Peeks the vendored driver's RX byte queue depth via `R_BYTEQ_Used()` — a non-public but externally-linkable symbol (`uart_rx_q_ctrl`) — before ever calling the driver's own `getchar()`, which otherwise blocks indefinitely when nothing is queued (a documented limitation of the vendored driver). Returns `false` immediately if nothing is available; never blocks. |
| `rs_rtc_logger_console_puts(p_str)` | Demo-only convenience | Reference CLI task (`demo_uart_write()`) | Writes a NUL-terminated string via `fwrite()` then `fflush(stdout)`. Not used by `rs_rtc_logger_hal_uart_write()` itself, which calls `fwrite()`/`fflush()` directly (see [Section 10](#10-internal-functions--rs_rtc_logger_halc--h)) — this function exists purely for the reference CLI's convenience. |

`rs_rtc_logger_console_init()` is the one function in this file the core module actually depends on for `Open()` to succeed — despite the file's own header comment describing it as a "demo application" adapter, it sits on the core's call path. Keep this in mind if you ever consider dropping the file as pure demo scaffolding: doing so breaks `rs_rtc_logger_hal_uart_open()`. See [FILE_GUIDE.md](FILE_GUIDE.md) for the full reuse/modify guidance on this file.

---

## 12. Complete Internal Function Reference Table

| Function | Tag | File | Compiled When |
|---|---|---|---|
| `rs_rl_is_open()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always |
| `rs_rl_enqueue()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always |
| `rs_rl_dequeue()` | ⚫ PRIVATE | `rs_rtc_logger.c` | Always (both modes) |
| `rs_rl_auto_init()` | ⚫ PRIVATE | `rs_rtc_logger.c` | `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1` |
| `rs_rtc_logger_internal_consumer_task()` | ⚫ PRIVATE | `rs_rtc_logger.c` | `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE == 1` |
| `rs_rtc_logger_format_timestamp()` | 🔒 INTERNAL | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_format_line()` | 🔒 INTERNAL | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_format_terminator()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_u2()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_d2()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_year()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_uint()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rm_rlf_write_str()` | ⚫ PRIVATE | `rs_rtc_logger_format.c` | Always |
| `rs_rtc_logger_hal_rtc_open()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_close()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_time_get()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_rtc_time_set()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_loco_freq_set()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_open()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_close()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_hal_uart_write()` | 🔒 INTERNAL | `rs_rtc_logger_hal.c` | Always |
| `rs_rl_bcd_to_dec()` | ⚫ PRIVATE | `rs_rtc_logger_hal.c` | Always |
| `rs_rtc_logger_console_init()` | 🔒 INTERNAL (core dependency) | `rs_rtc_logger_stdio_console.c` | Always |
| `rs_rtc_logger_console_getc_nb()` | Demo-only convenience | `rs_rtc_logger_stdio_console.c` | Always (unused unless the demo calls it) |
| `rs_rtc_logger_console_puts()` | Demo-only convenience | `rs_rtc_logger_stdio_console.c` | Always (unused unless the demo calls it) |

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
