/* sc1_mmu.c - SC1 memory management simulator

   Copyright (c) 2007, SiCortex, Inc.  All rights reserved.

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

   28-Sep-07    RMS     Added Mips64 R2 support
   05-Jan-06    RMS     Fixed PRid to include WHAMI
   21-Nov-05    RMS     Fixed multiple bugs in large page handling
   18-Nov-05    RMS     DMFC0, DMFC0 trap in 32b mode
                        Added WriteTest routine
   14-Oct-05    RMS     Mechanized performance counters
   09-Sep-05    RMS     MFC should read coprocessor, even if dst reg is R0
   08-Sep-05    RMS     Reworded TLB for greater accuracy
                        Added test for invalid index in tlb_read, tlb_write
   07-Sep-05    RMS     Fixed TBLR, BADVA to be read only
                        Fixed ERL impact on KUSEG addressing
                        Fixed XCONTEXT to mask low 4b onr read
   18-Aug-05    RMS     Changed counter, compare to 32b
   25-Nov-05    LCS     Fixed tb fill for phys addrs > 32b

   This module contains the routines for

        ReadB,H,W,D     -       read aligned virtual
        WriteB,H,W,D    -       write aligned virtual

   The TLB is comprised of these fields:

        tag             VA<63:13>, G, ASID
        mask            page mask
        indx            entry number in "real" TLB
        flag0           PFN0 flags
        flag1           PFN1 flags
        pfn0            PFN0, shifted left 12
        pfn1            PFN1, shifted left 12
        
   The TLB is sorted by tag for faster searching.
*/

#include "sc1_defs.h"
#include "sc1_stats.h"

#define CPR_S(r,s)      ((r) | ((s) << 5))

t_bool tlb_set_err (CORECTX *ctx, t_uint64 va, uint32 mode, uint32 err);
t_bool tlb_set_aer (CORECTX *ctx, t_uint64 va, uint32 mode);
INLINE t_bool tlb_read (CORECTX *ctx, uint32 indx);
t_bool tlb_write_i (CORECTX *ctx, uint32 indx);
t_bool tlb_write_r (CORECTX *ctx);
int tlb_comp (const void *e1, const void *e2);
INLINE TLBENT *tlb_search (CORECTX *ctx, t_uint64 va);
void tlb_inv_mtlb (CORECTX *ctx);
void tlb_read_ent (CORECTX *ctx, TLBENT *tlbp);
void tlb_write_ent (CORECTX *ctx, TLBENT *tlbp);
t_uint64 cp0_getreg (CORECTX *ctx, uint32 rn, uint32 sel);
void cp0_putreg (CORECTX *ctx, uint32 rn, uint32 sel, t_uint64 val);

extern t_uint64 counter_rd_count (CORECTX *ctx);
extern void counter_wr_count (CORECTX *ctx, t_uint64 val);
extern void counter_wr_compr (CORECTX *ctx, t_uint64 val);

extern t_uint64 total_count;
extern UNIT mem_unit;

/* Enable uncached access check unless told otherwise */
#ifndef DISABLE_UNCACHED_CHECK
#define TRAP_UNCACHED 1
#endif

#if TRAP_UNCACHED
static t_bool msg_kseg1   = FALSE;	
static t_bool msg_xkphys  = FALSE;	
static char*  msg_mode[5] = {"data", "write", "instr", "bad", "cons"};
#endif

/* Read virtual aligned

   Inputs:
        ctx     =       context
        va      =       virtual address
        val     =       pointer to value
   Output:
        TRUE if ok, FALSE if abort
*/

t_bool ReadB (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (!xlate_va (ctx, va, VA_DR, &pa, &catr)) return FALSE;
return CALL_READPB (ctx, pa, val, catr);
}

t_bool ReadH (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 1) return tlb_set_aer (ctx, va, VA_DR);
if (!xlate_va (ctx, va, VA_DR, &pa, &catr)) return FALSE;
return CALL_READPH (ctx, pa, val, catr);
}

t_bool ReadW (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 3) return tlb_set_aer (ctx, va, VA_DR);
if (!xlate_va (ctx, va, VA_DR, &pa, &catr)) return FALSE;
return CALL_READPW (ctx, pa, val, catr);
}

t_bool ReadD (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 7) return tlb_set_aer (ctx, va, VA_DR);
if (!xlate_va (ctx, va, VA_DR, &pa, &catr)) return FALSE;
return CALL_READPD (ctx, pa, val, catr);
}

t_bool ReadI (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (va & 3) return tlb_set_aer (ctx, va, VA_IR);
if (!xlate_va (ctx, va, VA_IR, &pa, &catr)) return FALSE; 
return CALL_READPI (ctx, pa, val, catr);
}

/* Write virtual aligned

   Inputs:
        ctx     =       context
        va      =       virtual address
        val     =       value
   Output:
        TRUE if ok, FALSE if abort
*/

t_bool WriteB (CORECTX *ctx, t_uint64 va, t_uint64 val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (!xlate_va (ctx, va, VA_DW, &pa, &catr)) return FALSE;
return CALL_WRITEPB (ctx, pa, val, catr);
}

t_bool WriteH (CORECTX *ctx, t_uint64 va, t_uint64 val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 1) return tlb_set_aer (ctx, va, VA_DW);
if (!xlate_va (ctx, va, VA_DW, &pa, &catr)) return FALSE;
return CALL_WRITEPH (ctx, pa, val, catr);
}

t_bool WriteW (CORECTX *ctx, t_uint64 va, t_uint64 val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 3) return tlb_set_aer (ctx, va, VA_DW);
if (!xlate_va (ctx, va, VA_DW, &pa, &catr)) return FALSE;
return CALL_WRITEPW (ctx, pa, val, catr);
}

t_bool WriteD (CORECTX *ctx, t_uint64 va, t_uint64 val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (va & 7) return tlb_set_aer (ctx, va, VA_DW);
if (!xlate_va (ctx, va, VA_DW, &pa, &catr)) return FALSE;
return CALL_WRITEPD (ctx, pa, val, catr);
}

/* Special test routine needed to get correct exc code for RMW sequences */

t_bool WriteTest (CORECTX *ctx, t_uint64 va)
{
t_uint64 pa;
uint32 catr;

return xlate_va (ctx, va, VA_DW, &pa, &catr); 
}

/* Translate address, instruction and data

   Inputs:
        ctx     =       context
        va      =       virtual address
        mode    =       access mode (0 = D-read, 1 = D-write, 2+ = console)
        pa      =       pointer to output physical address
        catr    =       pointer to cache attributes
   Outputs:
        TRUE if ok, FALSE if TLB exception
*/

t_bool xlate_va (CORECTX *ctx, t_uint64 va, uint32 mode, t_uint64 *pa, uint32 *catr)
{
uint32 rgn, fl;
TLBENT *tlbp;
t_uint64 pfn;

rgn = VA_GETRGN (va);
switch (rgn) {                                          /* case on VA<63:62> */

    case VA_USEG:                                       /* useg */
        if ((get_cp0_sr() & (CP0_SR_MD|CP0_SR_ERL)) == CP0_SR_ERL) { /* kuseg ERL? */
            if (va >= XUSEG_END) return tlb_set_aer (ctx, va, mode);
            *pa = va & PA_MASK;                         /* unmapped */
            *catr = CA_UNCA;
            return TRUE;
            }
        if (va >= (t_uint64) ((get_cp0_sr() & CP0_SR_UX)? XUSEG_END: USEG_END))
            return tlb_set_aer (ctx, va, mode);
        break;

    case VA_SSEG:                                       /* sseg */
        if (!(get_cp0_sr() & CP0_SR_SX) || Q_MD_U ||
            (va >= XSSEG_END))
            return tlb_set_aer (ctx, va, mode);
        break;

    case VA_XKPHYS:                                     /* physical */
        if (!(get_cp0_sr() & CP0_SR_KX) || !Q_MD_K ||
            (va & XKPHYS_MBZ))
            return tlb_set_aer (ctx, va, mode);
        *pa = va & PA_MASK;
        *catr = (uint32) ((va >> 59) & 07);
#if TRAP_UNCACHED
        /* On sc1, uncached writes to memory are not supported */
	if (!msg_xkphys && CA_UNCACHED(*catr) && (mode & VA_WRT) && !PA_IS_IO(*pa)) {
            msg_xkphys = TRUE; 
            printf("%%ERROR: SIMH: attempt to access XKPHYS UNCA memory %llX %s PC %llX\n", 
			va, msg_mode[mode], ctx->PC);
            ctx->traps |= TRAP_SIER;
	    return FALSE;
	   }
	/* Cached accesses to IO space are also illegal */
	if (!msg_xkphys && !CA_UNCACHED(*catr) && PA_IS_IO(*pa)) {
            msg_xkphys = TRUE; 
            printf("%%ERROR: SIMH: cached access to IO space address %llX %s PC %llX\n", 
			va, msg_mode[mode], ctx->PC);
            ctx->traps |= TRAP_SIER;
	    return FALSE;
	   }
#endif
        return TRUE;

    case VA_XKSEG:                                      /* kseg */
        if (va < XKSEG_COMP) {                          /* not compat mode? */
            if (!(get_cp0_sr() & CP0_SR_KX) || !Q_MD_K ||
                (va >= XKSEG_END))
                return tlb_set_aer (ctx, va, mode);
             }
        else if (va < XKSEG_SSEG) {                     /* KSEG0 or 1? */
            if (!Q_MD_K)
                return tlb_set_aer (ctx, va, mode);
            *pa = va & PA_MASK_29;
            *catr = (va & 0x20000000)? CA_UNCA: CP0_CNF_GETK0 (get_cp0_cnf());
#if TRAP_UNCACHED
            /* On sc1, uncached writes to memory are not supported */
	    if (!msg_kseg1 && *catr == CA_UNCA && (mode & VA_WRT)) {
		msg_kseg1 = TRUE; 
                printf("%%ERROR: SIMH: attempt to access KSEG0/KSEG1 uncached address %llX %s PC %llX\n", 
			va, msg_mode[mode], ctx->PC);
                ctx->traps |= TRAP_SIER;
	        return FALSE;
	    	}
#endif
            return TRUE;
            }
        else {                                          /* SSEG/KSEG3 */
            if ((va & 0x20000000)? !Q_MD_K: Q_MD_U)
                return tlb_set_aer (ctx, va, mode);
            }
        break;
        }                                               /* end case */

/* Check mini-TLBs */

if (mode & VA_INS) {                                    /* instruction? */
    if (((va ^ ctx->i_mtlb_tag) & ~VA_M_OFF) == 0) {    /* mini-TLB match? */
        *pa = ctx->i_mtlb_pfn | (va & VA_M_OFF);
        *catr = TLBF_GETCA (ctx->i_mtlb_fl);
        return TRUE;
        }
    }
else {                                                  /* data */
    if ((((va ^ ctx->d_mtlb_tag) & ~VA_M_OFF) == 0) &&  /* mini-TLB match? */
        ((ctx->d_mtlb_fl & TLBF_D) ||                   /* dirty or read? */
        !(mode & VA_WRT))) {
        *pa = ctx->d_mtlb_pfn | (va & VA_M_OFF);
        *catr = TLBF_GETCA (ctx->d_mtlb_fl);
        return TRUE;
        }
    }

/* Now do TLB lookup */

tlbp = tlb_search (ctx, va);                            /* search TLB */
if (tlbp != NULL) {                                     /* hit? */
    if (va & tlbp->sel) {                               /* which pfn? */
        fl = tlbp->f1;
        pfn = tlbp->pfn1;
        }
    else {
        fl = tlbp->f0;
        pfn = tlbp->pfn0;
        }
    if (!(fl & TLBF_V)) return tlb_set_err (ctx, va, mode,
        (mode & VA_WRT)? TRAP_STLBV: TRAP_LTLBV);
    if ((mode & VA_WRT) && !(fl & TLBF_D))
        return tlb_set_err (ctx, va, mode, TRAP_STLBW);
    *pa = pfn | VA_GETOFF (tlbp->mask >> 1, va);
    *catr = TLBF_GETCA (fl);
    if (mode & VA_INS) {                                /* instruction? */
        ctx->i_mtlb_tag = va;                           /* update i mini-TLB */
        ctx->i_mtlb_pfn = *pa & ~VA_M_OFF;
        ctx->i_mtlb_fl = fl;
        }
    else {                                              /* data */
        ctx->d_mtlb_tag = va;                           /* update d mini-TLB */
        ctx->d_mtlb_pfn = *pa & ~VA_M_OFF;
        ctx->d_mtlb_fl = fl;
        }
    return TRUE;
    }
return tlb_set_err (ctx, va, mode, (mode & VA_WRT)? TRAP_STLBM: TRAP_LTLBM);        
}

/* TLB error */

t_bool tlb_set_err (CORECTX *ctx, t_uint64 va, uint32 mode, uint32 err)
{
if ((mode & VA_CON) == 0) {
    t_uint64 rgn = VA_GETRGN (va);
    t_uint64 vpn2 = VA_GETVPN2 (va);

    STATS_TLBMISS(ctx, va, (mode & VA_INS) ? 1 : 0);
    ctx->traps |= err;
    set_cp0_badva (va);
    set_cp0_ctxt ((get_cp0_ctxt() & ~CP0_CTXT_VPN2) |
        ((vpn2 << 4) & CP0_CTXT_VPN2));
    set_cp0_xctxt ((get_cp0_xctxt() & ~CP0_XCTXT_RVPN2) |
        (((rgn << 31) | (vpn2 << 4)) & CP0_XCTXT_RVPN2));
    set_cp0_enthi ((get_cp0_enthi() & ~CP0_EHI_RVPN2) |
        (va & CP0_EHI_RVPN2));
    tlb_inv_mtlb (ctx);
    }
return FALSE;
}

/* TLB address error */

t_bool tlb_set_aer (CORECTX *ctx, t_uint64 va, uint32 mode)
{
if ((mode & VA_CON) == 0) {
    ctx->traps |= ((mode & VA_WRT)? TRAP_SAER: TRAP_LAER);
    set_cp0_badva (va);
    tlb_inv_mtlb (ctx);
    }
return FALSE;
}

/* TLB read routine */

INLINE t_bool tlb_read (CORECTX *ctx, uint32 indx)
{
uint32 i;

if (indx < TLB_LNT) {                                   /* VTLB? */
    for (i = 0; i < TLB_LNT; i++) {                     /* search */
        if (ctx->tlb[i].indx == indx) {                 /* found entry? */
            tlb_read_ent (ctx, ctx->tlb + i);           /* read it */
            return TRUE;
            }
        }
    }
#if (FTLB_LNT)
else if (indx < (TLB_LNT + (FTLB_LNT * FTLB_SETS))) {   /* FTLB? */
    tlb_read_ent (ctx, ctx->ftlb + (indx - TLB_LNT));   /* read it */
    return TRUE;
    }
#endif
printf ("%%Attempt to read TLB entry %d\n\r", indx);
ctx->traps |= TRAP_SIER;
return FALSE;
}

/* Read TLB entry */

void tlb_read_ent (CORECTX *ctx, TLBENT *tlbp)
{
uint32 gf = (tlbp->tag & TLBT_MASK_G)? TLBF_G: 0;

set_cp0_entlo0 ((tlbp->pfn0 >> (VA_V_VPN - CP0_ELO_V_PFN)) | tlbp->f0 | gf);
set_cp0_entlo1 ((tlbp->pfn1 >> (VA_V_VPN - CP0_ELO_V_PFN)) | tlbp->f1 | gf);
set_cp0_enthi (tlbp->tag & (TLBT_MASK_VPN2|TLBT_MASK_ASID));
return;
}

/* TLB write random routine */

t_bool tlb_write_r (CORECTX *ctx)
{
uint32 idx;

#if (FTLB_LNT)
if (get_cp0_mask() == FTLB_PGSIZE) {                    /* FTLB write? */
    uint32 t = FTLB_VAIDX (get_cp0_enthi());            /* line index */
    idx = (t << 2) | ctx->ftlb_lru[t];                  /* entry index */
    ctx->ftlb_lru[t] = (ctx->ftlb_lru[t] + 1) % FTLB_SETS;
    tlb_write_ent (ctx, ctx->ftlb + idx);               /* write entry */
    return TRUE;
    }
else
#endif
    {
    idx = get_cp0_tlbr();                               /* get random index */
    if (idx <= get_cp0_wired())                         /* decrement */
        set_cp0_tlbr (TLB_LNT - 1);
    else set_cp0_tlbr (idx - 1);
    return tlb_write_i (ctx, idx);                      /* write indexed */
    }
}

/* TLB write indexed routine */

t_bool tlb_write_i (CORECTX *ctx, uint32 indx)
{
int32 i, sav_i;
t_uint64 cmp;

tlb_inv_mtlb (ctx);                                     /* invald mini-TLBs */
if (indx < TLB_LNT) {                                   /* write variable TLB? */
    uint32 newg = (uint32) get_cp0_entlo0() & (uint32) get_cp0_entlo1() & TLBF_G;  /* new entry G? */
    for (i = 0, sav_i = -1; i < TLB_LNT; i++) {         /* check for duplicate */
        cmp = get_cp0_enthi() ^ ctx->tlb[i].tag;        /* compare tag to VA */
        if (ctx->tlb[i].indx == indx) sav_i = i;
        else if ((((cmp & TLBT_MASK_VPN2) &             /* VPN2 match and */
            ~(get_cp0_mask() | ctx->tlb[i].mask)) == 0) &&
            (newg || (ctx->tlb[i].tag & TLBT_MASK_G) || /* either G set or */
            ((cmp & TLBT_MASK_ASID) == 0))) {           /* ASID match? */
            ctx->traps |= TRAP_MCHK;
            set_cp0_sr (get_cp0_sr() | CP0_SR_TS);
#ifdef DEBUG
            fprintf(stderr,"MCHK: ctx hi %llx lo0 %llx lo1 %llx msk %x indx %d\n\r", get_cp0_enthi(),
                    get_cp0_entlo0(), get_cp0_entlo1(), get_cp0_mask(), indx);
            fprintf(stderr,"      tlb i %d tag %llx msk %llx\n\r", i, ctx->tlb[i].tag, ctx->tlb[i].mask);
#endif
            return FALSE;
            }
        }
    if (sav_i >= 0) {
        tlb_write_ent (ctx, ctx->tlb + sav_i);          /* write TLB entry */
        qsort (ctx->tlb, TLB_LNT, sizeof (TLBENT), &tlb_comp);
        return TRUE;
        }
    }
#if (FTLB_LNT)                                          /* fixed TLB? */
else if (indx < (TLB_LNT + (FTLB_LNT * FTLB_SETS))) {   /* write fixed TLB? */
    uint32 t = FTLB_VAIDX (get_cp0_enthi());            /* where it should go */
    uint32 findx = indx - TLB_LNT;                      /* where it's going */
    if ((t != (findx >> 2)) ||                          /* wrong params? */
        (get_cp0_mask() != FTLB_PGSIZE)) {
        ctx->traps |= TRAP_MCHK;                        /* you lose */
        set_cp0_sr (get_cp0_sr() | CP0_SR_TS);
#ifdef DEBUG
        fprintf(stderr,"MCHK: ctx hi %llx lo0 %llx lo1 %llx msk %x indx %d\n\r", get_cp0_enthi(),
                get_cp0_entlo0(), get_cp0_entlo1(), get_cp0_mask(), indx);
        fprintf(stderr,"      tlb i %d tag %llx msk %llx\n\r", indx, ctx->ftlb[indx].tag, get_cp0_mask());
#endif
        return FALSE;
        }
    tlb_write_ent (ctx, ctx->ftlb + findx);             /* write FTLB entry */
    return TRUE;
    }
#endif

printf ("%%Attempt to write TLB entry %d\n\r", indx);
ctx->traps |= TRAP_SIER;
return FALSE;
}

/* Write TLB entry */

void tlb_write_ent (CORECTX *ctx, TLBENT *tlbp)
{
t_int64 sel;
uint32 newg = (uint32) get_cp0_entlo0() & (uint32) get_cp0_entlo1() & TLBF_G;

if (newg) tlbp->tag = (get_cp0_enthi() & TLBT_MASK_VPN2) | TLBT_MASK_G;
else tlbp->tag = get_cp0_enthi() & (TLBT_MASK_VPN2|TLBT_MASK_ASID);
tlbp->mask = get_cp0_mask();
sel = ~get_cp0_mask() & ~SIM_ULL(0x1FFF);
tlbp->sel = (sel & -sel) >> 1;
tlbp->pfn0 = (get_cp0_entlo0() & CP0_ELO_PFN)
    << (VA_V_VPN - CP0_ELO_V_PFN);
tlbp->pfn1 = (get_cp0_entlo1() & CP0_ELO_PFN)
    << (VA_V_VPN - CP0_ELO_V_PFN);
tlbp->f0 = ((uint32) get_cp0_entlo0() & CP0_ELO_FLAGS) & ~TLBF_G;
tlbp->f1 = ((uint32) get_cp0_entlo1() & CP0_ELO_FLAGS) & ~TLBF_G;
#ifdef DEBUG
fprintf(stderr, "    WRITE TLB : tag %llx msk %llx indx %d pfn %llx %llx\n\r", 
        tlbp->tag, tlbp->mask, tlbp->indx, 
        tlbp->pfn0, tlbp->pfn1);
#endif
return;
}

/* Compare routine for qsort */

int tlb_comp (const void *e1, const void *e2)
{
TLBENT *t1 = (TLBENT *) e1;
TLBENT *t2 = (TLBENT *) e2;

if ((t1->tag & ~t1->mask) > (t2->tag & ~t2->mask)) return +1;
if ((t1->tag & ~t1->mask) < (t2->tag & ~t2->mask)) return -1;
return 0;
}

/* TLB search routine */

INLINE TLBENT *tlb_search (CORECTX *ctx, t_uint64 va)
{
int32 p, hi, lo;
t_uint64 mask, vag;

vag = (va & TLBT_MASK_VPN2) | TLBT_MASK_G;              /* look for globals */
va = (va & TLBT_MASK_VPN2) | (get_cp0_enthi() & CP0_EHI_M_ASID);

#if (FTLB_LNT)
lo = FTLB_VAIDX (va) << 2;
hi = lo + FTLB_SETS;
do {
    if ((((va ^ ctx->ftlb[lo].tag) & ~FTLB_PGSIZE) == 0) || /* match to FTLB? */
        (((vag ^ ctx->ftlb[lo].tag) & ~FTLB_PGSIZE) == 0))  /* gmatch to FTLB? */
        return (ctx->ftlb + lo);                        /* return entry */
    lo++;
    } while (lo < hi);
#endif

lo = 0;                                                 /* initial bounds */
hi = TLB_LNT - 1;
do {
    p = (lo + hi) >> 1;                                 /* probe */
    mask = ctx->tlb[p].mask;
    if (((va ^ ctx->tlb[p].tag) & ~mask) == 0) 		    /* match to TLB? */
        return (ctx->tlb + p);                          /* return entry */
    if ((va & ~mask) < (ctx->tlb[p].tag & ~mask))       /* go down? p is upper */
        hi = p - 1;
    else lo = p + 1;                                    /* go up? p is lower */
    }
while (lo <= hi);

lo = 0;                                                 /* initial bounds */
hi = TLB_LNT - 1;
do {
    p = (lo + hi) >> 1;                                 /* probe */
    mask = ctx->tlb[p].mask;
    if (((vag ^ ctx->tlb[p].tag) & ~mask) == 0)         /* match to TLB? */
        return (ctx->tlb + p);                          /* return entry */
    if (vag < ctx->tlb[p].tag)                          /* go down? p is upper */
        hi = p - 1;
    else lo = p + 1;                                    /* go up? p is lower */
    }
while (lo <= hi);
return NULL;
}

/* Invalidate mini-TLBs */

void tlb_inv_mtlb (CORECTX *ctx)
{
ctx->i_mtlb_tag = ((t_uint64) VA_XKPHYS) << VA_V_RGN;
ctx->i_mtlb_pfn = 0;
ctx->i_mtlb_fl = 0;
ctx->d_mtlb_tag = ((t_uint64) VA_XKPHYS) << VA_V_RGN;
ctx->d_mtlb_pfn = 0;
ctx->d_mtlb_fl = 0;
return;
}

/* TLB init routine */

void tlb_init (CORECTX *ctx)
{
uint32 i;

for (i = 0; i < TLB_LNT; i++) {
    ctx->tlb[i].indx = i;
    ctx->tlb[i].tag = (((t_uint64) VA_XKPHYS) << VA_V_RGN) + (((t_uint64) i) << (VA_V_VPN + 1));
    ctx->tlb[i].mask = 0;
    ctx->tlb[i].sel = 1u << VA_V_VPN;
    ctx->tlb[i].pfn0 = 0;
    ctx->tlb[i].pfn1 = 1u << VA_V_VPN;
    ctx->tlb[i].f0 = 0;
    ctx->tlb[i].f1 = 0;
    }
#if (FTLB_LNT)
for (i = 0; i < (FTLB_LNT * FTLB_SETS); i++) {
    ctx->ftlb[i].indx = i;
    ctx->ftlb[i].tag = (((t_uint64) VA_XKPHYS) << VA_V_RGN) + (((t_uint64) i) << FTLB_V_VAIDX);
    ctx->ftlb[i].mask = FTLB_PGSIZE;
    ctx->ftlb[i].sel = 1u << (FTLB_V_VAIDX - 1);
    ctx->ftlb[i].pfn0 = 0;
    ctx->ftlb[i].pfn1 = 1u << (FTLB_V_VAIDX - 1);
    ctx->ftlb[i].f0 = 0;
    ctx->ftlb[i].f1 = 0;
    }
for (i = 0; i < FTLB_LNT; i++)
    ctx->ftlb_lru[i] = 0;
#endif
tlb_inv_mtlb (ctx);
return;
}

/* Coprocessor 0 interface */

t_stat op_cop0 (CORECTX *ctx, uint32 ir)
{
uint32 rs, rt, rd, fnc;
TLBENT *tlbp;
t_uint64 t64;

rs = I_GETRS (ir);
rt = I_GETRT (ir);
rd = I_GETRD (ir);
fnc = I_GETFNC (ir);

if (rs & 0x10) {                                        /* TLB instructions */
    switch (fnc) {

        case CP0T_TLBR:
            tlb_read (ctx, get_cp0_tlbi());             /* read indexed */
            break;

        case CP0T_TLBWI:
            tlb_write_i (ctx, get_cp0_tlbi() & 0xFFF);  /* write indexed */
            break;

        case CP0T_TLBWR:
            tlb_write_r (ctx);                          /* write random */
            break;

        case CP0T_TLBP:                                 /* probe */
            tlbp = tlb_search (ctx, get_cp0_enthi());
            if (tlbp != NULL) set_cp0_tlbi(tlbp->indx);
            else set_cp0_tlbi(W_SIGN);
            break;

        case CP0T_ERET:                                 /* error return */
            PCQ_ENTRY;                                  /* yes, branch */
            if (get_cp0_sr() & CP0_SR_ERL) {            /* error level? */
                set_pc (get_cp0_errpc());               /* use err_pc */
                set_cp0_sr (get_cp0_sr() & ~CP0_SR_ERL); /* clear erl */
                }
            else {                                      /* exception level */
                set_pc (get_cp0_epc());                 /* use epc */
                set_cp0_sr (get_cp0_sr() & ~CP0_SR_EXL); /* clear exl */
                }
            lock_clear (ctx->cpu_num);                  /* clear lock */
            break;

        case CP0T_DERET:                                /* debug return */
            PCQ_ENTRY;                                  /* yes, branch */
            set_pc (get_cp0_depc());                    /* use depc */
            set_cp0_debug (get_cp0_debug() & ~CP0_DEBUG_DM);
            break;

        case CP0T_WAIT:                                 /* wait */
            ctx->events |= EVT_WAIT;
            break;

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }
        }
else {                                                  /* cop0 instructions */
    switch (rs) {

        case CP0_MFC0:                                  /* MFC0 */
            t64 = cp0_getreg (ctx, rd, fnc);
            if (rt != 0) setgpr(rt, SEXT_W_D (t64));
            break;

        case CP0_DMFC0:                                 /* DMFC0 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            t64 = cp0_getreg (ctx, rd, fnc);
            if (rt != 0) setgpr(rt, t64);
            break;

        case CP0_MTC0:                                  /* MTC0 */
            cp0_putreg (ctx, rd, fnc, gpr(rt));
            break;

        case CP0_DMTC0:                                 /* DMTC0 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            cp0_putreg (ctx, rd, fnc, gpr(rt));
            break;

#if defined (_MIPS64_R2_)
        case CP0_MFMC0:                                 /* EI, DI */
            if (rd == CP0_MFMC0_XI) {
                t64 = cp0_getreg (ctx, 12, 0);
                if (rt != 0) setgpr (rt, SEXT_W_D (t64));
                if (ir & (1u << 5))
                    t64 |= CP0_SR_IE;
                else t64 &= ~CP0_SR_IE;
                cp0_putreg (ctx, 12, 0, t64);
                break;
                }
            else ctx->traps |= TRAP_RSVI;
            break;
#endif

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }
        }
eval_intr (ctx);                                        /* eval interrupts */
return SCPE_OK;
}

/* Read CP0 register */

t_uint64 cp0_getreg (CORECTX *ctx, uint32 rn, uint32 sel)
{
sel = sel & 7;
switch (CPR_S (rn,sel)) {

    case CPR_S(0,0):                                    /* TLBI */
        return get_cp0_tlbi() & CP0_TLBI_R;

    case CPR_S(1,0):                                    /* TLBR */
        return get_cp0_tlbr() & CP0_TLBR_R;

    case CPR_S(2,0):                                    /* ENTLO0 */
        return get_cp0_entlo0() & CP0_ELO_RW;

    case CPR_S(3,0):                                    /* ENTLO1 */
        return get_cp0_entlo1() & CP0_ELO_RW;

    case CPR_S(4,0):                                    /* CONTEXT */
        return get_cp0_ctxt() & CP0_CTXT_R;

    case CPR_S(5,0):                                    /* MASK */
        return get_cp0_mask() & CP0_MASK_RW;

    case CPR_S(6,0):                                    /* WIRED */
        return get_cp0_wired() & CP0_WIRED_RW;

    case CPR_S(8,0):                                    /* BADVA */
        return get_cp0_badva();

    case CPR_S(9,0):                                    /* COUNT */
        return get_cp0_count();

    case CPR_S(10,0):                                   /* ENTHI */
        return get_cp0_enthi() & CP0_EHI_RW;

    case CPR_S(11,0):                                   /* COMPARE */
        return get_cp0_compr();

    case CPR_S(12,0):                                   /* SR */
        return get_cp0_sr() & CP0_SR_RW;

    case CPR_S(13,0):                                   /* CAUSE */
        return get_cp0_cause() & CP0_CAUSE_R;

    case CPR_S(14,0):                                   /* EPC */
        return get_cp0_epc();

    case CPR_S(15,0):                                   /* PID */
        return CP0_PID | (ctx->cpu_num << CP0_PID_V_WHAMI);

    case CPR_S(16,0):                                   /* CNF */
        return (get_cp0_cnf() | CP0_CNF_MBO) & CP0_CNF_R;

    case CPR_S(16,1):                                   /* CNF1 */
        return CP0_CNF1;

    case CPR_S(18,0):                                   /* WATCHLO */
        return get_cp0_watchlo();

    case CPR_S(19,0):                                   /* WATCHHI */
        return get_cp0_watchhi() & CP0_WHI_RW;

    case CPR_S(20,0):                                   /* XCONTEXT */
        return get_cp0_xctxt() & CP0_XCTXT_R;

    case CPR_S(23,0):                                   /* DEBUG */
        return get_cp0_debug() & CP0_DEBUG_R;

    case CPR_S(24,0):                                   /* DEPC */
        return get_cp0_depc();

    case CPR_S(25,0): case CPR_S(25,1):                 /* PERF */
    case CPR_S(25,2): case CPR_S(25,3):
    case CPR_S(25,4): case CPR_S(25,5):
    case CPR_S(25,6): case CPR_S(25,7):
        if (sel > (NUM_PERF * 2)) break;                /* valid? */
        if ((sel & 1) == 0)                             /* control? */
             set_cp0_perf(sel,                          /* not last? */
                ((sel != ((NUM_PERF - 1) * 2))? W_SIGN: 0) |
                (get_cp0_perf(sel) & CP0_PCTRL_RW));
        return get_cp0_perf(sel);

    case CPR_S(26,0):                                   /* ERRCTL */
        return get_cp0_errctl() & CP0_ERRCTL_RW;

    case CPR_S(27,0):                                   /* CCHERR */
        return get_cp0_ccherr() & CP0_CCHERR_R;

    case CPR_S(28,0):                                   /* TAGLO */
        return get_cp0_taglo() & CP0_TLO_RW;

    case CPR_S(28,1):                                   /* DATLO */
        return get_cp0_datlo();

    case CPR_S(29,0):                                   /* TAGHI */
        return 0;

    case CPR_S(29,1):                                   /* DATHI */
        return get_cp0_dathi();

    case CPR_S(30,0):                                   /* ERRPC */
        return get_cp0_errpc();

    case CPR_S(31,0):                                   /* DESAVE */
        return get_cp0_desave();

#if defined (_MIPS64_R2_)
    case CPR_S(5,1):                                    /* page grain */
        return CP0_EPA_FIX;

    case CPR_S(7,0):                                    /* HWRENA */
        return get_cp0_hwrena() & CP0_HWRENA_RW;

    case CPR_S(12,1):                                   /* INTCTL */
        return CP0_INTCTL_FIX | (get_cp0_intctl() & CP0_INTCTL_RW);

    case CPR_S(12,2):                                   /* SHRCTL */
        return 0;

    case CPR_S(15,1):                                   /* EBASE */
        return (get_cp0_ebase() | (uint32) CP0_EBASE_FIX);

    case CPR_S(16,2):                                   /* CNF2 */
        return CP0_CNF2;

    case CPR_S(16,3):                                   /* CNF3 */
        return CP0_CNF3;

#endif
#if defined (_MIPS_SCTX_)
    case CPR_S(9,7):                                    /* TIME */
        return get_cp0_time();

    case CPR_S(11,6):                                   /* SCR0 */
        return get_cp0_scr0();

    case CPR_S(11,7):                                   /* SCR1 */
        return get_cp0_scr1();

    case CPR_S(16,6):                                   /* CNF6 */
        return get_cp0_cnf6() & CP0_CNF6_RW;

    case CPR_S(16,7):                                   /* CNF7 */
        return get_cp0_cnf6() & CP0_CNF7_RW;

    case CPR_S(22,0): case CPR_S(22,1):                 /* PERF VA/PA */
    case CPR_S(22,2): case CPR_S(22,3):
    case CPR_S(22,4): case CPR_S(22,5):
    case CPR_S(22,6): case CPR_S(22,7):
        if (sel > (NUM_PERF * 2)) break;                /* valid? */
        return get_cp0_perf_ad(sel);

#endif
        }

printf ("%%ERROR: SIMH: Read reference to undefined CP0 reg %d-%d. pc is %llx\n", rn, sel, ctx->PC);
return 0;
}

/* Write CP0 register */

void cp0_putreg (CORECTX *ctx, uint32 rn, uint32 sel, t_uint64 val)
{
uint32 val32 = (uint32) val & M32;

sel = sel & 7;
switch (CPR_S (rn,sel)) {

    case CPR_S(0,0):                                    /* TLBI */
        set_cp0_tlbi ((get_cp0_tlbi() & ~CP0_TBLI_W) |
            (val32 & CP0_TBLI_W));
        return;

    case CPR_S(1,0):                                    /* TBLR */
        return;

    case CPR_S(2,0):                                    /* ENTLO0 */
        set_cp0_entlo0 (val & CP0_ELO_RW);
        return;

    case CPR_S(3,0):                                    /* ENTLO1 */
        set_cp0_entlo1 (val & CP0_ELO_RW);
        return;

    case CPR_S(4,0):                                    /* CONTEXT */
        set_cp0_ctxt ((get_cp0_ctxt() & ~CP0_CTXT_W) | (val & CP0_CTXT_W));
        return;

    case CPR_S(5,0):                                    /* MASK */
        set_cp0_mask (val32 & CP0_MASK_RW);
        return;

    case CPR_S(6,0):                                    /* WIRED */
        set_cp0_wired (val32 & CP0_WIRED_RW);
        set_cp0_tlbr (TLB_LNT - 1);                     /* reset TLBR */
        return;

    case CPR_S(8,0):                                    /* BADVA */
        return;

    case CPR_S(9,0):                                    /* COUNT */
        counter_wr_count (ctx, val);                    /* write counter */
        return;

    case CPR_S(10,0):                                   /* ENTHI */
        set_cp0_enthi (val & CP0_EHI_RW);
        tlb_inv_mtlb (ctx);                             /* inv mini-TLBs */
        return;

    case CPR_S(11,0):                                   /* COMPARE */
        counter_wr_compr (ctx, val);                    /* write compare */
        return;

    case CPR_S(12,0):                                   /* SR */
        set_cp0_sr(val32 & CP0_SR_RW);
        return;

    case CPR_S(13,0):                                   /* CAUSE */
        set_cp0_cause((get_cp0_cause() & ~CP0_CAUSE_W) |
            (val32 & CP0_CAUSE_W));
        return;

    case CPR_S(14,0):                                   /* EPC */
        set_cp0_epc(val);
        return;

    case CPR_S(15,0):                                   /* PID */
        return;

    case CPR_S(16,0):                                   /* CNF */
        set_cp0_cnf ((get_cp0_cnf() & ~CP0_CNF_W) |
            (val32 & CP0_CNF_W) | CP0_CNF_MBO);
        return;

    case CPR_S(16,1):                                   /* CNF1 */
        return;

    case CPR_S(18,0):                                   /* WATCHLO */
        set_cp0_watchlo (val);
        return;

    case CPR_S(19,0):                                   /* WATCHHI */
        set_cp0_watchhi (val32 & CP0_WHI_RW);
        return;

    case CPR_S(20,0):                                   /* XCONTEXT */
        set_cp0_xctxt ((get_cp0_xctxt() & ~CP0_XCTXT_W) |
            (val & CP0_XCTXT_W));
        return;

    case CPR_S(23,0):                                   /* DEBUG */
        set_cp0_debug ((get_cp0_debug() & ~CP0_DEBUG_W) |
            (val32 & CP0_DEBUG_W));
        return;

    case CPR_S(24,0):                                   /* DEPC */
        set_cp0_depc (val);
        return;

    case CPR_S(25,0): case CPR_S(25,1):                 /* PERF */
    case CPR_S(25,2): case CPR_S(25,3):
    case CPR_S(25,4): case CPR_S(25,5):
    case CPR_S(25,6): case CPR_S(25,7):
        if (sel > (NUM_PERF * 2)) break;                /* valid? */
        if (sel & 1)                                    /* counter? */
            set_cp0_perf(sel, val32);
        else set_cp0_perf(sel, val32 & CP0_PCTRL_RW);
        return;

    case CPR_S(26,0):                                   /* ERRCTL */
        set_cp0_errctl (val32 & CP0_ERRCTL_RW);
        return;

    case CPR_S(27,0):                                   /* CCHERR */
        return;

    case CPR_S(28,0):                                   /* TAGLO */
        set_cp0_taglo (val & CP0_TLO_RW);
        return;

    case CPR_S(28,1):                                   /* DATLO */
        set_cp0_datlo (val);
        return;

    case CPR_S(28,2):                                   /* probed */
        return;

    case CPR_S(29,0):                                   /* TAGHI */
        return;

    case CPR_S(29,1):                                   /* DATHI */
        set_cp0_dathi (val);
        return;

    case CPR_S(29,2):                                   /* probed */
        return;

    case CPR_S(30,0):                                   /* ERRPC */
        set_cp0_errpc (val);
        return;

    case CPR_S(31,0):                                   /* DESAVE */    
        set_cp0_desave (val);
        return;

#if defined (_MIPS64_R2_)
    case CPR_S(5,1):                                    /* page grain */
        return;

    case CPR_S(7,0):                                    /* HWRENA */
        set_cp0_hwrena(val32 & CP0_HWRENA_RW);
        return;

    case CPR_S(12,1):                                   /* INTCTL */
#if defined (_MIPS64_R2_VI_)
        set_cp0_intctl(CP0_INTCTL_FIX | (get_cp0_intctl() & CP0_INTCTL_RW));
#endif
        return;

    case CPR_S(15,1):                                   /* EBASE */
        set_cp0_ebase((val32 & CP0_EBASE_RW) |
		      (uint32) CP0_EBASE_FIX);
        return;

    case CPR_S(16,2):                                   /* CNF2 */
    case CPR_S(16,3):                                   /* CNF3 */
        return;

#endif
#if defined (_MIPS_SCTX_)
    case CPR_S(9,7):                                    /* TIME */
        if (get_cp0_cnf() & CP0_CNF_TIWR)
            set_cp0_time(val);
        return;

    case CPR_S(11,6):                                   /* SCR0 */
        set_cp0_scr0(val);
        return;

    case CPR_S(11,7):                                   /* SCR1 */
        set_cp0_scr1(val);
        return;

    case CPR_S(16,6):                                   /* CNF6 */
        set_cp0_cnf6(val32 & CP0_CNF6_RW);
        return;

    case CPR_S(16,7):                                   /* CNF7 */
        set_cp0_cnf7(val32 & CP0_CNF7_RW);
        return;

    case CPR_S(22,0): case CPR_S(22,1):                 /* PERF VA/PA */
    case CPR_S(22,2): case CPR_S(22,3):
    case CPR_S(22,4): case CPR_S(22,5):
    case CPR_S(22,6): case CPR_S(22,7):
        return;

#endif
        }

printf ("%%ERROR: SIMH: Write reference to undefined CP0 reg %d-%d\n", rn, sel);
return;
}
