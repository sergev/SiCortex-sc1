#include "sc1_defs.h"
#include "sc1_scdma.h"
#include "sc1_cac.h"
#include <unistd.h>

#include "CpuBeh.h"

#define SCDMA_POLL_INTVL 600000


/* Declarations */

t_bool scdma_rd( t_uint64 pa, t_uint64 *val, uint32 lnt );
t_bool scdma_wr( t_uint64 pa, t_uint64 val, uint32 lnt );
t_stat scdma_reset( DEVICE *dptr );

t_stat scdma_rcv_svc( UNIT *uptr );

extern CORECTX *cpu_ctx[NUM_CORES];

/* SCDMA data structures

   scdma_dib	SCDMA dib
   scdma_dev	SCDMA device descriptor
   scdma_unit	SCDMA unit descriptor
   scdma_reg	SCDMA register list
*/

DIB scdma_dib = { SCDMABASE, SCDMAEND, &scdma_rd, &scdma_wr, 0 };

UNIT scdma_unit[] = { 
    { UDATA(&scdma_rcv_svc, 0, SCDMASIZE) },
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
    if (lnt != L_DOUB) return FALSE;  /* 64-bit access only */

    /* printf("got scdma_rd - sending it to simx\n"); */
    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    uint64_t tmp;
    bool res = cpuBeh->read(pa&~7ULL, tmp);
    *val = tmp;
    return res;
}

t_bool
scdma_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
    if (lnt != L_DOUB) return FALSE;  /* 64-bit access only */

    /* printf("got scdma_wr - sending it to simx\n"); */
    CpuBeh* cpuBeh = ScxGlobal::accessCpuBeh(0);
    bool locked = false;
    return cpuBeh->write(pa&~7ULL, val, locked);
}

/*
 *      SCDMA reset
 */

t_stat
scdma_reset(DEVICE *dptr)
{
  sim_activate(dptr->units, SCDMA_POLL_INTVL);
  return SCPE_OK;
}

/*
 *      SCDMA Recieve service
 */

t_stat
scdma_rcv_svc( UNIT *uptr )
{

    /* FIXME make simtime advance here! */
    /* wait(10,SC_NS); */
    extern void ScxStep(unsigned int count);

    ScxStep(10);

    sim_activate(uptr, SCDMA_POLL_INTVL);
    return SCPE_OK;
}
