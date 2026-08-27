/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SEGGER_RTT.h — SEGGER Real-Time Transfer (RTT) API.
 *
 * RTT protocol summary
 * --------------------
 * J-Link searches target RAM for the 16-byte magic string "SEGGER RTT\0\0\0\0\0\0"
 * that marks the start of the control block (_SEGGER_RTT).  It reads WrOff
 * (written by the target) from up-buffers to drain log output, and writes WrOff
 * in down-buffers so the target can read keystrokes.
 *
 * This implementation is a minimal, self-contained version:
 *   - One up-buffer   (channel 0, "Terminal") — target → J-Link RTT Viewer
 *   - One down-buffer (channel 0, "Terminal") — J-Link RTT Viewer → target
 *   - Static allocation only; no malloc
 *   - FreeRTOS critical-section locking (configured in SEGGER_RTT_Conf.h)
 **********************************************************************************************************************/

#ifndef SEGGER_RTT_H
#define SEGGER_RTT_H

#include "SEGGER_RTT_Conf.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*──────────────────────────────────────────────────────────────────────────────
 * Write-mode flags (stored in SEGGER_RTT_BUFFER_UP.Flags)
 *────────────────────────────────────────────────────────────────────────────*/
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP     (0U) /**< Skip write if buffer full  */
#define SEGGER_RTT_MODE_NO_BLOCK_TRIM     (1U) /**< Trim write to fit buffer   */
#define SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL (2U) /**< Spin until space available */

/*──────────────────────────────────────────────────────────────────────────────
 * Ring-buffer descriptors — layout must exactly match what J-Link expects.
 *────────────────────────────────────────────────────────────────────────────*/

/** Up-buffer: data flows from the target to J-Link (log output). */
typedef struct {
    const char * sName;        /**< Human-readable channel name            */
    char       * pBuffer;      /**< Pointer to the ring-buffer storage      */
    unsigned     SizeOfBuffer; /**< Storage size in bytes                   */
    unsigned     WrOff;        /**< Write offset, advanced by the target    */
    unsigned     RdOff;        /**< Read  offset, advanced by J-Link        */
    unsigned     Flags;        /**< SEGGER_RTT_MODE_* write policy          */
} SEGGER_RTT_BUFFER_UP;

/** Down-buffer: data flows from J-Link to the target (CLI input). */
typedef struct {
    const char * sName;        /**< Human-readable channel name            */
    char       * pBuffer;      /**< Pointer to the ring-buffer storage      */
    unsigned     SizeOfBuffer; /**< Storage size in bytes                   */
    unsigned     WrOff;        /**< Write offset, advanced by J-Link        */
    unsigned     RdOff;        /**< Read  offset, advanced by the target    */
    unsigned     Flags;        /**< Reserved (unused for down-buffers)      */
} SEGGER_RTT_BUFFER_DOWN;

/** RTT control block — J-Link locates this in RAM by scanning for acID. */
typedef struct {
    char                   acID[16];                                /**< "SEGGER RTT\0\0\0\0\0\0" */
    int                    MaxNumUpBuffers;                         /**< Number of up-buffers       */
    int                    MaxNumDownBuffers;                       /**< Number of down-buffers     */
    SEGGER_RTT_BUFFER_UP   aUp  [SEGGER_RTT_MAX_NUM_UP_BUFFERS];   /**< Up-buffer array            */
    SEGGER_RTT_BUFFER_DOWN aDown[SEGGER_RTT_MAX_NUM_DOWN_BUFFERS]; /**< Down-buffer array          */
} SEGGER_RTT_CB;

/*──────────────────────────────────────────────────────────────────────────────
 * Public API
 *────────────────────────────────────────────────────────────────────────────*/

/**
 * @brief Initialise the RTT control block.
 *
 * Writes the "SEGGER RTT" magic ID and sets up channel 0 up/down buffers.
 * Idempotent — safe to call more than once (subsequent calls are no-ops).
 */
void SEGGER_RTT_Init(void);

/**
 * @brief Write bytes to an RTT up-buffer (target → J-Link RTT Viewer).
 *
 * @param BufferIndex  Up-buffer channel index (0 for "Terminal").
 * @param pBuffer      Source data (not required to be NUL-terminated).
 * @param NumBytes     Number of bytes to write.
 *
 * @return Number of bytes actually written (may be less than NumBytes if the
 *         buffer is full and the channel is in NO_BLOCK_TRIM mode).
 */
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void * pBuffer, unsigned NumBytes);

/**
 * @brief Check whether a character is available in down-buffer 0.
 *
 * @return 1 if at least one byte is waiting; 0 otherwise.
 */
int SEGGER_RTT_HasKey(void);

/**
 * @brief Read one character from down-buffer 0 (J-Link RTT Viewer → target).
 *
 * Non-blocking: returns immediately even when no data is available.
 *
 * @return The character (0–255) on success, or -1 if the buffer is empty.
 */
int SEGGER_RTT_GetKey(void);

#ifdef __cplusplus
}
#endif

#endif /* SEGGER_RTT_H */
