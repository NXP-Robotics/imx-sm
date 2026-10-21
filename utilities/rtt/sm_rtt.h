/*
** ###################################################################
**
** Copyright 2026 NXP
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** o Redistributions of source code must retain the above copyright notice, this list
**   of conditions and the following disclaimer.
**
** o Redistributions in binary form must reproduce the above copyright notice, this
**   list of conditions and the following disclaimer in the documentation and/or
**   other materials provided with the distribution.
**
** o Neither the name of the copyright holder nor the names of its
**   contributors may be used to endorse or promote products derived from this
**   software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
** ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** ###################################################################
*/

/*!
 * @addtogroup RTT
 * @{
 *
 * Header file for the debug console over a SWD debug probe.
 *
 * Carries the debug console over an attached SWD/JTAG probe instead of a
 * UART, using a pair of RAM ring buffers with a SEGGER RTT compatible
 * control block. The probe performs its half of each transfer using
 * background memory access, so no pins, SWO or trace clock are needed.
 *
 * No initialization call is needed, the control block is published on
 * first use. These calls replace the LPUART ones and handle blocking and
 * CR/LF translation internally:
 *
 *   LPUART_WriteBlocking()   -> SM_RTT_ConsoleWrite()
 *   LPUART_ReadBlocking()    -> SM_RTT_ConsoleRead()
 *   LPUART_GetStatusFlags()  -> SM_RTT_ConsoleCharPending()
 *
 * Attach from the host with JLinkRTTClient, or:
 *
 *   pyocd rtt -t mimx95_cm33_mx25um -a 0x20020000 -s 0x20000
 *
 * pyOCD needs the search range, the region it uses by default does not
 * contain .bss.
 */

#ifndef SM_RTT_H
#define SM_RTT_H

/* Includes */

#include "sm.h"

/* Defines */

/*! Size of the target to host (stdout/stderr) buffer */
#ifndef SM_RTT_UP_SIZE
#define SM_RTT_UP_SIZE      4096U
#endif

/*! Size of the host to target (stdin) buffer */
#ifndef SM_RTT_DOWN_SIZE
#define SM_RTT_DOWN_SIZE    64U
#endif

/*!
 * Microseconds a write may wait for a reading host to drain a full
 * buffer. Budget for the whole call, not per byte. Must stay well below
 * the watchdog period.
 */
#ifndef SM_RTT_DRAIN_USEC
#define SM_RTT_DRAIN_USEC   10000U
#endif

/* Functions */

/*!
 * Console write, replaces LPUART_WriteBlocking().
 *
 * Expands '\n' to CR/LF. Best effort: a UART drains its own FIFO at the
 * baud rate, but nothing drains this buffer unless a host is reading it,
 * so a full buffer is waited on for at most SM_RTT_DRAIN_USEC and only
 * when a host has been seen consuming data and the SysTick is refreshing
 * the watchdog. Anything that still does not fit is dropped, so output
 * is discarded rather than blocking when no host is attached.
 *
 * @param[in]     data   Buffer to send
 * @param[in]     len    Number of bytes to send
 */
void SM_RTT_ConsoleWrite(const uint8_t *data, uint32_t len);

/*!
 * Console read, replaces LPUART_ReadBlocking().
 *
 * Blocks until @a len bytes arrive, translating '\r' to '\n'. Does not
 * echo, as RTT host clients echo typed input locally.
 *
 * @param[out]    data   Buffer to fill
 * @param[in]     len    Number of bytes to read
 */
void SM_RTT_ConsoleRead(uint8_t *data, uint32_t len);

/*!
 * Check for pending console input, replaces LPUART_GetStatusFlags().
 *
 * @return Returns true if at least one byte can be read.
 */
bool SM_RTT_ConsoleCharPending(void);

#endif /* SM_RTT_H */

/** @} */
