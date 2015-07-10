/* sc1_cac.c: SiCortex 1 cache controller

   Copyright (c) 2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

   Copyright (c) 2005, Robert M Supnik

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

   CAC          L2 cache controller
   COHO         odd coherency controller
   COHE         even coherency controller
*/

#include "sim_defs.h"
#include "sc1_defs.h"
#include "sc1_cac.h"

static uint32 coh_eccmode[2];
static uint32 coh_eccstat[2];
static uint32 coh_eccaddr[2];
static uint32 coh_eccsynd[2];
static uint32 cac_slow_int = 0;
static int32 cac_bus2core[CAC_M_BUSSTOP + 1] = {
    -1, -1,  0,  1,  2,  3,  4,  5,
    -1, -1, -1, -1, -1, -1, -1, -1
     };	
extern CORECTX *cpu_ctx[NUM_CORES];

t_bool cac_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool cac_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat cac_reset (DEVICE *dptr);
t_bool coh_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool coh_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat coh_reset (DEVICE *dptr);
t_stat cac_set_ecc (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cac_set_noecc (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cac_show_ecc (FILE *st, UNIT *uptr, int32 val, void *desc);

/* CAC data structures

   cac_dev      CAC device descriptor
   cac_unit     CAC unit
   cac_reg      CAC register list
   cac_mod      CAC modifier list
*/

DIB cac_dib = { PA_CAC_BASE, PA_CAC_BASE+PA_CAC_LNT, &cac_rd, &cac_wr, 0 };

UNIT cac_unit = { UDATA (&cac_ecc_svc, 0, 0) };

REG cac_reg[] = {
    { HRDATA (SLOW_INT, cac_slow_int, 9) },
    { NULL }
    };

MTAB cac_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "ECC", "ECC",
      &cac_set_ecc, &cac_show_ecc },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, NULL, "NOECC",
      &cac_set_noecc, NULL },
    { 0 }
    };

DEVICE cac_dev = {
    "CAC", &cac_unit, cac_reg, cac_mod,
    1, 16, 0, 1, 16, 64,
    NULL, NULL, &cac_reset,
    NULL, NULL, NULL,
    &cac_dib, DEV_DIB|DEV_CORE, 0,
    NULL, NULL, NULL
    };

/* COHx data structures

   cohx_dev     COH device descriptor
   cohx_unit    COH unit
   cohx_reg     COH register list
*/

DIB coho_dib = { PA_COHO_BASE, PA_COHO_BASE+PA_COHO_LNT, &coh_rd, &coh_wr, 0 };
DIB cohe_dib = { PA_COHE_BASE, PA_COHE_BASE+PA_COHE_LNT, &coh_rd, &coh_wr, 0 };

UNIT coho_unit = { UDATA (NULL, 0, 0) };
UNIT cohe_unit = { UDATA (NULL, 0, 0) };

REG coho_reg[] = {
    { HRDATA (ECCMODE, coh_eccmode[0], 3) },
    { HRDATA (ECCSTAT, coh_eccstat[0], 3) },
    { HRDATA (ECCADDR, coh_eccaddr[0], 31) },
    { HRDATA (ECCSYNC, coh_eccsynd[0], 8) },
    { NULL }
    };

REG cohe_reg[] = {
    { HRDATA (ECCMODE, coh_eccmode[1], 3) },
    { HRDATA (ECCSTAT, coh_eccstat[1], 3) },
    { HRDATA (ECCADDR, coh_eccaddr[1], 31) },
    { HRDATA (ECCSYNC, coh_eccsynd[1], 8) },
    { NULL }
    };

DEVICE coh_dev[] = {
    {
    "COHO", &coho_unit, coho_reg, NULL,
    1, 16, 0, 1, 16, 64,
    NULL, NULL, &coh_reset,
    NULL, NULL, NULL,
    &coho_dib, DEV_DIB, 0,
    NULL, NULL, NULL
    },
    {
    "COHE", &cohe_unit, cohe_reg, NULL,
    1, 16, 0, 1, 16, 64,
    NULL, NULL, &coh_reset,
    NULL, NULL, NULL,
    &cohe_dib, DEV_DIB, 0,
    NULL, NULL, NULL
    }
    };

/* CAC register read and write */

t_bool cac_rd (t_uint64 pa, t_uint64 *val, uint32 lnt)
{
uint32 off = (uint32) (pa - PA_CAC_BASE) >> 3;
CORECTX *ctx = cpu_ctx[lnt];

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: CAC: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
if (off < CAC_ICR_NUM) *val = ctx->cac_icr[off] & CAC_ICR_RD;
else switch (off) {

    case CAC_SLSEL_OFF:
        *val = ctx->cac_slow_mask & CAC_SLSEL_RW;
        break;

    case CAC_SLSTA_OFF:
        *val = cac_slow_int;
        break;

    case CAC_ECCMODE_OFF:
        *val = ctx->cac_EccMode & CAC_ECCMODE_RW;
        break;

    case CAC_ECCSTAT_OFF:
        *val = ctx->cac_EccStat & CAC_ECCSTAT_R;
        break;

    case CAC_L2ECCADDR_OFF:
        *val = ctx->cac_L2EccAddr & CAC_ECCADDR_R;
        break;

    case CAC_CSWECCADDR_OFF:
        *val = ctx->cac_CSWEccAddr & CAC_ECCADDR_R;
        break;

    case CAC_TAGECCADDR_OFF:
        *val = ctx->cac_TagEccAddr & CAC_ECCADDR_R;
        break;

    case CAC_ECCSYND_OFF:
        *val = ctx->cac_EccSynd & CAC_ECCSYND_R;
        break;

    default:
        *val = 0;
        break;
    }

return TRUE;
}

t_bool cac_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
uint32 off = (uint32) (pa - PA_CAC_BASE) >> 3;
uint32 v32 = (uint32) val;
CORECTX *ctx = cpu_ctx[lnt];

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: CAC: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
if (off < CAC_ICR_NUM) {
    if (v32 & CAC_ICR_ACT) ctx->cac_icr[off] &= ~(CAC_ICR_ACT|CAC_ICR_OVF);
    eval_intr (ctx);
    }
else switch (off) {

    case CAC_SLSEL_OFF:
        ctx->cac_slow_mask = v32 & CAC_SLSEL_RW;
        break;

    case CAC_SLSTA_OFF:
        ctx->cac_slow_local &= ~(v32 & SLOW_LOCAL);
        eval_intr (ctx);
        break;

    case CAC_ECCMODE_OFF:
        ctx->cac_EccMode = v32 & CAC_ECCMODE_RW;
        break;

    case CAC_ECCSTAT_OFF:
        ctx->cac_EccStat &= ~(v32 & CAC_ECCSTAT_W1C);
        break;

    case CAC_IDR_OFF:
        cac_csw_int (CAC_IDR_GETBUS (v32), CAC_IDR_GETIDX (v32), v32 & CAC_IDR_CAUSE);
        break;

    default:
        break;
    }

return TRUE;
}

/* COH register read and write */

t_bool coh_rd (t_uint64 pa, t_uint64 *val, uint32 lnt)
{
uint32 off = (uint32) ((pa - PA_COHO_BASE) & (PA_COHO_LNT -1)) >> 3;
uint32 coh = (uint32) (pa >> 24) & 1;

switch (off) {

    case COH_ECCMODE_OFF:
        *val = coh_eccmode[coh] & COH_ECCMODE_RW;
        break;

    case COH_ECCSTAT_OFF:
        *val = coh_eccstat[coh] & COH_ECCSTAT_RW1C;
        break;

    case COH_ECCADDR_OFF:
        *val = coh_eccaddr[coh] & COH_ECCADDR_R;
        break;

    case COH_ECCSYND_OFF:
        *val = coh_eccsynd[coh] & COH_ECCSYND_R;

    default:
        *val = 0;
        break;
    }

return TRUE;
}

t_bool coh_wr (t_uint64 pa, t_uint64 val, uint32 lnt)
{
uint32 off = (uint32) ((pa - PA_COHO_BASE) & (PA_COHO_LNT -1)) >> 3;
uint32 coh = (uint32) (pa >> 24) & 1;
uint32 v32 = (uint32) val;

switch (off) {

    case COH_ECCMODE_OFF:
        coh_eccmode[coh] = v32 & COH_ECCMODE_RW;
        break;

    case COH_ECCSTAT_OFF:
        coh_eccstat[coh] &= ~(v32 & COH_ECCSTAT_RW1C);
        break;

    default:
        break;
    }

return TRUE;
}

/* CAC API's */

void cac_set_slow (uint32 slow)
{
cac_slow_int |= (slow & SLOW_GLOBAL);
eval_intr_all ();
return;
}

void cac_clr_slow (uint32 slow)
{
cac_slow_int &= ~(slow & SLOW_GLOBAL);
eval_intr_all ();
return;
}

t_stat cac_csw_int (uint32 bus, uint32 idx, uint32 val)
{
int32 core;
CORECTX *ctx;

core = cac_bus2core [bus & CAC_M_BUSSTOP];
if ((bus > CAC_M_BUSSTOP) || (core < 0) || (idx >= CAC_ICR_NUM)) {
    fprintf (stderr, "%%Error: Invalid CSW interrupt call, bus = %d, idx = %d\r\n", bus, idx);
    return SCPE_IERR;
    }
ctx = cpu_ctx[core];
if (ctx->cac_icr[idx] & CAC_ICR_ACT) ctx->cac_icr[idx] |= CAC_ICR_OVF;
else {
    ctx->cac_icr[idx] = (ctx->cac_icr[idx] & ~CAC_ICR_CAUSE) |
    (val & CAC_ICR_CAUSE) | CAC_ICR_ACT;
    eval_intr_all ();
    }
return SCPE_OK;
}

uint32 cac_eval_intr (CORECTX *ctx)
{
uint32 i, pins;

pins = 0;
for (i = 0; i < CAC_ICR_NUM; i++) {
    if (ctx->cac_icr[i] & CAC_ICR_ACT) pins |= (1u << (i / 2));
    if ((ctx->cac_slow_local | cac_slow_int) & ctx->cac_slow_mask)
        pins |= (1u << (SLOW_ILVL - INT_N_SLVLS));
    }
return pins;
}

/* Schedule an ECC event for the future */

void cac_ecc_event (t_uint64 dat)
{
if (dat == 0) sim_cancel (&cac_unit);
else {
    uint32 time = (((uint32) dat) >> ECCEVT_V_TIME) & ECCEVT_M_TIME;
    cac_unit.u3 = (uint32) dat & 0xFFFFFFFF;
    cac_unit.u4 = (uint32) ((dat >> 32) & 0xFFFFFFFF);
    sim_activate (&cac_unit, time);
    }
return;
}

/* Spring an ECC event, parameters in u3 and u4 */

t_stat cac_ecc_svc (UNIT *uptr)
{
uint32 sect = (uptr->u3 >> ECCEVT_V_SECT) & ECCEVT_M_SECT;
uint32 event = (uptr->u3 >> ECCEVT_V_EVT) & ECCEVT_M_EVT;
uint32 data = uptr->u4;
uint32 coh;
CORECTX *ctx;

switch (sect) {

    case ECCS_C0: case ECCS_C1: case ECCS_C2:
    case ECCS_C3: case ECCS_C4: case ECCS_C5:
        ctx = cpu_ctx[sect];
        if ((event & ECC_CE_L1) &&
            (get_cp0_errctl() & CP0_ERRCTL_ECCE)) {
            set_cp0_errctl (get_cp0_errctl() & ~CP0_ERRCTL_ECCE);
            set_cp0_ccherr (data & CP0_CCHERR_R);
            ctx->events |= EVT_CCHE;         
            data = (data << 1) && (data >> 1);
            }
        if ((event & ECC_CE_L2T) &&
            (ctx->cac_EccMode & CAC_ECCMODE_L2TE)) {
            ctx->cac_L2EccAddr = ((t_uint64) data) << 3;
            UPDATE_ECC_STAT (ctx, L2T, event);
            if (ctx->cac_EccStat & CAC_ECCSTAT_L2TC)
                ctx->cac_slow_local |= SLOW_L2C;
            if (ctx->cac_EccStat & CAC_ECCSTAT_L2TU)
                ctx->cac_slow_local |= SLOW_L2U;
            data = (data << 1) && (data >> 1);
            }
        if ((event & ECC_CE_CSW) &&
            (ctx->cac_EccMode & CAC_ECCMODE_CSWE)) {
            ctx->cac_CSWEccAddr = ((t_uint64) data) << 3;
            ctx->cac_EccSynd = (data >> 16) ^ data;
            UPDATE_ECC_STAT (ctx, CSW, event);
            if (ctx->cac_EccStat & CAC_ECCSTAT_CSWC)
                ctx->cac_slow_local |= SLOW_CSWC;
            if (ctx->cac_EccStat & CAC_ECCSTAT_CSWU)
                ctx->cac_slow_local |= SLOW_CSWU;
            data = (data << 1) && (data >> 1);
            }
        if ((event & ECC_CE_L2D) &&
            (ctx->cac_EccMode & CAC_ECCMODE_L2DE)) {
            ctx->cac_L2EccAddr = ((t_uint64) data) << 3;
            UPDATE_ECC_STAT (ctx, L2D, event);
            if (ctx->cac_EccStat & CAC_ECCSTAT_L2DC)
                ctx->cac_slow_local |= SLOW_L2C;
            if (ctx->cac_EccStat & CAC_ECCSTAT_L2DU)
                ctx->cac_slow_local |= SLOW_L2U;
            }
        break;

    case ECCS_COHO: case ECCS_COHE:
        coh = sect - ECCS_COHO;
        if (event & ECC_COH_TAG) {
            coh_eccstat[coh] = event & COH_ECCSTAT_RW1C;
            if (((event & COH_ECCSTAT_COR) &&
                 (coh_eccmode[coh] & COH_ECCMODE_SGL)) ||
                ((event & COH_ECCSTAT_UNC) &&
                 (coh_eccmode[coh] & COH_ECCMODE_DBL))) {
                coh_eccaddr[coh] = data & COH_ECCADDR_R;
                coh_eccsynd[coh] = event & COH_ECCSYND_R;
                cac_set_slow (1u << (SLOW_V_COHO + coh));
                }
            }
        break;            

    default:
        fprintf (stderr, "%%Error: CAC: invalid ECC event %X %X %X\n",
            sect, event, data);
        break;
        }

return SCPE_OK;
}

/* ECC event SET/SHOW routines */

t_stat cac_set_ecc (UNIT *uptr, int32 val, char *cptr, void *desc)
{
struct op_vld {
    uint32  min;
    uint32  max;
    t_bool  qdflt;
    uint32  rdx;
    uint32  dflt;
    };

static struct op_vld opnds[4] = {
    { 0, ECCEVT_M_SECT, FALSE, 16, 0 },
    { 1, ECCEVT_M_EVT,  FALSE, 16, 0 },
    { 1, ECCEVT_M_TIME, TRUE,  10, 1 },
    { 0, 0xFFFFFFFF,    TRUE,  16, 0 },
    };

uint32 i, dat[4];
t_stat r;
char gbuf[CBUFSIZE];

if ((cptr == NULL) || (*cptr == 0)) return SCPE_ARG;
for (i = 0; i < 4; i++) {
    if (*cptr) {
        cptr = get_glyph (cptr, gbuf, ';');
        dat[i] = (uint32) get_uint (gbuf, opnds[i].rdx, opnds[i].max, &r);
        if ((r != SCPE_OK) || (dat[i] < opnds[i].min))
            return SCPE_ARG;
        }
    else {
        if (!opnds[i].qdflt) return SCPE_OK;
        dat[i] = opnds[i].dflt;
        }
    }
if (*cptr != 0) return SCPE_ARG;
uptr->u3 = (dat[0] << ECCEVT_V_SECT) |
    (dat[1] << ECCEVT_V_EVT) |
    (dat[2] << ECCEVT_V_TIME);
uptr->u4 = dat[3];
sim_cancel (&cac_unit);
sim_activate (&cac_unit, dat[2]);
return SCPE_OK;        
}

t_stat cac_set_noecc (UNIT *uptr, int32 val, char *cptr, void *desc)
{
sim_cancel (&cac_unit);
return SCPE_OK;
}

t_stat cac_show_ecc (FILE *st, UNIT *uptr, int32 val, void *desc)
{
int32 t;

if ((t = sim_is_active (&cac_unit)) != 0) {
    uint32 sect = (uptr->u3 >> ECCEVT_V_SECT) & ECCEVT_M_SECT;
    uint32 event = (uptr->u3 >> ECCEVT_V_EVT) & ECCEVT_M_EVT;
    uint32 data = uptr->u4;
    fprintf(st, "ECC event scheduled at queue time %d, section=%X, event=%X, data=%X\n",
        t - 1, sect, event, data);
    }
else fputs ("No ECC event scheduled\n", st);
return SCPE_OK;
}

/* Reset routines */

t_stat cac_reset (DEVICE *dptr)
{
cac_slow_int = 0;
sim_cancel (&cac_unit);
return SCPE_OK;
}

t_stat coh_reset (DEVICE *dptr)
{
uint32 coh = dptr - coh_dev;

if (coh > 1) return SCPE_IERR;
coh_eccmode[coh] = 0;
coh_eccstat[coh] = 0;
coh_eccaddr[coh] = 0;
coh_eccsynd[coh] = 0;
return SCPE_OK;
}
