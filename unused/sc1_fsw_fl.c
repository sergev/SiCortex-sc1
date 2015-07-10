

/*
 * fsw device
 *
 * Notes from a recent checkin:
 *
 * This checkin makes sc1_fsw_fl.c a permanent part of pure simh; it is no
 * longer used only for the USE_HLMDMA case. Sc1_fsw_fl's previous claiming
 * of scb addresses has been moved to sc1_scb.c. Sc1_scb.c handle's the 
 * USE_HLMDMA case just as fsw_fl did.
 *
 * This has been tested with cpu_simh, cpu_rtl (yes, it builds simh to use the
 * disassembler), and even the exotic cpu_simh_mpi (the HLMDMA case).
 *
 * Maybe this can be simplified by having all simx users screen addresses before
 * sending them to simh, i.e. addresses are sent to simh only if simh ultimately
 * handles that address.
 */


#include "sc1_defs.h"
#include "sc1_fsw_fl.h"
#include "sc1_cac.h"
#include <unistd.h>

#ifdef USE_HLMDMA
#include "CpuBeh.h"
#endif

#define DEBUG 0

/* Declarations */

t_bool fswfl_rd( t_uint64 pa, t_uint64 *val, uint32 lnt );
t_bool fswfl_wr( t_uint64 pa, t_uint64 val, uint32 lnt );
t_stat fswfl_reset( DEVICE *dptr );

extern CORECTX *cpu_ctx[NUM_CORES];

/* FSWFL data structures

   fsw_dib	FSW dib
   fsw_dev	FSW device descriptor
   fsw_unit	FSW unit descriptor
   fsw_reg	FSW register list
*/

DIB fsw_dib = { FSW_BASE, FSW_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flr0_dib = { FLR0_BASE, FLR0_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flr1_dib = { FLR1_BASE, FLR1_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flr2_dib = { FLR2_BASE, FLR2_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flt0_dib = { FLT0_BASE, FLT0_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flt1_dib = { FLT1_BASE, FLT1_END, &fswfl_rd, &fswfl_wr, 0 };
DIB flt2_dib = { FLT2_BASE, FLT2_END, &fswfl_rd, &fswfl_wr, 0 };
DIB qsc_dib =  { QSC_BASE, QSC_END, &fswfl_rd, &fswfl_wr, 0 };
DIB sdma_dib = { SDMA_BASE, SDMA_END, &fswfl_rd, &fswfl_wr, 0 };

UNIT fsw_unit[] = { 
    { UDATA(NULL, 0, FSW_SIZE) },
};

UNIT flr0_unit[] = { 
    { UDATA(NULL, 0, FLR0_SIZE) },
};
UNIT flr1_unit[] = { 
    { UDATA(NULL, 0, FLR1_SIZE) },
};
UNIT flr2_unit[] = { 
    { UDATA(NULL, 0, FLR2_SIZE) },
};

UNIT flt0_unit[] = { 
    { UDATA(NULL, 0, FLT0_SIZE) },
};
UNIT flt1_unit[] = { 
    { UDATA(NULL, 0, FLT1_SIZE) },
};
UNIT flt2_unit[] = { 
    { UDATA(NULL, 0, FLT2_SIZE) },
};

UNIT qsc_unit[] = { 
    { UDATA(NULL, 0, QSC_SIZE) },
};

UNIT sdma_unit[] = { 
    { UDATA(NULL, 0, SCB_SIZE) },
};

REG fswfl_reg[] =
{
    { NULL }
};


DEVICE fsw_dev = {
    "FSW",            /* name */
    fsw_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&fsw_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

DEVICE flr_dev[] = {
    {
    "FLR0",            /* name */
    flr0_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flr0_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    },
    {
    "FLR1",            /* name */
    flr1_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flr1_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    },
    {
    "FLR2",            /* name */
    flr2_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flr2_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    }
};

DEVICE flt_dev[] = {
    {
    "FLT0",            /* name */
    flt0_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flt0_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    },
    {
    "FLT1",            /* name */
    flt1_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flt1_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    },
    {
    "FLT2",            /* name */
    flt2_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&flt2_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
    }
};

DEVICE qsc_dev = {
    "QSC",            /* name */
    qsc_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&qsc_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

DEVICE sdma_dev = {
    "SDMA",            /* name */
    sdma_unit,         /* units */
    fswfl_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &fswfl_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&sdma_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

/* NOTE: these are the same as the read/write routines on sc1_dma.c;
 * They exist solely to get these addresses out to the hardware model
 */

t_bool
fswfl_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    t_uint64 tmp;
    uint8  mask;
    uint32 sc;

#if DEBUG
    printf("got fswfl_rd - lnt = %d\n",lnt);
#endif

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

#ifdef USE_HLMDMA
    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    if(!cpuBeh->read(pa & ~0x7ULL, tmp, false, mask, false)) return FALSE;
#else
   if( PA_IS_SDMA(pa) ) {
        switch (pa) {
        case ICE9_RA_SDmaEccMode:
            *val = (t_uint64) 0;
	    printf("\r\nSIMH: read from ICE9_RA_SDmaEccMode not implemented\n\r");
            break;
        case ICE9_RA_SDmaIntMask:
            *val = (t_uint64) 0;
	    printf("\r\nSIMH: read from ICE9_RA_SDmaIntMask not implemented\n\r");
            break;
        default:
            printf("\r\nSIMH : read unknown FSW reg: %#016llx (%d)\r\n", pa, lnt);
        }
    }
    return TRUE;
#endif

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
}

t_bool
fswfl_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
    uint8 mask;

#if DEBUG
    printf("got fswfl_wr - addr %llx lnt = %d\n", pa, lnt);
#endif

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

#ifdef USE_HLMDMA
    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    bool locked = false;
    if(!cpuBeh->write(pa&~7ULL, val, locked, mask, false)) return FALSE;
    return TRUE;
#else
    if ( PA_IS_SDMA(pa) ) {
	switch (pa) {
        case ICE9_RA_SDmaIntMask:
	    printf("\r\nSIMH: write to RA_SDmaIntMask not implemented\n\r");
	    return TRUE;
        case ICE9_RA_SDmaIntCause:
	    printf("\r\nSIMH: write to ICE9_RA_SDmaIntCause not implemented\n\r");
	    return TRUE;
        case ICE9_RA_SDmaEccMode:
	    printf("\r\nSIMH: write to ICE9_RA_SDmaEccMode not implemented\n\r");
	    return TRUE;
        case ICE9_RA_FswBypassEnable:
	    printf("\r\nSIMH: write to ICE9_RA_FswBypassEnable not implemented\n\r");
	    return TRUE;
        case ICE9_RA_FswIntMask:
        case ICE9_RA_FswIntMask+4:
        case ICE9_RA_FswIntMask+8:
        case ICE9_RA_FswIntMask+12:
	    printf("\r\nSIMH: write to RA_FswIntMask not implemented\n\r");
	    return TRUE;
	default:
	    printf("\r\nSIMH: ERROR : urecognized write FSW/SDMA/QSC\n\r");
	    break;
	}
    }
    return FALSE;
#endif
}

/*
 *      FSWFL reset
 */

t_stat
fswfl_reset(DEVICE *dptr)
{
#if DEBUG
    printf("got fswfl_reset\n");
#endif
    return SCPE_OK;
}
