/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SEGGER_RTT_Conf.h — project-specific RTT configuration for the RTC Event Logger.
 *
 * RAM budget (RTT backend, compile with RS_RTC_LOGGER_CFG_CONSOLE_BACKEND=1):
 *   Up-buffer   0  "Terminal"  512 B   log output (target → J-Link host)
 *   Down-buffer 0  "Terminal"   64 B   CLI input  (J-Link host → target)
 *   Control block header         ~72 B
 *   ─────────────────────────────────
 *   Total                       ~648 B
 *
 * vs. UART buffered-IO backend:
 *   TX byteq    256 B + malloc flush buffer ~256 B  (heap)
 *   RX byteq    256 B
 *   Total       ~768 B  (plus heap fragmentation)
 *
 * RTT is therefore cheaper in RAM and eliminates all heap use in the I/O path.
 **********************************************************************************************************************/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

/*──────────────────────────────────────────────────────────────────────────────
 * Channel count: only channel 0 ("Terminal") is used.
 *────────────────────────────────────────────────────────────────────────────*/
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS    (1U)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS  (1U)

/*──────────────────────────────────────────────────────────────────────────────
 * Buffer sizes.
 * BUFFER_SIZE_UP must be large enough for the longest formatted log line
 * (RS_RTC_LOGGER_CFG_LINE_BUF_SIZE = 256 B default) plus some headroom for
 * burst queuing before J-Link drains the buffer.  512 B fits two full lines
 * simultaneously, which is sufficient for normal single-event bursts.
 *────────────────────────────────────────────────────────────────────────────*/
#ifndef BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP   (512U)   /* target → J-Link (log output) */
#endif
#ifndef BUFFER_SIZE_DOWN
  #define BUFFER_SIZE_DOWN  (64U)   /* J-Link → target (CLI keystrokes) */
#endif

/*──────────────────────────────────────────────────────────────────────────────
 * Locking.
 * All RTT I/O in this project (console_init / console_write / console_getc_nb)
 * is called exclusively from FreeRTOS task context — never from an ISR.
 * FreeRTOS critical sections are therefore correct and sufficient.
 *
 * SEGGER_RTT_LOCK / UNLOCK must be paired as a compound statement block;
 * the braces below ensure the saved_ipl variable is scoped to the lock region
 * so nested lock/unlock sequences work correctly even if the preprocessor
 * expands them inline.
 *────────────────────────────────────────────────────────────────────────────*/
#include "FreeRTOS.h"
#include "task.h"

#define SEGGER_RTT_LOCK()   do { taskENTER_CRITICAL();
#define SEGGER_RTT_UNLOCK()      taskEXIT_CRITICAL(); } while (0)

/*──────────────────────────────────────────────────────────────────────────────
 * Memory attributes — place control block in normal (non-cached) SRAM.
 * The RA4E2 has a single SRAM region; no special placement is needed.
 *────────────────────────────────────────────────────────────────────────────*/
#define SEGGER_RTT_SECTION(name)   /* default: linker assigns .bss / .data */
#define SEGGER_RTT_ALIGNMENT       4U

/*──────────────────────────────────────────────────────────────────────────────
 * Default channel 0 write mode.
 * NO_BLOCK_TRIM: if the up-buffer is full the write is trimmed (no target
 * stall).  This is preferable to blocking on an embedded target since a J-Link
 * that is not running RTT viewer would otherwise deadlock the consumer task.
 *────────────────────────────────────────────────────────────────────────────*/
#define SEGGER_RTT_MODE_DEFAULT    SEGGER_RTT_MODE_NO_BLOCK_TRIM

#endif /* SEGGER_RTT_CONF_H */
