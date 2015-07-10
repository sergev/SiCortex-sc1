/* sc1_uart.h - SiCortex 1 UART simulator definitions

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

   Copyright (c) 2003-2005, Robert M Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   uart         16550 compatible UART

   05-Dec-05    RMS     Revised UART base address
   09-Sep-05    RMS     IntID<7:6> are MBO; added timeout interrupt
   29-Aug-05    RMS     Added DLAB bit
*/

#ifndef _SC1_UART_H_
#define _SC1_UART_H_    0

#define UART_FIFO_SIZE          16
#define UART_FIFO_WAIT          5

#define UART_BASE               SIM_ULL(0xEB8000000)
#define UART_END                (UART_BASE + 63)

#define UART_RCV_TMO_CNT        4

/* Receive buffer (read only) */

#define UART_RBUF               0

/* Transmit hold register (write only) */

#define UART_XBUF               0

/* Interrupt enables (read/write) and interrupt requests (implicit) */

#define UART_IE                 1
#define UART_IE_RW              0x0F

#define UART_ISTA_RCV           0x01
#define UART_ISTA_XMT           0x02
#define UART_ISTA_RSTA          0x04
#define UART_ISTA_MODEM         0x08
#define UART_ISTA_TMO           0x10

/* Interrupt ID (read only) */

#define UART_IID                2
#define UART_IID_NOINTR         0x01
#define UART_IID_MODEM          0x00
#define UART_IID_XMT            0x02
#define UART_IID_RCV            0x04
#define UART_IID_RSTA           0x06
#define UART_IID_TMO            0x0C
#define UART_IID_MBO            0xC0

/* FIFO control (write only) */

#define UART_FCTL               2
#define UART_FCTL_RRST          0x02
#define UART_FCTL_XRST          0x04
#define UART_FCTL_V_ALM         6
#define UART_FCTL_M_ALM         0x03
#define UART_FCTL_ALARM(x)      (((x) >> UART_FCTL_V_ALM) & UART_FCTL_M_ALM)

/* Line control (read/write) */

#define UART_LCTL               3
#define UART_LCTL_PE            0x08
#define UART_LCTL_V_PAR         4
#define UART_LCTL_M_PAR         0x03
#define UART_LCTL_DLAB         0x80
#define UART_LCTL_PAR(x)        (((x) >> UART_LCTL_V_PAR) & UART_LCTL_M_PAR)
#define UART_LCTL_DEFAULT       0x03

/* Modem control (write only) - not implemented */

#define UART_MCTL               4

/* Line status (read only) */

#define UART_LSTA               5
#define UART_LSTA_DRDY          0x01
#define UART_LSTA_OE            0x02
#define UART_LSTA_PE            0x04
#define UART_LSTA_FE            0x08
#define UART_LSTA_BRK           0x10
#define UART_LSTA_FEMP          0x20
#define UART_LSTA_XEMP          0x40
#define UART_LSTA_ERR           0x80
#define UART_LSTA_DYN           (UART_LSTA_DRDY|UART_LSTA_FEMP|UART_LSTA_XEMP|UART_LSTA_ERR)

/* Modem status (read only) - not implemented */

#define UART_MSTA               6

#define UART_MSTA_VAL           0xB0

/* FIFO error bits */

#define UART_RCV_BRK            0x0100
#define UART_RCV_PE             0x0200

#endif
