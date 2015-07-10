/* sc1_scb.c - Serial Control Bus (SCB)

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

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

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

*/

#include "sc1_defs.h"
#include "sc1_scb.h"
#include <assert.h>

#ifdef USE_HLMDMA
#include "CpuBeh.h"
#endif

/* Declarations */

static t_bool scb_rd(t_uint64 pa, t_uint64 *val, uint32 lnt);
static t_bool scb_wr(t_uint64 pa, t_uint64 val, uint32 lnt);
static t_stat scb_reset( DEVICE *dptr );

static uint32 chiprev     = SCB_CHIPREV;
static uint32 chipnum     = 0;
static uint32 chipmissing = 0;
static uint32 chipspeed   = 0;
static uint32 gpio        = 0;
static uint32 led         = 0;
static uint32 atnchip     = 0;

DIB scb_dib = { SCB_BASE, SCB_END, &scb_rd, &scb_wr, 0 };

UNIT scb_unit[] = { 
    { UDATA(NULL, 0, SCB_SIZE) },
};

REG scb_reg[] = {
    { HRDATA(ScbChipRev, chiprev, 32) },
    { HRDATA(ScbChipNum, chipnum, 32) },
    { HRDATA(ScbChipMissing, chipmissing, 32) },
    { HRDATA(ScbChipSpeed, chipspeed, 32) },
    { HRDATA(ScbGpio, gpio, 32) },
    { HRDATA(ScbLed, led, 32) },
    { HRDATA(ScbAtnChip, atnchip, 32) },
    { NULL }
};

DEVICE scb_dev = {
    "SCB",              /* name */
    scb_unit,           /* units */
    scb_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &scb_reset,         /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *) &scb_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};


static t_bool scb_rd(t_uint64 pa, t_uint64 *val, uint32 lnt)
{
#ifdef USE_HLMDMA
    t_uint64 tmp;
    uint8  mask;
    uint32 sc;

    switch(lnt) {
    case L_BYTE:
        mask = 0x1 << (pa & 0x7);
        break;
    case L_HALF:
        mask = 0x3 << (pa & 0x6);
        break;
    case L_WORD:
        mask = 0xf << (pa & 0x4);
        break;
    case L_DOUB:
        mask = 0xff;
        break;
    default:
        return FALSE;
    }

    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    if(!cpuBeh->read(pa & ~0x7ULL, tmp, false, mask, false)) return FALSE;

    switch(lnt) {
    case L_BYTE:
        sc = (pa & 0x7) << 3;
        *val = (tmp >> sc) & M8;
        break;
    case L_HALF:
        sc = (pa & 0x6) << 3;
        *val = (tmp >> sc) & M16;
        break;
    case L_WORD:
        sc = (pa & 0x4) << 3;
        *val = (tmp >> sc) & M32;
    case L_DOUB:
        *val = tmp;
        break;
    }
    return TRUE;

#else

    if( PA_IS_SCB(pa) ) {
	switch (pa) {
	case ICE9_RA_ScbChipRev:
            *val = (t_uint64) chiprev;
	    break;
	case ICE9_RA_ScbChipNum:
            *val = (t_uint64) chipnum;
	    break;
	case ICE9_RA_ScbChipMissing:
            *val = (t_uint64) chipmissing;
	    break;
	case ICE9_RA_ScbSpeed:
            *val = (t_uint64) chipspeed;
	    break;
        default:
	    printf("\r\nSIMH : read unknown SCB register: %#016llx (%d)\r\n", pa, lnt);
	}
    }

    return TRUE;
#endif
}

static t_bool scb_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
#ifdef USE_HLMDMA
    uint8 mask;

    switch(lnt) {
    case L_BYTE:
        mask = 0x1 << (pa & 0x7);
        val <<= ((pa & 0x7) << 3);
        break;
    case L_HALF:
        mask = 0x3 << (pa & 0x6);
        val <<= ((pa & 0x6) << 3);
        break;
    case L_WORD:
        mask = 0xf << (pa & 0x4);
        val <<= ((pa & 0x4) << 3);
        break;
    case L_DOUB:
        mask = 0xff;
        break;
    default:
        return FALSE;

    }

    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    bool locked = false;

    if(!cpuBeh->write(pa&~7ULL, val, locked, mask, false)) return FALSE;

    return TRUE;

#else
    if( PA_IS_SCB(pa) ) {
	switch (pa) {
	case ICE9_RA_ScbChipNum:
            chipnum = (uint32) val;
	    break;
	default:
	    printf("\r\nSIMH : write unknown SCB register: %#016llx <= %#016llx (%d)\n\r", pa, val, lnt);
	}
    }

    return TRUE;
#endif
}

static t_stat scb_reset(DEVICE *dptr)
{
    return SCPE_OK;
}

