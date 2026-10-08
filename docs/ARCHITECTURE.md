# Design Flow and Architecture

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Lifecycle State Machine](#1-lifecycle-state-machine)
2. [Public Function State Gating](#2-public-function-state-gating)
3. [Event Capture and Telemetry Flow](#3-event-capture-and-telemetry-flow)
4. [Module File Responsibilities](#4-module-file-responsibilities)

---

## 1. Lifecycle State Machine

The module implements a four-state lifecycle. State transitions are the only way to change state; no transition occurs implicitly except for the auto-initialization on first `rs_rtc_logger_EventTrigger()` call in Example Project mode.

```
         rs_rtc_logger_Open()              rs_rtc_logger_Start()
CLOSED ─────────────────────→ OPEN ──────────────────────────→ STARTED
   ↑                           │   ↑   rs_rtc_logger_Stop()   │
   │   rs_rtc_logger_Close()   │   └────────────────────────←┘
   └───────────────────────────┘
                               │
         rs_rtc_logger_Close() │   (from STOPPED also)
                               ↓
                             CLOSED
```

| State | Description |
|---|---|
| CLOSED | Resources not allocated; both HAL drivers closed |
| OPEN | Initialized; UART output path not yet active; `rs_rtc_logger_TimeSet()`, `rs_rtc_logger_SeqCountSet()` may be called |
| STARTED | UART output path active; `rs_rtc_logger_EventTrigger()` and `rs_rtc_logger_Write()` accepted |
| STOPPED | TX suspended; buffer continues accepting events; `rs_rtc_logger_Start()` resumes transmission |

---

## 2. Public Function State Gating

| Public Function | CLOSED | OPEN | STARTED | STOPPED |
|---|---|---|---|---|
| `rs_rtc_logger_Open()` | ✓ | ERR_ALREADY_OPEN | ERR_ALREADY_OPEN | ERR_ALREADY_OPEN |
| `rs_rtc_logger_Start()` | ✗ | ✓ | ERR_INVALID_MODE | ✓ |
| `rs_rtc_logger_Stop()` | ✗ | ✗ | ✓ | ERR_INVALID_MODE |
| `rs_rtc_logger_Close()` | ✗ | ✓ | Stop first | ✓ |
| `rs_rtc_logger_EventTrigger()` | EP only¹ | ✗ | ✓ | ✓ |
| `rs_rtc_logger_Write()` | ✗ | ✗ | ✓ | ✗ |
| `rs_rtc_logger_WriteStr()` | ✗ | ✗ | ✓ | ✗ |
| `rs_rtc_logger_TimestampFormatSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_TimeSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_ClockValidate()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_LocoCalibrationSet()` | ✗ | hot | hot | hot |
| `rs_rtc_logger_SeqCountSet()` | ✗ | ✓ | ✗ | ✓ |
| `rs_rtc_logger_StatusGet()` | ✗ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_TimeGet()` | ✗ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_VersionGet()` | ✓ | ✓ | ✓ | ✓ |
| `rs_rtc_logger_ConfigGen()` / `ConfigCheck()` / `ConfigGetSize()` | ✓ | ✓ | ✓ | ✓ |

¹ EP only = Example Project mode only: `rs_rtc_logger_EventTrigger()` in CLOSED state enqueues the event immediately (no event loss) and triggers a deferred auto-`rs_rtc_logger_Open()`/`rs_rtc_logger_Start()` sequence. Expert mode returns `FSP_ERR_NOT_OPEN` in CLOSED state.

*hot = hot-safe: callable while STARTED; takes effect on the next `rs_rtc_logger_Write()` call without interrupting the current transmission. See the definition of "hot-safe" at the top of [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md).*

---

## 3. Event Capture and Telemetry Flow

```
Application ISR (r_icu button callback)
  │
  └──→ rs_rtc_logger_EventTrigger(p_ctrl, p_data, length)
            │
            ├── R_RTC_CalendarTimeGet()        ← timestamp captured at ISR time
            ├── seq_count++ (atomic)
            ├── enqueue to event_buf[buf_head]  ← inline copy, no pointers
            └── xTaskNotifyFromISR()
                    │
                    ↓
         Consumer Task (module-owned in Example Project mode /
                        app-owned in Expert mode)
            │
            ├── dequeue event_buf[buf_tail]
            └── rs_rtc_logger_Write(p_ctrl, &entry)
                    │
                    ├── format: "[YYYY-MM-DD HH:MM:SS] Event Detected #N\r\n"
                    └── rs_rtc_logger_hal_uart_write(line_buf)
                            │
                            ├── fwrite(line_buf, ..., stdout)   ← queues into the
                            │                                     Buffered I/O UART
                            │                                     driver's TX byteq
                            └── fflush(stdout)                  ← hands queued bytes
                                    │                              to the UART driver;
                                    │                              briefly spin-waits
                                    │                              on the driver's own
                                    │                              busy flag if a prior
                                    │                              transmission is still
                                    │                              in flight, then returns
                                    ↓
                    (call returns here — write is safely submitted/
                     serialized; consumer task proceeds to the next
                     entry immediately, no separate completion wait)
```

`rs_rtc_logger_hal_uart_write()` goes through `fwrite()`/`fflush(stdout)` on the shared Buffered I/O UART driver's stdio stream and does not return until the write has already been safely handed off (the driver's own `uart_flush()` serializes internally).

In the reference application, the button ISR path additionally applies a software debounce (`demo_button_s1_callback()`) before calling `rs_rtc_logger_EventTrigger()` — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#6-debounce-configuration-reference-application). That debounce sits entirely in application code, upstream of the diagram above; the core module's flow is unchanged regardless of what triggers `EventTrigger()`.

---

## 4. Module File Responsibilities

| Module | Responsibility |
|---|---|
| `rs_rtc_logger.c` | Lifecycle state machine; all public function entry points; state gating; `rs_rtc_logger_EventTrigger()` ISR path; circular buffer management; diagnostic counters; Example Project mode auto-lifecycle logic and internal consumer task (compiled in only when `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE` is enabled) |
| `rs_rtc_logger_format.c` | Pure timestamp formatting — converts `rtc_time_t` plus format selection into an ASCII string in a caller-supplied buffer. No I/O. No side effects. Integer arithmetic only. Fully unit-testable in isolation. |
| `rs_rtc_logger_hal.c` | Thin HAL shim: RTC open/close/get/set wrappers (direct raw-driver calls). UART open/close/write wrappers route through `rs_rtc_logger_stdio_console.c` to the shared Buffered I/O UART driver's stdio stream.
| `rs_rtc_logger_stdio_console.c` | Thin adapter to the vendored Buffered I/O UART driver: forces deterministic driver init + validation (`rs_rtc_logger_console_init()`, called from `rs_rtc_logger_hal_uart_open()` — a genuine core dependency), plus non-blocking char read and string write used by the reference CLI |
| `rs_rtc_logger_data_fpb_<board>.c/.h` | Board preset **RTC** instance; also hosts `rs_rtc_logger_ConfigGen()`, `rs_rtc_logger_ConfigCheck()`, `rs_rtc_logger_ConfigGetSize()` (see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md#2-configuration-bridge-call-before-open)) |
| `rs_stdio_uart_buffer_io/*` | **Vendored, used unmodified.** Owns the shared physical UART for the whole application (log output and CLI console alike) — see [FILE_GUIDE.md](FILE_GUIDE.md#4a-vendored-buffered-io-uart-driver--reference-only-do-not-edit) |

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md).*
