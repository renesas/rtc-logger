# Integration Guide

> Part of the [`rs_rtc_logger` documentation set](RTC_EVENT_LOGGER-USER_MANUAL.md#documentation-map).
> This guide covers **how to wire the PES into your application** — initialization order, task/RTOS integration, and multi-instance considerations. For **adapting the core logic to a different event source or use case**, see [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md). For the functions referenced below, see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md).

## Table of Contents

1. [Simple Path Integration (Example Project Mode)](#1-simple-path-integration-example-project-mode)
2. [Expert Path Integration (Full Lifecycle Control)](#2-expert-path-integration-full-lifecycle-control)
3. [Warm-Reset Integration Ordering](#3-warm-reset-integration-ordering)
4. [Task and RTOS Integration](#4-task-and-rtos-integration)
5. [Shared UART and Multi-Instance Considerations](#5-shared-uart-and-multi-instance-considerations)
6. [Firmware-Update Integration Pattern (Expert Mode)](#6-firmware-update-integration-pattern-expert-mode)

---

## 1. Simple Path Integration (Example Project Mode)

This is the recommended starting point for most integrations. Set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 1` (the default) in `rs_rtc_logger_cfg.h`.

Recommended call order, from your FreeRTOS thread entry:

```c
/* 1. Bind the RTC HAL instance (preset or FSP Stack Configurator) */
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);

/* 2. Optional but recommended: validate configuration early */
fsp_err_t err = rs_rtc_logger_ConfigCheck(&g_logger_cfg);
if (FSP_SUCCESS != err) { /* handle bad configuration */ }

/* 3. Create the PES-owned consumer task — task context only, before enabling any IRQ */
rs_rtc_logger_EP_PrepareConsumerTask(&g_logger_ctrl);

/* 4. Enable your event source's interrupt (e.g. r_icu external IRQ) */
R_ICU_ExternalIrqOpen(&g_external_irq_ctrl, &g_external_irq_cfg);
R_ICU_ExternalIrqEnable(&g_external_irq_ctrl);

/* 5. From here on, your ISR calls only: */
rs_rtc_logger_EventTrigger(&g_logger_ctrl, p_data, length);
```

> `rs_rtc_logger_ConfigGen()` takes an RTC instance. The shared UART is opened automatically the first time your application makes any stdio call (`printf`/`getchar`/`fflush`), or deterministically inside `rs_rtc_logger_Open()` itself via `rs_rtc_logger_hal_uart_open()`. You do not need to open it yourself. See [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update).

`rs_rtc_logger_Open()` and `rs_rtc_logger_Start()` are called automatically on the first `EventTrigger()` — you do not call them yourself in this mode. See [ARCHITECTURE.md](ARCHITECTURE.md#2-public-function-state-gating) footnote 1 for exactly what happens on that first call.

**Ordering constraint:** Step 3 (`EP_PrepareConsumerTask`) must complete *before* step 4 (enabling the interrupt), because `xTaskCreateStatic()` must not be called from ISR context and the PES has no way to defer task creation if the ISR fires first.

---

## 2. Expert Path Integration (Full Lifecycle Control)

Set `RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE = 0`. The application owns every transition and must provide its own consumer task.

```c
/* 1. Bind and validate configuration */
rs_rtc_logger_ConfigGen(&g_logger_cfg, &g_rtc0_preset);
rs_rtc_logger_ConfigCheck(&g_logger_cfg);

/* 2. Open — validates RTC oscillator; ensures the shared Buffered I/O UART
 *    driver is initialized and validated (see rs_rtc_logger_hal_uart_open()) */
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);

/* 3. Restore warm-reset state, if applicable — see Section 3 below */

/* 4. Start — enables the UART output path */
rs_rtc_logger_Start(&g_logger_ctrl);

/* 5. Create your own consumer task, which loops on: */
rs_rtc_logger_event_entry_t entry;
if (FSP_SUCCESS == rs_rtc_logger_EventDequeue(&g_logger_ctrl, &entry))
{
    rs_rtc_logger_Write(&g_logger_ctrl, &entry);
}

/* 6. Enable your event source's interrupt — safe any time after Start() */

/* 7. Application-driven shutdown, if needed */
rs_rtc_logger_Stop(&g_logger_ctrl);
rs_rtc_logger_Close(&g_logger_ctrl);
```

Your consumer task can use whatever wait strategy fits your application (polling `EventDequeue()`, a task notification triggered from your own ISR path, a queue, etc.) — the PES does not prescribe one in Expert mode, unlike the internal consumer task used in Example Project mode.

---

## 3. Warm-Reset Integration Ordering

On a warm reset (detected via `R_SYSTEM->RSTSR0` POR flag), the RTC keeps running and its configuration is retained, but the PES's own state (sequence counter, "is the clock trustworthy" flag) resets with the rest of RAM unless your application restores it from non-volatile/`BSP_SECTION_NOINIT` storage. Integrate the restore calls **after `Open()`, before enabling any interrupt source that calls `EventTrigger()`**:

```c
rs_rtc_logger_Open(&g_logger_ctrl, &g_logger_cfg);

if (warm_reset_detected)
{
    rs_rtc_logger_SeqCountSet(&g_logger_ctrl, restored_seq_count);
    rs_rtc_logger_ClockValidate(&g_logger_ctrl, true);
}

rs_rtc_logger_Start(&g_logger_ctrl);
/* enable interrupt source only after this point */
```

**Why ordering matters:** if an `EventTrigger()` ISR fires between the moment your application chooses the restore value and the `SeqCountSet()` store executing, the ISR's `seq_count++` is overwritten by the restore, silently creating a duplicate sequence number in the output. `rs_rtc_logger_Open()` always initializes the "clock is trustworthy" flag to `false`; skipping `ClockValidate(true)` on a warm reset causes the first few events to display `[0000-00-00 00:00:00]` even though the RTC actually holds a valid, previously-set time.

In Example Project mode, `Open()`/`Start()` happen automatically inside the deferred auto-init path — if you need `SeqCountSet()`/`ClockValidate()` restored before the first event is transmitted, call them from your own warm-reset detection code *before* the first `EventTrigger()` call, so they land before the auto-init sequence starts.

---

## 4. Task and RTOS Integration

| Consideration | Example Project Mode | Expert Mode |
|---|---|---|
| Consumer task ownership | PES-owned (internal static task, created by `rs_rtc_logger_EP_PrepareConsumerTask()`) | Application-owned |
| Task stack size | `RS_RTC_LOGGER_CFG_TASK_STACK_SIZE` (default 2048 bytes) | Sized by your own task creation |
| Task priority | `RS_RTC_LOGGER_CFG_TASK_PRIORITY` (default `tskIDLE_PRIORITY + 1`) | Set by your own task creation |
| Consumer task wake interval | `RS_RTC_LOGGER_CFG_TX_TIMEOUT_MS` (default 1000 ms) — periodic wake-up bound on `ulTaskNotifyTake()`, not a completion timeout (writes are synchronous — see [ARCHITECTURE.md](ARCHITECTURE.md#3-event-capture-and-telemetry-flow)) | Your responsibility to guard if desired |
| FreeRTOS heap scheme | `heap_4` (or `heap_3`) required for `xTaskCreateStatic()` | Same requirement applies if you use static allocation |
| `configSUPPORT_STATIC_ALLOCATION` | Must be `1` | Only required if your own task uses static allocation |

Tune `RS_RTC_LOGGER_CFG_TASK_PRIORITY` relative to your other application tasks — see the adaptation table in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#key-adaptation-points).

---

## 5. Shared UART and Multi-Instance Considerations

**The RTC Event Logger uses a Buffered I/O over UART driver** (see [RELEASE_NOTES.md](RELEASE_NOTES.md#2-buffered-io-over-uart-update)). The RTC Event Logger component does not open, own, or register a callback for a UART instance.

The shared physical UART is owned by the **vendored Buffered I/O UART driver** (`src/rs_stdio_uart_buffer_io/`), and both the RTC Event Logger's log output and the reference application's CLI console write through the *same* stdio stream (`stdout`), via `fwrite()`/`fflush()`. This has two practical implications for integration:

1. **Single shared UART, by design.** The vendored driver is written around one physical UART instance (`g_uart_jlob_vcom`) and one pair of RX/TX byte queues. If your application needs the RTC Event Logger to log to a *different* UART than your CLI console uses, you would need to either: (a) run two independent copies of the vendored driver bound to different UART instances (not a supported out-of-the-box configuration — the driver's global byte-queue state, described in `r_byteq.c`, is written for a single instance), or (b) have the RTC Event Logger's log lines share the same stream as your console, as the reference application does.
2. **Multiple `rs_rtc_logger_ctrl_t` instances within one application** are possible in principle (the core module itself has no static state that assumes a single control block — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md)), but since all instances would write through the same shared stdio stream, their log lines would interleave on the same physical UART. This has not been validated by the reference implementation; treat it as a case-by-case adaptation.

If you need genuinely independent UART channels for different purposes (e.g. one for logging, one for a separate protocol), do not attempt to share the vendored driver across them — give the second channel its own `uart_instance_t` opened directly through the FSP HAL, bypassing the Buffered I/O UART driver and `rs_rtc_logger_stdio_console.c` entirely for that channel.

---

## 6. Firmware-Update Integration Pattern (Expert Mode)

In Expert mode, the `rs_rtc_logger_Stop()`/`rs_rtc_logger_Start()` pair can be used to silence UART activity during a firmware write operation while continuing to buffer events:

```c
// Before firmware write — silence UART output
rs_rtc_logger_Stop(&g_logger_ctrl);

// ... firmware write executes here ...
// rs_rtc_logger_EventTrigger() events continue to buffer silently

// After firmware verify — resume logging from same seq_count
rs_rtc_logger_Start(&g_logger_ctrl);
// Buffered events are transmitted in order
```

The `seq_count` and circular buffer are preserved across `rs_rtc_logger_Stop()`/`rs_rtc_logger_Start()`. Events that arrived during the STOPPED window are transmitted in order once STARTED. This pattern is not applicable in Example Project mode, since the PES owns the lifecycle transitions automatically in that mode.

---

*Return to the [User Manual](RTC_EVENT_LOGGER-USER_MANUAL.md). For adapting the core logic to a different event source or use case, see [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md).*
