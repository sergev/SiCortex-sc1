#include "sc1_defs.h"
#include "sc1_scdma.h"
#include "sc1_cac.h"
#include <unistd.h>
#include "sicortex/simdma.h"
#include "simdma_internal.h"

#define SCDMA_POLL_INTVL 1000


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
    { HRDATA(write_count, simdma_context.io_write_count, 64) },
    { HRDATA(write_addr, simdma_context.io_write_addr, 64) },
    { HRDATA(write_data, simdma_context.io_write_data, 64) },
    { HRDATA(read_count, simdma_context.io_read_count, 64) },
    { HRDATA(read_addr, simdma_context.io_read_addr, 64) },
    { HRDATA(packet_for_bad_process, simdma_context.packet_for_bad_process, 64) },
    { HRDATA(packet_for_disabled_process, simdma_context.packet_for_disabled_process, 64) },
    { HRDATA(bad_routeindex, simdma_context.bad_routeindex, 64) },
    { HRDATA(bad_command, simdma_context.bad_command, 64) },
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
    int len;
    if (lnt == L_HALF) len = 4;
    else {
	if (lnt == L_DOUB) len = 8;
	else return FALSE;
    }
    /* 32-bit align */
    pa >>= 2;
    pa <<= 2;

    if( simdma_io_read( pa, (uint32_t *) val, len ) == SIMDMA_SUCCESS ) {
        return TRUE;
    }

    return FALSE;
}

t_bool
scdma_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
    int len;
    t_uint64 *vp = &val;
    if (lnt == L_HALF) len = 4;
    else {
	if (lnt == L_DOUB) len = 8;
	else return FALSE;
    }
    if(lnt != L_DOUB ) return FALSE;

    /* 32-bit align */
    pa >>= 2;
    pa <<= 2;

    if( simdma_io_write( pa, (uint32_t *) vp, len ) == SIMDMA_SUCCESS ) {
        return TRUE;
    }

    return FALSE;
}

/*
 *      SCDMA reset
 */

t_stat
scdma_reset(DEVICE *dptr)
{
  simdma_initialize();
  sim_activate(dptr->units, SCDMA_POLL_INTVL);
  return SCPE_OK;
}

/*
 *      SCDMA Recieve service
 */

t_stat
scdma_rcv_svc( UNIT *uptr )
{
    simdma_progress();

    sim_activate(uptr, SCDMA_POLL_INTVL);
    return SCPE_OK;
}


/*
 * For simdma to call to read/write the M array 
 */

/* simdma_link.c
 *
 * Revision $Id: sc1_scdma.c 24851 2006-08-30 21:35:13Z gingold $
 *
 * Contains implementation of simdma_phys_read and write
 * for the case simdma is linked with the application
 */

#include <string.h>
#include <assert.h>


/* cases
 *
 * source and dest are 64 bit aligned, length is 64-bit aligned
 *         copy in 64 bit units
 * other cases: copy in 32 bit units
 */
int simdma_phys_read(uint32_t *addr, uint32_t *data, int length)
{
    t_uint64 val;
    if (length == 0) return (SIMDMA_SUCCESS);
    assert(length >= 4);
    assert(((uintptr_t) addr & 3) == 0);
    assert(((uintptr_t) data & 3) == 0);

    if ((((uintptr_t) addr & 7) == 0)
	&& (((uintptr_t) data & 7) == 0)
	&& ((length & 7) == 0)) {

	while (length > 0) {
	    CALL_READPD(cpu_ctx[0], (uintptr_t) addr, (t_uint64*) data, 0);
	    addr += 2;
	    data += 2;
	    length -= 8;
	}
    } else {
	while (length > 0) {
	    CALL_READPW(cpu_ctx[0], (uintptr_t) addr, &val, 0);
	    *data = val;
	    addr += 1;
	    data += 1;
	    length -= 4;
	}
    }
  return(SIMDMA_SUCCESS);
}


int simdma_phys_write(uint32_t *addr, uint32_t *data, int length)
{
    if (length == 0) return (SIMDMA_SUCCESS);
    assert(length >= 4);
    assert(((uintptr_t) addr & 3) == 0);
    assert(((uintptr_t) data & 3) == 0);

    if ((((uintptr_t) addr & 7) == 0)
	&& (((uintptr_t) data & 7) == 0)
	&& ((length & 7) == 0)) {

	while (length > 0) {
	    CALL_WRITEPD(cpu_ctx[0], (uintptr_t) addr, *((uint64_t *) data), 0);
	    addr += 2;
	    data += 2;
	    length -= 8;
	}
    } else {
	while (length > 0) {
	    CALL_WRITEPW(cpu_ctx[0], (uintptr_t) addr, *data, 0);
	    addr += 1;
	    data += 1;
	    length -= 4;
	}
    }
  return(SIMDMA_SUCCESS);
  return(SIMDMA_SUCCESS);
}


int simdma_cause_interrupt(uint64_t value)
{
    cac_csw_int((value >> 12) & 15, (value >> 8) & 15, value & 0xff);
    return(0);
}
