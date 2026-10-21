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

/*==========================================================================*/
/* Implementation of the debug console over a SWD debug probe.              */
/*                                                                          */
/* Memory safety: the control block lives in RAM the probe can write, so    */
/* all of its fields are untrusted. Buffer addresses, buffer sizes and the  */
/* target owned offsets are held in private static state and are never read */
/* back from it. Only the host owned offset of each buffer is read from the */
/* control block, and it is range checked before use, so a malfunctioning   */
/* probe can corrupt console data but cannot cause an out of bounds access. */
/*==========================================================================*/

/* Includes */

#include <string.h>
#include "fsl_device_registers.h"
#include "sm_rtt.h"

/* Declared locally to avoid including the whole device API for two calls */
extern uint64_t DEV_SM_Usec64Get(void);
extern uint64_t DEV_SM_GetTimerMsec(void);

/* Defines */

/*! Number of up (target to host) buffers */
#define RTT_NUM_UP      1U

/*! Number of down (host to target) buffers */
#define RTT_NUM_DOWN    1U

/*! Buffer is non blocking, drop data when full */
#define RTT_FLAG_SKIP   0U

/* Types */

/*!
 * RTT ring buffer descriptor, layout fixed by the host tools. The
 * offsets are volatile because the probe updates them behind the back of
 * the CPU.
 */
typedef struct
{
    const char *sName;          /*!< Buffer name, shown by host tools */
    uint8_t *pBuffer;           /*!< Pointer to the data buffer */
    uint32_t SizeOfBuffer;      /*!< Size of the data buffer in bytes */
    volatile uint32_t WrOff;    /*!< Write offset, next byte to write */
    volatile uint32_t RdOff;    /*!< Read offset, next byte to read */
    uint32_t Flags;             /*!< Blocking behavior */
} rtt_buffer_t;

/*! RTT control block, layout fixed by the host tools */
typedef struct
{
    char acID[16];              /*!< Signature searched for by the host */
    int32_t MaxNumUpBuffers;    /*!< Number of up buffers */
    int32_t MaxNumDownBuffers;  /*!< Number of down buffers */
    rtt_buffer_t aUp[RTT_NUM_UP];      /*!< Up buffer descriptors */
    rtt_buffer_t aDown[RTT_NUM_DOWN];  /*!< Down buffer descriptors */
} rtt_cb_t;

/* Local Variables */

static uint8_t s_upBuffer[SM_RTT_UP_SIZE];
static uint8_t s_downBuffer[SM_RTT_DOWN_SIZE];

/* Target owned offsets, authoritative copies out of reach of the probe */
static uint32_t s_upWr = 0U;
static uint32_t s_downRd = 0U;

/*
 * Host read offset as last observed, and whether it has ever moved.
 * Progress shows up between calls rather than within one, so the
 * previous value has to be remembered.
 */
static uint32_t s_upRdSeen = 0U;
static bool s_hostReads = false;

/*
 * Named _SEGGER_RTT so stock host tools locate it by scanning RAM for
 * the signature, or by being passed the address of this symbol.
 */
/* coverity[misra_c_2012_rule_5_8_violation] */
rtt_cb_t _SEGGER_RTT;

static bool s_inited = false;

/* Local Functions */

/*--------------------------------------------------------------------------*/
/* Publish the control block on first use                                   */
/*--------------------------------------------------------------------------*/
static void Init(void)
{
    rtt_cb_t *cb = &_SEGGER_RTT;

    if (!s_inited)
    {
        (void) memset((void*) cb, 0, sizeof(rtt_cb_t));

        cb->MaxNumUpBuffers = (int32_t) RTT_NUM_UP;
        cb->MaxNumDownBuffers = (int32_t) RTT_NUM_DOWN;

        cb->aUp[0].sName = "Terminal";
        cb->aUp[0].pBuffer = s_upBuffer;
        cb->aUp[0].SizeOfBuffer = SM_RTT_UP_SIZE;
        cb->aUp[0].Flags = RTT_FLAG_SKIP;

        cb->aDown[0].sName = "Terminal";
        cb->aDown[0].pBuffer = s_downBuffer;
        cb->aDown[0].SizeOfBuffer = SM_RTT_DOWN_SIZE;
        cb->aDown[0].Flags = RTT_FLAG_SKIP;

        /* Publish the signature last so an attached probe never sees a
         * half initialized control block */
        __DMB();
        (void) memcpy((void*) cb->acID, "SEGGER RTT", 11U);
        __DMB();

        s_inited = true;
    }
}

/*--------------------------------------------------------------------------*/
/* Is tick handling live, i.e. is the watchdog being refreshed for us?      */
/*--------------------------------------------------------------------------*/
static bool TickRunning(void)
{
    /*
     * The millisecond counter only advances from SysTick_Handler() once
     * that handler's enable flag is set, which is the same condition
     * that lets it reach BRD_SM_TimerTick() -> BOARD_WdogRefresh(). A
     * non-zero value is therefore evidence that the watchdog is being
     * refreshed. SysTick->CTRL is not a usable substitute: the counter
     * is started in BOARD_InitTimers() but handling stays disabled until
     * SM_SYSTICKENABLE() much later in main(), so between the two the
     * hardware looks live while nothing refreshes the watchdog.
     */
    return DEV_SM_GetTimerMsec() != 0ULL;
}

/*--------------------------------------------------------------------------*/
/* Read a host owned offset, treating an out of range value as empty        */
/*--------------------------------------------------------------------------*/
static uint32_t HostOff(const volatile uint32_t *off, uint32_t size)
{
    uint32_t val = *off;

    return (val < size) ? val : 0U;
}

/*--------------------------------------------------------------------------*/
/* Copy as much as fits into the up ring, in at most two contiguous runs    */
/*--------------------------------------------------------------------------*/
static uint32_t UpCopy(const uint8_t *data, uint32_t len)
{
    /* One byte is always left empty to distinguish full from empty */
    uint32_t rdOff = HostOff(&_SEGGER_RTT.aUp[0].RdOff, SM_RTT_UP_SIZE);
    uint32_t space = ((rdOff > s_upWr) ? (rdOff - s_upWr)
        : ((SM_RTT_UP_SIZE - s_upWr) + rdOff)) - 1U;
    uint32_t done = (len > space) ? space : len;
    uint32_t pos = 0U;

    while (pos < done)
    {
        /* Bytes to the end of the buffer, so each memcpy is contiguous */
        uint32_t run = SM_RTT_UP_SIZE - s_upWr;

        if (run > (done - pos))
        {
            run = done - pos;
        }

        (void) memcpy(&s_upBuffer[s_upWr], &data[pos], run);

        pos += run;
        s_upWr = ((s_upWr + run) < SM_RTT_UP_SIZE) ? (s_upWr + run) : 0U;
    }

    /* Make the data visible before advancing the write offset */
    __DMB();
    _SEGGER_RTT.aUp[0].WrOff = s_upWr;

    return done;
}

/*--------------------------------------------------------------------------*/
/* Write to the up ring, dropping whatever does not fit                     */
/*--------------------------------------------------------------------------*/
static void UpWrite(const uint8_t *data, uint32_t len)
{
    uint32_t rdOff = HostOff(&_SEGGER_RTT.aUp[0].RdOff, SM_RTT_UP_SIZE);
    uint32_t written;

    /* Host progress since the last write is what licenses the wait */
    if (rdOff != s_upRdSeen)
    {
        s_hostReads = true;
    }

    written = UpCopy(data, len);

    /*
     * Full. Give a host that is known to be reading one bounded chance
     * to make room, but only while the tick is refreshing the watchdog.
     */
    if ((written < len) && s_hostReads && TickRunning())
    {
        uint64_t start = DEV_SM_Usec64Get();

        while ((HostOff(&_SEGGER_RTT.aUp[0].RdOff, SM_RTT_UP_SIZE) == rdOff) &&
            ((DEV_SM_Usec64Get() - start) < SM_RTT_DRAIN_USEC))
        {
            ; /* Intentional empty while, waiting on the host */
        }

        (void) UpCopy(&data[written], len - written);
    }

    s_upRdSeen = HostOff(&_SEGGER_RTT.aUp[0].RdOff, SM_RTT_UP_SIZE);
}

/*--------------------------------------------------------------------------*/
/* Take one byte from the down ring if the host has written one             */
/*--------------------------------------------------------------------------*/
static bool DownRead(uint8_t *data)
{
    uint32_t wrOff;
    bool got;

    Init();

    wrOff = HostOff(&_SEGGER_RTT.aDown[0].WrOff, SM_RTT_DOWN_SIZE);
    got = (wrOff != s_downRd);

    if (got)
    {
        *data = s_downBuffer[s_downRd];
        s_downRd = ((s_downRd + 1U) < SM_RTT_DOWN_SIZE) ? (s_downRd + 1U) : 0U;

        /* Release the space back to the host after consuming it */
        __DMB();
        _SEGGER_RTT.aDown[0].RdOff = s_downRd;
    }

    return got;
}

/* Global Functions */

/*--------------------------------------------------------------------------*/
/* Console write (replaces LPUART_WriteBlocking)                            */
/*--------------------------------------------------------------------------*/
void SM_RTT_ConsoleWrite(const uint8_t *data, uint32_t len)
{
    static const uint8_t crLf[2] = { '\r', '\n' };
    uint32_t pos = 0U;

    Init();

    /*
     * Pass whole runs rather than one byte at a time. The drain budget
     * is per call, so writing byte by byte would multiply it by the
     * length of the output.
     */
    while ((data != NULL) && (pos < len))
    {
        uint32_t run = 0U;

        /* Longest run with no newline in it */
        while (((pos + run) < len) && (data[pos + run] != (uint8_t) '\n'))
        {
            run++;
        }

        if (run > 0U)
        {
            UpWrite(&data[pos], run);
            pos += run;
        }
        else
        {
            /* Expand LF to CR/LF, as the UART console does */
            UpWrite(crLf, (uint32_t) sizeof(crLf));
            pos++;
        }
    }
}

/*--------------------------------------------------------------------------*/
/* Console read (replaces LPUART_ReadBlocking)                              */
/*--------------------------------------------------------------------------*/
void SM_RTT_ConsoleRead(uint8_t *data, uint32_t len)
{
    /*
     * Structured like LPUART_ReadBlocking(): for each byte, spin until
     * it arrives, then take it. The spin must not use WFI. On this SoC a
     * WFI commits the CPU to the sleep mode armed in the GPC, which
     * stops the SysTick, so nothing refreshes the watchdog and WDOG2
     * fires. Every WFI in the SM is preceded by a CPU_SleepModeSet() to
     * choose that mode deliberately, and a console has no business
     * re-arming it. Spinning keeps the core in RUN with interrupts taken
     * normally, so the tick keeps refreshing the watchdog.
     */
    for (uint32_t i = 0U; (data != NULL) && (i < len); i++)
    {
        while (!DownRead(&data[i]))
        {
            ; /* Intentional empty while, waiting on the host */
        }

        /* Translate CR to LF, as the UART console does */
        if (data[i] == (uint8_t) '\r')
        {
            data[i] = (uint8_t) '\n';
        }
    }
}

/*--------------------------------------------------------------------------*/
/* Console character pending (replaces LPUART_GetStatusFlags check)         */
/*--------------------------------------------------------------------------*/
bool SM_RTT_ConsoleCharPending(void)
{
    Init();

    return HostOff(&_SEGGER_RTT.aDown[0].WrOff, SM_RTT_DOWN_SIZE)
        != s_downRd;
}

/*==========================================================================*/
