/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SEGGER_RTT.c — Minimal SEGGER Real-Time Transfer implementation.
 *
 * Implements the RTT protocol used by J-Link / J-Link RTT Viewer.
 * Only the functions required by rs_rtc_logger_rtt_console.c are implemented:
 *   SEGGER_RTT_Init / SEGGER_RTT_Write / SEGGER_RTT_HasKey / SEGGER_RTT_GetKey
 *
 * Ring-buffer invariant
 * ---------------------
 * One slot is always kept empty so that a full buffer (WrOff == RdOff - 1 mod
 * Size) is distinguishable from an empty buffer (WrOff == RdOff).
 * Available bytes to write = (RdOff - WrOff - 1 + Size) % Size
 * Available bytes to read  = (WrOff - RdOff       + Size) % Size
 *
 * J-Link visibility
 * -----------------
 * J-Link searches target RAM for the 16-byte ASCII signature "SEGGER RTT"
 * (NUL-padded to 16 bytes) at the start of the control block.
 * Declaring _SEGGER_RTT as a global (placed in .bss / .data by default) puts
 * it in the SRAM region that J-Link scans, so no linker-script entry or
 * explicit symbol export is required.
 **********************************************************************************************************************/

#include "SEGGER_RTT.h"
#include <string.h>

/*──────────────────────────────────────────────────────────────────────────────
 * Static ring-buffer storage — sized by SEGGER_RTT_Conf.h tunables.
 *────────────────────────────────────────────────────────────────────────────*/
static char _rtt_up_buf  [BUFFER_SIZE_UP];
static char _rtt_down_buf[BUFFER_SIZE_DOWN];

/*──────────────────────────────────────────────────────────────────────────────
 * RTT control block.
 *
 * The ANSI-C 'volatile' qualifier is intentionally NOT used on the control
 * block itself because J-Link accesses it via the debug port (not via the
 * CPU data bus) and will not cause the compiler to incorrectly cache values.
 * The LOCK/UNLOCK macros provide the necessary ordering guarantees for the
 * target side.
 *────────────────────────────────────────────────────────────────────────────*/
SEGGER_RTT_CB _SEGGER_RTT;

/** Guard against double-initialisation from two concurrent callers. */
static volatile int _rtt_initialized = 0;

/*──────────────────────────────────────────────────────────────────────────────
 * SEGGER_RTT_Init
 *────────────────────────────────────────────────────────────────────────────*/
void SEGGER_RTT_Init(void)
{
    SEGGER_RTT_LOCK();

    if (_rtt_initialized)
    {
        SEGGER_RTT_UNLOCK();
        return;
    }

    /* Zero the whole control block first so all padding / reserved fields
     * are deterministically 0 (J-Link may inspect any part of the struct). */
    memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));

    /* Up-buffer 0: target → J-Link RTT Viewer */
    _SEGGER_RTT.aUp[0].sName        = "Terminal";
    _SEGGER_RTT.aUp[0].pBuffer      = _rtt_up_buf;
    _SEGGER_RTT.aUp[0].SizeOfBuffer = sizeof(_rtt_up_buf);
    _SEGGER_RTT.aUp[0].WrOff        = 0U;
    _SEGGER_RTT.aUp[0].RdOff        = 0U;
    _SEGGER_RTT.aUp[0].Flags        = SEGGER_RTT_MODE_DEFAULT;

    /* Down-buffer 0: J-Link RTT Viewer → target */
    _SEGGER_RTT.aDown[0].sName        = "Terminal";
    _SEGGER_RTT.aDown[0].pBuffer      = _rtt_down_buf;
    _SEGGER_RTT.aDown[0].SizeOfBuffer = sizeof(_rtt_down_buf);
    _SEGGER_RTT.aDown[0].WrOff        = 0U;
    _SEGGER_RTT.aDown[0].RdOff        = 0U;
    _SEGGER_RTT.aDown[0].Flags        = 0U;

    /* Channel counts */
    _SEGGER_RTT.MaxNumUpBuffers   = SEGGER_RTT_MAX_NUM_UP_BUFFERS;
    _SEGGER_RTT.MaxNumDownBuffers = SEGGER_RTT_MAX_NUM_DOWN_BUFFERS;

    /* Write the magic ID LAST so J-Link cannot find a partially-initialised
     * control block.  "SEGGER RTT" is 10 chars; the remaining 6 bytes are
     * already 0 from the memset above. */
    memcpy(_SEGGER_RTT.acID, "SEGGER RTT", 10U);

    _rtt_initialized = 1;

    SEGGER_RTT_UNLOCK();
}

/*──────────────────────────────────────────────────────────────────────────────
 * SEGGER_RTT_Write  (up-buffer, target → J-Link)
 *────────────────────────────────────────────────────────────────────────────*/
unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void * pBuffer, unsigned NumBytes)
{
    if ((BufferIndex >= (unsigned)SEGGER_RTT_MAX_NUM_UP_BUFFERS) ||
        (NULL == pBuffer) || (0U == NumBytes))
    {
        return 0U;
    }

    SEGGER_RTT_BUFFER_UP * pRing = &_SEGGER_RTT.aUp[BufferIndex];

    SEGGER_RTT_LOCK();

    unsigned WrOff = pRing->WrOff;
    unsigned RdOff = pRing->RdOff;
    unsigned Size  = pRing->SizeOfBuffer;

    /* Available space (one slot kept empty per ring-buffer invariant) */
    unsigned Avail = (WrOff >= RdOff) ? (Size - (WrOff - RdOff) - 1U)
                                      : (RdOff - WrOff - 1U);

    if (0U == Avail)
    {
        SEGGER_RTT_UNLOCK();
        return 0U;
    }

    /* Trim write to what fits (NO_BLOCK_TRIM / NO_BLOCK_SKIP policy) */
    if (NumBytes > Avail)
    {
        NumBytes = Avail;
    }

    /* Write to ring buffer, handling the wrap-around case */
    unsigned Rem = Size - WrOff;  /* contiguous space from WrOff to end */

    if (NumBytes <= Rem)
    {
        /* Contiguous — single memcpy */
        memcpy(pRing->pBuffer + WrOff, pBuffer, NumBytes);
        WrOff += NumBytes;
        if (WrOff == Size) { WrOff = 0U; }
    }
    else
    {
        /* Wrapping — two memcpy calls */
        memcpy(pRing->pBuffer + WrOff, pBuffer, Rem);
        memcpy(pRing->pBuffer, (const char *)pBuffer + Rem, NumBytes - Rem);
        WrOff = NumBytes - Rem;
    }

    pRing->WrOff = WrOff;

    SEGGER_RTT_UNLOCK();

    return NumBytes;
}

/*──────────────────────────────────────────────────────────────────────────────
 * SEGGER_RTT_HasKey  (down-buffer 0, non-blocking peek)
 *────────────────────────────────────────────────────────────────────────────*/
int SEGGER_RTT_HasKey(void)
{
    /* Read WrOff without a lock: J-Link only ever WRITES WrOff (it advances
     * the write pointer when it puts a character in the down-buffer) and the
     * target only READs WrOff here.  A lock-free read of an unsigned value
     * that the other side only increments is safe on Cortex-M33 because
     * aligned 32-bit reads/writes are atomic (ARMv8-M architecture reference). */
    return (_SEGGER_RTT.aDown[0].WrOff != _SEGGER_RTT.aDown[0].RdOff) ? 1 : 0;
}

/*──────────────────────────────────────────────────────────────────────────────
 * SEGGER_RTT_GetKey  (down-buffer 0, non-blocking read)
 *────────────────────────────────────────────────────────────────────────────*/
int SEGGER_RTT_GetKey(void)
{
    SEGGER_RTT_BUFFER_DOWN * pRing = &_SEGGER_RTT.aDown[0];

    SEGGER_RTT_LOCK();

    if (pRing->WrOff == pRing->RdOff)
    {
        SEGGER_RTT_UNLOCK();
        return -1;
    }

    int c = (int)(unsigned char)pRing->pBuffer[pRing->RdOff];

    unsigned NextRd = pRing->RdOff + 1U;
    if (NextRd >= pRing->SizeOfBuffer) { NextRd = 0U; }
    pRing->RdOff = NextRd;

    SEGGER_RTT_UNLOCK();

    return c;
}
