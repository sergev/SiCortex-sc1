/* sc1_i2c.c: SC1 I2C/SPD simulator

   Copyright (c) 2005-2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

   Copyright (c) 1998-2005, Robert M Supnik

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

   This module contains the I2C controller and the two serial presence detect
   EEROMs.

   i2c          I2C controller/SPD EEPROM
*/

#include "sc1_defs.h"
#include "sc1_i2c.h"

#define AP      u3                                      /* current addr pointer */

uint32 i2c_state = I2C_IDLE;                            /* state */
uint32 i2c_presch = 0;                                  /* prescale high */
uint32 i2c_prescl = 0;                                  /* prescale low */
uint32 i2c_rcv = 0;                                     /* receive buffer */
uint32 i2c_xmt = 0;                                     /* transmit buffer */
uint32 i2c_ctl = 0;                                     /* control reg */
uint32 i2c_sta = 0;                                     /* status reg */
uint32 i2c_cmd = 0;                                     /* command reg */
uint32 i2c_slvsel = 0;                                  /* slave select */
uint32 i2c_time = 10;                                   /* delay */

t_bool i2c_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool i2c_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat i2c_svc (UNIT *uptr);
t_stat i2c_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat i2c_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat i2c_reset (DEVICE *dptr);
t_stat i2c_attach (UNIT *uptr, char *cptr);
t_stat i2c_detach (UNIT *uptr);
uint32 i2c_map_sel (uint32 addr);

/* I2C data structures

   i2c_dev      I2C device descriptor
   i2c_unit     I2C units
   i2c_reg      I2C register list
*/

DIB i2c_dib = { I2C_BASE, I2C_END, &i2c_rd, &i2c_wr, 0 };

UNIT i2c_unit[] = {
    { UDATA (&i2c_svc, UNIT_FIX+UNIT_SWP, I2C_SIZE) },
    { UDATA (&i2c_svc, UNIT_FIX+UNIT_SWP, I2C_SIZE) }
    };

REG i2c_reg[] = {
    { HRDATA (MODE, i2c_state, 3) },
    { HRDATA (PRESCH, i2c_presch, 8) },
    { HRDATA (PRESCL, i2c_prescl, 8) },
    { HRDATA (CTL, i2c_ctl, 8) },
    { HRDATA (RCV, i2c_rcv, 8) },
    { HRDATA (XMT, i2c_xmt, 8) },
    { HRDATA (STA, i2c_sta, 8) },
    { HRDATA (CMD, i2c_cmd, 8) },
    { HRDATA (SLVSEL, i2c_slvsel, 8) },
    { URDATA (AP, i2c_unit[0].AP, 16, 8, 0, I2C_NUMUNIT, 0) },
    { DRDATA (TIME, i2c_time, 24), REG_NZ },
    { NULL }
    };

MTAB i2c_mod[] = {
    { UNIT_SWP, 0, "write protected", "NOSWP", NULL },
    { UNIT_SWP, UNIT_SWP, "write enabled", "SWP", NULL },
    { 0 }
    };

DEVICE i2c_dev = {
    "I2C", i2c_unit, i2c_reg, i2c_mod,
    I2C_NUMUNIT, 16, 8, 1, 16, 8,
    &i2c_ex, &i2c_dep, &i2c_reset,
    NULL, &i2c_attach, &i2c_detach,
    &i2c_dib, DEV_DIB
    };

/* I2C: access SPD EEPROM on memories */

t_bool i2c_rd (t_uint64 pa, t_uint64 *val, uint32 lnt)
{
uint32 off = (uint32) SC1_REG_OFF (pa);

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: I2C: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
switch (off) {                                          /* case on reg */

    case I2C_PRESCH:
        *val = i2c_presch;
        break;

    case I2C_PRESCL:
        *val = i2c_prescl;
        break;

    case I2C_CTL:
        *val = i2c_ctl & I2C_CTL_RW;
        break;

    case I2C_RCV:
        *val = i2c_rcv;
        break;

    case I2C_STA:
        *val = i2c_sta & I2C_STA_RD;
        break;

    default:
        return FALSE;
        }

return TRUE;
}

t_bool i2c_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
uint32 off = (uint32) SC1_REG_OFF (pa);
uint32 i, t, v8 = (uint32) val & 0xFF;

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: I2C: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
switch (off) {                                          /* case on reg */

    case I2C_PRESCH:
        i2c_presch = v8;
        break;

    case I2C_PRESCL:
        i2c_prescl = v8;
        break;

    case I2C_CTL:
        i2c_ctl = v8 & I2C_CTL_RW;
        break;

    case I2C_XMT:
        i2c_xmt = v8;
        break;

    case I2C_CMD:                                       /* new command */
        if ((i2c_ctl & I2C_CTL_ENB) == 0) break;        /* must be enabled */
        i2c_cmd = v8;
        if (((v8 & I2C_CMD_START) &&                    /* validate cmd */
             ((v8 & (I2C_CMD_RD|I2C_CMD_WR|I2C_CMD_STOP)) != I2C_CMD_WR)) ||
            ((v8 & I2C_CMD_RD) && (v8 & I2C_CMD_WR))) {
            fprintf (stderr, "%%Error: I2C: invalid command %X\n", v8);
            return FALSE;
            }
        if ((v8 & I2C_CMD_RD) && (i2c_state != I2C_READ)) {
            fprintf (stderr, "%%Error: I2C: read select invalid state %X\n", i2c_state);
            return FALSE;
            }
        if ((v8 & I2C_CMD_WR) && (i2c_state == I2C_READ)) {
            fprintf (stderr, "%%Error: I2C: write select invalid state %X\n", i2c_state);
            return FALSE;
            }
        if (v8 & (I2C_CMD_START|I2C_CMD_RD|I2C_CMD_WR)) { /* new transfer? */
            if (v8 & I2C_CMD_START) {                   /* start */
                i2c_slvsel = i2c_xmt;                   /* save addr, r/w */
                i2c_sta |= I2C_STA_BUSY;                /* I2C is busy */
                i2c_state = I2C_IDLE;
                }
           if ((t = i2c_map_sel (i2c_slvsel)) == 0) {   /* invalid unit? */
                fprintf (stderr, "%%Error: I2C: invalid slave select %X\n", i2c_slvsel);
                return FALSE;
                }
            if (i2c_sta & I2C_STA_TIP) {                /* already transferring? */
                fprintf (stderr, "%%Error: I2C: transfer while busy %X\n", i2c_cmd);
                return FALSE;
                }
            sim_activate (&i2c_unit[t - 1], i2c_time);  /* schedule transfer */
            i2c_sta |= I2C_STA_TIP;                     /* set transfer in prog */
            }
        else if (v8 & I2C_CMD_STOP) {                   /* stop */
            for (i = 0; i < I2C_NUMUNIT; i++)           /* cancel transfer */
                sim_cancel (&i2c_unit[i]);
            i2c_sta &= ~I2C_STA_BUSY;                   /* I2C is not busy */
            i2c_state = I2C_IDLE;                       /* state is idle */
            }
        if (v8 & I2C_CMD_IACK) i2c_sta &= ~I2C_STA_INT; /* iack? clear intr */
        break;

    default:
        return FALSE;
        }

return TRUE;
}

/* Timeout */

t_stat i2c_svc (UNIT *uptr)
{
uint8 *buf = (uint8 *) uptr->filebuf;

switch (i2c_state) {                                    /* case on state */

    case I2C_IDLE:                                      /* from start */
        if (i2c_slvsel & I2C_SEL_R)                     /* next is read or */
            i2c_state = I2C_READ;                       /* write addr */
        else i2c_state = I2C_WRADR;
        break;

    case I2C_READ:                                      /* read */
        i2c_rcv = buf[uptr->AP] & 0xFF;                 /* return data byte */
        if (++uptr->AP >= I2C_SIZE) uptr->AP = 0;       /* adv addr pointer */
        if (i2c_cmd & I2C_CMD_NACK)                     /* NACK set? */
            i2c_state = I2C_IDLE;                       /* read done */
        break;

    case I2C_WRADR:                                     /* write addr */
        uptr->AP = i2c_xmt & 0xFF;                      /* set addr pointer */
        i2c_state = I2C_WRDAT1;                         /* next is 1st data */
        i2c_sta &= ~I2C_STA_NACK;                       /* clear NACK */
        break;

    case I2C_WRDATN:                                    /* write nth data */
        if ((uptr->AP & (I2C_PSIZE - 1)) == 0) {        /* page size wrap? */
            fprintf (stderr, "%%Error: I2C: page write across boundary %X\n", uptr->AP);
            i2c_sta |= I2C_STA_NACK;
            if (++uptr->AP > I2C_SIZE) uptr->AP = 0;
            break;
            }
    case I2C_WRDAT1:                                    /* write first data */
        if ((uptr->flags & UNIT_SWP) &&                 /* protection enabled and */
            (uptr->AP < I2C_PROT_LIM)) {                /* protected region? */
            fprintf (stderr, "%%Error: I2C: write below protection boundary %X\n", uptr->AP);
            i2c_sta |= I2C_STA_NACK;
            }
        else {
            buf[uptr->AP] = i2c_xmt & 0xFF;
            i2c_sta &= ~I2C_STA_NACK;
            }
        if (++uptr->AP > I2C_SIZE) uptr->AP = 0;        /* adv addr pointer */
        i2c_state = I2C_WRDATN;
	    break;

    default:
        return SCPE_IERR;
        }

i2c_sta = (i2c_sta & ~I2C_STA_TIP) | I2C_STA_INT;       /* clear tip, set intr */
return SCPE_OK;
}

/* Map select to unit number (+1), return 0 if invalid */

uint32 i2c_map_sel (uint32 sel)
{
if (I2C_SEL_GETTYPE (sel) != I2C_SEL_MEM) return 0;
sel = I2C_SEL_GETDIMM (sel);
if (sel >= I2C_NUMUNIT) return 0;
return sel + 1;
}

/* I2C examine */

t_stat i2c_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;
uint8 *buf = (uint8 *) uptr->filebuf;

if (vptr == NULL) return SCPE_ARG;
if (addr >= I2C_SIZE) return SCPE_NXM;
*vptr = buf[addr];
return SCPE_OK;
}

/* I2C deposit */

t_stat i2c_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;
uint8 *buf = (uint8 *) uptr->filebuf;

if (addr >= I2C_SIZE) return SCPE_NXM;
buf[addr] = (uint8) val;
return SCPE_OK;
}

/* I2C reset */

t_stat i2c_reset (DEVICE *dptr)
{
uint32 i;

for (i = 0; i < I2C_NUMUNIT; i++) {
    UNIT *uptr = dptr->units + i;
    uptr->AP = 0;
    sim_cancel (uptr);
    if (uptr->filebuf == NULL)
        uptr->filebuf = (uint8 *) calloc (I2C_SIZE, sizeof (uint8));
    if (uptr->filebuf == NULL) return SCPE_MEM;
    }
i2c_cmd = 0;
i2c_ctl = 0;
i2c_xmt = 0;
i2c_rcv = 0;
i2c_sta = 0;
i2c_slvsel = 0;
i2c_state = I2C_IDLE;
return SCPE_OK;
}

/* I2C attach */

t_stat i2c_attach (UNIT *uptr, char *cptr)
{
t_stat r;

uptr->flags = uptr->flags | (UNIT_ATTABLE | UNIT_BUFABLE);
r = attach_unit (uptr, cptr);
if (r != SCPE_OK)
    uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
else uptr->hwmark = (uint32) uptr->capac;
return r;
}

/* I2C detach */

t_stat i2c_detach (UNIT *uptr)
{
t_stat r = SCPE_OK;

if (uptr->flags & UNIT_ATT) r = detach_unit (uptr);
if ((uptr->flags & UNIT_ATT) == 0)
    uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
return r;
}
