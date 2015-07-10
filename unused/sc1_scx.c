/* sc1_scx.c - Pseudo Device to Control System-C Model (SCX)

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
#include "sc1_scx.h"
#include <assert.h>

#define SCX_RATIO_DEFAULT	2000
#define SCX_CYCLES_DEFAULT	50

/* Interface with SCX simulator */
#ifdef SIMH_CPUSIMH
extern void ScxStep(unsigned int);
extern t_uint64 backdoorUntil;
void setSimhMemModel (int newModel);
int getSimhMemModel ();
void setSimhRatio (uint64_t newRatio);
uint64_t getSimhRatio ();
void setEpochCycles (uint64_t newCycles);
uint64_t getEpochCycles ();
#endif

extern t_uint64 total_count;

/* Declarations */

static t_bool scx_rd( t_uint64 pa, t_uint64 *val, uint32 unit );
static t_bool scx_wr( t_uint64 pa, t_uint64 val, uint32 unit );
static t_stat scx_reset( DEVICE *dptr );
static t_stat scx_rcv_svc (UNIT *uptr);


DIB scx_dib = { SCX_BASE, SCX_END, &scx_rd, &scx_wr, 0 };

UNIT scx_unit[] = { 
    { UDATA(&scx_rcv_svc, 0, 0) },
};

REG scx_reg[] = {
 { NULL }
};

DEVICE scx_dev = {
    "SCX",              /* name */
    scx_unit,           /* units */
    scx_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &scx_reset,         /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *) &scx_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};


static t_bool scx_rd( t_uint64 pa, t_uint64 *val, uint32 unit)
{
#ifdef SIMH_CPUSIMH

    if( PA_IS_SCX(pa) ) {
	switch (pa) {
	case ICE9_RA_SimhRatio:
            *val = (t_uint64) getSimhRatio ();
            printf("\r\nSIMH: read simh-to-simx ratio = %lld ", *val);
	    break;
	case ICE9_RA_SimhEpochCycles:
            *val = (t_uint64) getEpochCycles ();
            printf("\r\nSIMH: read R_SimhEpochCycles = %lld ", *val);
	    break;
	case ICE9_RA_SimhMemModel:
            *val = (t_uint64) getSimhMemModel ();
            printf("\r\nSIMH: read R_SimhMemModel = %lld ", *val);
	    break;
        default:
	    printf("\r\nSIMH : read unknown SIMH control register: %#016llx (%d)", pa, unit);
	}
    }

    printf("\r\n");

#endif
    return TRUE;
}

static t_bool scx_wr(t_uint64 pa, t_uint64 idata, uint32 unit)
{
#ifdef SIMH_CPUSIMH

    if( PA_IS_SCX(pa) ) {
	t_bool reschedule = FALSE;
	switch (pa) {
	case ICE9_RA_SimhRatio:
            setSimhRatio (idata);
            //printf("\r\nSIMH: wrote simh-to-simx ratio = %lld\n", idata);
	    reschedule = TRUE;
	    break;
	case ICE9_RA_SimhEpochCycles:
	    setEpochCycles (idata);
            //printf("\r\nSIMH: wrote epoch cycles = %lld\n", idata);
	    reschedule = TRUE;
	    break;
	case ICE9_RA_SimhMemModel:
	    setSimhMemModel (idata);
            //printf("\r\nSIMH: wrote memModel = %lld\n", idata);
	    break;
	default:
	    printf("\r\nSIMH : unrecognized write to SIMH control register: %#016llx <= %#016llx (%d)", pa, idata, unit);
	}
	if (reschedule) {
	    sim_cancel(&scx_unit[0]);
	    sim_activate(&scx_unit[0], getSimhRatio() * getEpochCycles());
	}
    }

    printf("\r\n");

#endif
    return TRUE;
}

static t_stat scx_reset(DEVICE *dptr)
{
#ifdef SIMH_CPUSIMH
    /*printf("\r\nSIMH: SCX : at reset, total count %lld, simhRatio=%lld, memModel %d", 
	   total_count, getSimhRatio(), (int)getSimhMemModel ()); */
    sim_activate(dptr->units, getSimhRatio() * getEpochCycles());
#endif
    return SCPE_OK;
}

t_stat scx_rcv_svc (UNIT *uptr)
{
#ifdef SIMH_CPUSIMH
    uint64_t cycles = getEpochCycles ();
    uint64_t ratio = getSimhRatio ();
    if (ratio && cycles) {
	STATS_SCX_BEGIN_EPOCH(cycles);
	ScxStep(cycles);
	STATS_SCX_END_EPOCH(cycles);

	// clear the EVT_STALL_EPOCH bit for every cpu.
	// This is part of the simhLLStallCpu feature.
	for (int cpu=0; cpu<NUM_CORES; cpu++) {
	    cpu_ctx[cpu]->events &= ~EVT_STALL_EPOCH;
	}
    }
    /* printf("\r\nSIMH : SCX : svc ratio %d total count %lld", ratio, total_count); */
    sim_activate(uptr, ratio * cycles);
#endif
    return SCPE_OK;
}
