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
/* Board configuration overlay for the RTT console.                         */
/*                                                                          */
/* Only on the include path when the RTT console is built, where it shadows  */
/* the generated configs/<config>/config_board.h. Pulls in the real header   */
/* and then reassigns the debug UART to instance 0, the existing encoding    */
/* for "no console UART": index 0 of LPUART_BASE_PTRS is a NULL base and     */
/* LPUART_RX_TX_IRQS is NotAvail_IRQn, as used by the simu config. The board */
/* code then skips the UART on its own, so no board file needs to know which */
/* console transport was selected.                                          */
/*                                                                          */
/* The generated header cannot be edited, configtool.pl rewrites it, and a   */
/* -D on the command line would collide with its #define.                   */
/*==========================================================================*/

#ifndef SM_RTT_CONFIG_BOARD_H
#define SM_RTT_CONFIG_BOARD_H

/* Pull in the generated board configuration this file shadows */
#include_next "config_board.h"

/* Reassign the debug console UART to the "not present" instance */
#undef BOARD_DEBUG_UART_INSTANCE
#define BOARD_DEBUG_UART_INSTANCE   0U

#endif /* SM_RTT_CONFIG_BOARD_H */
