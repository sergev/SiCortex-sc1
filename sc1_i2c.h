/* sc1_i2c.h - SiCortex 1 I2C/SPD EEPROM simulator definitions

   Copyright (c) 2006, SiCortex, Inc.  All rights reserved.

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
*/

#ifndef _SC1_I2C_H_
#define _SC1_I2C_H_             0

#define I2C_SIZE                256                     /* size of EEPROM */
#define I2C_PROT_LIM            128                     /* size of WP region */
#define I2C_PSIZE               16                      /* page size */
#define I2C_NUMUNIT             2                       /* number of DIMMs */
#define I2C_BASE                SIM_ULL(0xEA8000000)
#define I2C_END                 (I2C_BASE + 63)
#define UNIT_SWP                (1u << UNIT_V_UF)

/* States */

#define I2C_IDLE                0                       /* idle */
#define I2C_READ                1                       /* reading data */
#define I2C_WRADR               2                       /* write will be addr */
#define I2C_WRDAT1              3                       /* write will be 1st data */
#define I2C_WRDATN              4                       /* write will be nth data */

/* Prescalers (read/write) */

#define I2C_PRESCH              0
#define I2C_PRESCL              1

/* Control (read/write) */

#define I2C_CTL                 2
#define I2C_CTL_ENB             0x80                    /* enable I2C */
#define I2C_CTL_IENB            0x40                    /* enable interrupt */
#define I2C_CTL_RW              0xC0

/* Receive (read only) */

#define I2C_RCV                 3

/* Transmit (write only) */

#define I2C_XMT                 3

/* Status (read only) */

#define I2C_STA                 4
#define I2C_STA_NACK            0x80                    /* NACK received */
#define I2C_STA_BUSY            0x40                    /* I2C busy */
#define I2C_STA_ARBL            0x20                    /* ARB lost - NI */
#define I2C_STA_TIP             0x02                    /* transfer in progress */
#define I2C_STA_INT             0x01                    /* interrupt */
#define I2C_STA_RD              0xC3

/* Command (write only) */

#define I2C_CMD                 4
#define I2C_CMD_START           0x80
#define I2C_CMD_STOP            0x40
#define I2C_CMD_RD              0x20
#define I2C_CMD_WR              0x10
#define I2C_CMD_NACK            0x08
#define I2C_CMD_IACK            0x01

/* Slave (address) select */

#define I2C_SEL_R               0x01                    /* read/~write */
#define I2C_SEL_GETDIMM(x)      (((x) >> 1) & 0x7)      /* DIMM select */
#define I2C_SEL_GETTYPE(x)      (((x) >> 4) & 0xF)      /* addr space select */
#define I2C_SEL_MEM             0xA
#define I2C_SEL_SWP             0x6

#endif
