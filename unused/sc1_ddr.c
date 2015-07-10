/* sc1_ddr.c - DDR Memory Controller (Ddr)

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
#include "sc1_ddr.h"
#include <assert.h>

/* Declarations */

static t_bool ddr_rd( t_uint64 pa, t_uint64 *val, uint32 unit );
static t_bool ddr_wr( t_uint64 pa, t_uint64  val, uint32 unit );
static t_stat ddr_reset( DEVICE *dptr );

/*
 * We set default dimmsize to be illegally large because even the smallest DIMM
 * at 1 GB is too large for fast simulations, given current implementation of 
 * simh memory. But the register exists and hence the same kernel can run on simh
 * and actual hw.
 */
static uint32 dimmsize    = ICE9_DdrxDIMMSize_DIMMSize_MASK+1;

DIB ddr_dib = { DDR_BASE, DDR_END, &ddr_rd, &ddr_wr, 0 };

UNIT ddr_unit[] = { 
    { UDATA(NULL, 0, DDR_SIZE) },
};

REG ddr_reg[] = {
    { HRDATA(DdrxDIMMSize, dimmsize, 32) },
    { NULL }
};

DEVICE ddr_dev = {
    "DDR",              /* name */
    ddr_unit,           /* units */
    ddr_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &ddr_reset,         /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *) &ddr_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};


static t_bool ddr_rd(t_uint64 pa, t_uint64 *val, uint32 unit)
{
    if( PA_IS_DDR(pa) ) {
	switch (pa) {
	case ICE9_RBASEA_Ddr0+ICE9_RA_DdrxDIMMSize:
	case ICE9_RBASEA_Ddr1+ICE9_RA_DdrxDIMMSize:
            *val = (t_uint64) dimmsize;
	    break;
        default:
	    printf("\r\nSIMH : read unknown DDR register: %#016llx (%d)\r\n", pa, unit);
	}
    }

    return TRUE;
}

static t_bool ddr_wr(t_uint64 pa, t_uint64 idata, uint32 unit)
{
    if( PA_IS_DDR(pa) ) {
	switch (pa) {
	default:
	    printf("\r\nSIMH : write unknown DDR register: %#016llx <= %#016llx (%d)\n\r", pa, idata, unit);
	}
    }

    return TRUE;
}

static t_stat ddr_reset(DEVICE *dptr)
{
    return SCPE_OK;
}

