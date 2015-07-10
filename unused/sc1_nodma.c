
/*
 * sc1_nodma.c -- emulate DMA dmem and imem as memory, but provide no
 *                other DMA functionality.
 */

#include "sc1_defs.h"
#include "sc1_nodma.h"

#include <unistd.h>

t_uint64 *scdma_mem = NULL;

/* Declarations */

t_bool scdma_rd( t_uint64 pa, t_uint64 *val, uint32 lnt );
t_bool scdma_wr( t_uint64 pa, t_uint64 val, uint32 lnt );
t_stat scdma_reset( DEVICE *dptr );


/* SCDMA data structures

   scdma_dib	SCDMA dib
   scdma_dev	SCDMA device descriptor
   scdma_unit	SCDMA unit descriptor
   scdma_reg	SCDMA register list
*/

DIB scdma_dib = { SCDMABASE, SCDMAEND, &scdma_rd, &scdma_wr, 0 };

UNIT scdma_unit[] = { 
    { UDATA(NULL, 0, SCDMASIZE) },
};

REG scdma_reg[] =
{
    { NULL }
};


DEVICE scdma_dev = {
    "SCDMA",            /* name */
    scdma_unit,         /* units */
    scdma_reg,          /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &scdma_reset,       /* reset routine */
    NULL,               /* boot routine */
    NULL,               /* attach routine */
    NULL,               /* detach routine */
    (void *)&scdma_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

t_bool
scdma_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    uint32 sc, rg;

    if(!PA_IS_SCDMA(pa)) return FALSE;

    rg = (pa - SCDMABASE) >> 3;
    
    switch (lnt) {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            *val = (scdma_mem[rg] >> sc) & M8;
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            *val = (scdma_mem[rg] >> sc) & M16;
            break;
        case L_WORD:
            if (pa & 4) *val = (scdma_mem[rg] >> 32) & M32;
            else *val = scdma_mem[rg] & M32;
            break;
        case L_DOUB:
            *val = scdma_mem[rg];
            break;
        default:
	    return FALSE;
    }
    return TRUE;
}

t_bool
scdma_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
    uint32 sc, rg;

    if(!PA_IS_SCDMA(pa)) return FALSE;

    rg = (pa - SCDMABASE) >> 3;

    switch (lnt) {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            scdma_mem[rg] = (scdma_mem[rg] & ~(((t_uint64) M8) << sc)) |
		(((t_uint64) (val & M8)) << sc);
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            scdma_mem[rg] = (scdma_mem[rg] & ~(((t_uint64) M16) << sc)) | 
		(((t_uint64) (val & M16)) << sc);
            break;
        case L_WORD:
            if (pa & 4) scdma_mem[rg] = ((t_uint64) (scdma_mem[rg] & M32)) |
			    (((t_uint64) (val & M32)) << 32);
            else scdma_mem[rg] = (scdma_mem[rg] & ~((t_uint64) M32)) |
		     ((t_uint64) val & M32);
            break;
        case L_DOUB:
            scdma_mem[rg] = val;
            break;
        default:
	    return FALSE;
    }

    return TRUE;
}

/*
 *      SCDMA reset
 */

t_stat
scdma_reset(DEVICE *dptr)
{
    if (scdma_mem == NULL) {
        scdma_mem = (t_uint64*) calloc (SCDMASIZE >> 3, sizeof (t_uint64));
    }
    if (scdma_mem == NULL) {
        return SCPE_MEM;
    }

    return SCPE_OK;
}
