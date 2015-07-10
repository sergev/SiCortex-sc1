/* sc1_nvr.c - SiCortex 1 cut-through flash simulator

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

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

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

*/

#include "sc1_defs.h"
#include "sc1_nvr.h"

t_uint64 *nvr = NULL;                                     /* non-volatile mem */


/* Declarations */

t_bool nvr_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool nvr_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat nvr_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvr_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvr_reset (DEVICE *dptr);
t_stat nvr_attach (UNIT *uptr, char *cptr);
t_stat nvr_detach (UNIT *uptr);

/* NVR data structures

   nvr_dib	NVR dib
   nvr_dev	NVR device descriptor
   nvr_unit	NVR unit descriptor
   nvr_reg	NVR register list
*/

DIB nvr_dib = { NVRBASE, NVRBASE+NVRSIZE, &nvr_rd, &nvr_wr, 0 };

UNIT nvr_unit = { UDATA (NULL,  UNIT_FIX+UNIT_BINK, NVRSIZE) };

REG nvr_reg[] = { { NULL }  };

DEVICE nvr_dev = {
    "NVR",              /* name */
    &nvr_unit,          /* units */
    nvr_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    NVRAWIDTH,          /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    &nvr_ex,            /* examine routine */
    &nvr_dep,           /* deposit routine */
    &nvr_reset,         /* reset routine */
    NULL,               /* boot routine */
    &nvr_attach,        /* attach routine */
    &nvr_detach,        /* detach routine */
    (void *) &nvr_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

/* NVR: non-volatile RAM - stored in a buffered file */

t_bool nvr_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    uint32 sc, rg = ((uint32) ((pa - NVRBASE) & NVRAMASK)) >> 3;

    switch (lnt) {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            *val = (nvr[rg] >> sc) & M8;
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            *val = (nvr[rg] >> sc) & M16;
            break;
        case L_WORD:
            if (pa & 4) *val = (nvr[rg] >> 32) & M32;
            else *val = nvr[rg] & M32;
            break;
        case L_DOUB:
            *val = nvr[rg];
            break;
    }

    return TRUE;
}

t_bool nvr_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
    uint32 sc, rg = ((uint32) ((pa - NVRBASE) & NVRAMASK)) >> 3;

    switch (lnt) {
        case L_BYTE:
            sc = (((uint32) pa) & 7) * 8;
            nvr[rg] = (nvr[rg] & ~(((t_uint64) M8) << sc)) | (((t_uint64) (val & M8)) << sc);
            break;
        case L_HALF:
            sc = (((uint32) pa) & 6) * 8;
            nvr[rg] = (nvr[rg] & ~(((t_uint64) M16) << sc)) | (((t_uint64) (val & M16)) << sc);
            break;
        case L_WORD:
            if (pa & 4) nvr[rg] = ((t_uint64) (nvr[rg] & M32)) | (((t_uint64) (val & M32)) << 32);
            else nvr[rg] = (nvr[rg] & ~((t_uint64) M32)) | ((t_uint64) val & M32);
            break;
        case L_DOUB:
            nvr[rg] = val;
            break;
    }

    return TRUE;
}

/* NVR examine */

t_stat nvr_ex (t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
    uint32 addr = (uint32) exta;

    if (vptr == NULL) {
        return SCPE_ARG;
    }

    if (addr >= NVRSIZE) {
        return SCPE_NXM;
    }

    *vptr = nvr[addr >> 3];

    return SCPE_OK;
}

/* NVR deposit */

t_stat nvr_dep (t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
    uint32 addr = (uint32) exta;

    if (addr >= NVRSIZE) {
        return SCPE_NXM;
    }

    nvr[addr >> 3] = val;

    return SCPE_OK;
}

/* NVR reset */

t_stat nvr_reset (DEVICE *dptr)
{
    if (nvr == NULL) {
        nvr = (t_uint64*) calloc (NVRSIZE >> 3, sizeof (t_uint64));
        nvr_unit.filebuf = nvr;
    }
    if (nvr == NULL) {
        return SCPE_MEM;
    }

    return SCPE_OK;
}

/* NVR attach */

t_stat nvr_attach (UNIT *uptr, char *cptr)
{
    t_stat r;

    uptr->flags = uptr->flags | (UNIT_ATTABLE | UNIT_BUFABLE);

    r = attach_unit (uptr, cptr);
    if (r != SCPE_OK) {
        uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
    } else {
        uptr->hwmark = (uint32) uptr->capac;
    }

    return r;
}

/* NVR detach */

t_stat nvr_detach (UNIT *uptr)
{
    t_stat r;

    r = detach_unit (uptr);
    if ((uptr->flags & UNIT_ATT) == 0) {
        uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
    }

    return r;
}

