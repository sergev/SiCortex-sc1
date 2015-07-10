/* sc1_mem.c: SiCortex 1 memory simulator

   Copyright (c) 2005-2006, SiCortex, Inc.  All rights reserved.

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

   MEM          memory hierarchy

   16-Jan-06    RMS     Added TRACE capability
   04-Jan-06    RMS     Revised for new L2 interrupt mechanism
   29-Dec-05    RMS     Removed NVR support
   13-Dec-05    RMS     CACHE instruction can cause TLB exceptions
   23-Aug-05    ADB     Adding NVRAM support
   17-Aug-05    RMS     Revised counter to work off instruction counts
   26-Jul-05    RMS     Wire up counter initialization

   Because the SC1 is a multiprocessor system on a chip, the master controller
   (and owner of system memory) is not the "CPU" but "memory" (or more properly,
   the L2 cache).
*/

#include "sc1_defs.h"
#include "sc1_eth.h"
#include "sc1_stats.h"


#if defined (_WIN32)

#define SNOOZE          sim_os_sleep (0);

#else

/* POSIXy systems have this */
#include <sched.h>

# ifdef SIMH_CPUSIMH

extern void ScxStep(unsigned int);
# define SNOOZE          do {ScxStep(1); sched_yield (); } while (0)

# else

#define SNOOZE          sched_yield ();

# endif
#endif

#define UNIT_MSIZE      (1u << UNIT_V_UF)

#ifdef SIMH_CPUSIMH
extern t_uint64 simhLLStallCpu;
#endif

t_uint64 *M = NULL;
t_uint64 lock_addr[NUM_CORES];
t_uint64 lock_last[NUM_CORES];
t_uint64 lock_try[NUM_CORES];
t_uint64 total_count = 0;                               /* global timer */
uint32 global_lock = 0;
uint32 global_stop = 0;
uint32 global_int = 0;
uint32 global_sleep = 0;
uint32 global_stall = 0;
CORECTX *cpu_ctx[NUM_CORES];

extern uint32 sim_brk_types, sim_brk_dflt, sim_brk_summ;
extern int32 sim_interval, sim_int_char, sim_switches;
extern FILE *sim_log;

t_bool lock_write (t_uint64 addr);
t_stat cpu_report_err (t_stat r0, t_stat r1, DEVICE *dptr, CORECTX *ctx);
t_stat mem_reset (DEVICE *dptr);
t_stat mem_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat mem_dep (t_value vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat mem_set_size (UNIT *uptr, int32 val, char *cptr, void *desc);

extern t_stat cpu_create (uint32 i);
extern t_stat cpu_one_inst (CORECTX *ctx);

/* MEM data structures

   mem_dev      MEM device descriptor
   mem_unit     MEM unit
   mem_reg      MEM register list
   mem_mod      MEM modifier list
*/

UNIT mem_unit = { UDATA (NULL, UNIT_FIX + UNIT_BINK, INITMEMSIZE) };

REG mem_reg[] = {
    { HRDATA (INT_REQ, global_int, 16) },
    { HRDATA (LOCK_FLAGS, global_lock, NUM_CORES) },
    { BRDATA (LOCK_ADDR, lock_addr, 16, 64, NUM_CORES) },
    { DRDATA (GCOUNT, total_count, 64) },
    { HRDATA (STOP, global_stop, 16) },
    { HRDATA (WRU, sim_int_char, 8) },
    { NULL }
    };

MTAB mem_mod[] = {
    { UNIT_MSIZE, (1u << 24), NULL, "16M", &mem_set_size },
    { UNIT_MSIZE, (1u << 25), NULL, "32M", &mem_set_size },
    { UNIT_MSIZE, (1u << 26), NULL, "64M", &mem_set_size },
    { UNIT_MSIZE, (1u << 27), NULL, "128M", &mem_set_size },
    { UNIT_MSIZE, (1u << 28), NULL, "256M", &mem_set_size },
    { UNIT_MSIZE, (1u << 29), NULL, "512M", &mem_set_size },
    { UNIT_MSIZE, (1u << 30), NULL, "1024M", &mem_set_size },
    { UNIT_MSIZE, (1u << 31), NULL, "2048M", &mem_set_size },
    { 0 }
    };

DEVICE mem_dev = {
    "MEM", &mem_unit, mem_reg, mem_mod,
    1, 16, PA_W, 8, 16, 64,
    &mem_ex, &mem_dep, &mem_reset,
    NULL, NULL, NULL,
    NULL, DEV_DYNM + DEV_64B, 0,
    NULL, &mem_set_size, NULL
    };

/* Main instruction fetch/decode loop */

t_stat sim_instr (void)
{
t_stat reason, r1;
uint32 i, num_enab;
t_bool cpu_enb[NUM_CORES];
CORECTX *ctx;
DEVICE *dptr, *dev_list[NUM_CORES];
char cname[NUM_CORES]; 

    strcpy (cname, "CPU0");
    for (i = 0, num_enab = 0; i < NUM_CORES; i++) {
	CORECTX *ctx = cpu_ctx[i];
        dptr = find_dev (cname);
        if (dptr == NULL) return SCPE_IERR;
        dev_list[i] = dptr;
        if (dptr->flags & DEV_DIS) cpu_enb[i] = FALSE;
        else {
            cpu_enb[i] = TRUE;
            num_enab++;
            }
        eval_intr (cpu_ctx[i]);
        ctx->debug = dptr->dctrl;
        set_cp0_ebase (get_cp0_ebase() & CP0_EBASE_RW);
        if (cpu_ctx[i]->hst_lnt || (cpu_ctx[i]->debug & TRAP_SIMTRC))
            cpu_ctx[i]->events |= EVT_HIST;
        else cpu_ctx[i]->events &= ~EVT_HIST;
        if (sim_brk_summ) cpu_ctx[i]->events |= EVT_BKPT;
        else cpu_ctx[i]->events &= ~EVT_BKPT;
        cname[3]++;
        }

    reason = 0;        
    while (reason == 0) {                               /* loop until halted */

        if (sim_interval <= 0) {                        /* check clock queue */
            if ((reason = sim_process_event ())) break;
            eval_intr_all ();
            }
        sim_interval = sim_interval - 1;
        global_sleep = 0;
        global_stall = 0;
        total_count++;

        reason = cpu_one_inst (cpu_ctx[0]);
	    for (i = 1/*not0*/; i < NUM_CORES; i++) {
	        if (cpu_enb[i] && (r1 = cpu_one_inst (cpu_ctx[i])))
		        reason = cpu_report_err (reason, r1, dev_list[i], cpu_ctx[i]);
	        }
        if ((global_sleep == num_enab) ||               /* everyone napping? */
            (global_stall == num_enab)) {
            SNOOZE;
            }
        }                                               /* end while */

    for (i = 0; i < NUM_CORES; i++) {
        ctx = cpu_ctx[i];
        dptr = dev_list[i];
        dptr->dctrl = ctx->debug;
        ctx->pcq_r->qptr = ctx->pcq_p;                  /* update pc q ptr */
        }
global_stop = reason;
return reason;
}

/* Stop code from non-primary core */

t_stat cpu_report_err (t_stat r0, t_stat r1, DEVICE *dptr, CORECTX *ctx)
{
if ((r0 == SCPE_OK) || (r0 == STOP_PASS))
    r0 = r1;
sim_ttcmd ();                                           /* unwedge console */
fprintf (stdout, "Core %d: ", ctx->cpu_num);
fprint_stopped_gen (stdout, r1, dptr->registers, dptr);
if (sim_log) {
    fprintf (sim_log, "Core %d: ", ctx->cpu_num);
    fprint_stopped_gen (sim_log, r1, dptr->registers, dptr);
    }
return r0;
}

/* Memory read routines */

t_bool ReadPB (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr)
{
uint32 sc;

if (PA_IS_MEM (pa)) {
    sc = (((uint32) pa) & 7) << 3;
    *val = (M[pa >> 3] >> sc) & M8;
    STATS_READPB(ctx, pa, *val, catr);
    return TRUE;
    }
if (CALL_READIO (ctx, pa, val, L_BYTE)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_bool ReadPH (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr)
{
uint32 sc;

if (PA_IS_MEM (pa)) {
    sc = (((uint32) pa) & 6) << 3;
    *val = (M[pa >> 3] >> sc) & M16;
    STATS_READPH(ctx, pa, *val, catr);
    return TRUE;
    }
if (CALL_READIO (ctx, pa, val, L_HALF)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_bool ReadPW (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr)
{
if (PA_IS_MEM (pa)) {
    if (pa & 4) *val = (M[pa >> 3] >> 32) & M32;
    else *val = M[pa >> 3] & M32;
    STATS_READPW(ctx, pa, *val, catr);
    return TRUE;
    }
if (CALL_READIO (ctx, pa, val, L_WORD)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_bool ReadPD (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr)
{
if (PA_IS_MEM (pa)) {
    *val = M[pa >> 3];
    STATS_READPD(ctx, pa, *val, catr);
    return TRUE;
    }
if (CALL_READIO (ctx, pa, val, L_DOUB)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

/* Istream reads split out to allow different cache treatment */

t_bool ReadPI (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr)
{
if (PA_IS_MEM (pa)) {
    if (pa & 4) *val = (M[pa >> 3] >> 32) & M32;
    else *val = M[pa >> 3] & M32;
    STATS_READPI(ctx, pa, *val, catr);
    return TRUE;
    }
if (CALL_READIO (ctx, pa, val, L_WORD)) return TRUE;
ctx->traps |= TRAP_IBE;
return FALSE;
}

/* Memory write routines */

t_stat WritePB (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr)
{
uint32 sc;
t_uint64 mask;

if (global_lock) lock_write (pa);
if (PA_IS_MEM (pa)) {
    sc = (((uint32) pa) & 7) << 3;
    mask = ((t_uint64) M8) << sc;
    M[pa >> 3] = (M[pa >> 3] & ~mask) | ((dat << sc) & mask);
    STATS_WRITEPB(ctx, pa, dat, catr);
    return TRUE;
    }
if (CALL_WRITEIO (ctx, pa, dat, L_BYTE)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_stat WritePH (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr)
{
uint32 sc;
t_uint64 mask;

if (global_lock) lock_write (pa);
if (PA_IS_MEM (pa)) {
    sc = (((uint32) pa) & 6) << 3;
    mask = ((t_uint64) M16) << sc;
    M[pa >> 3] = (M[pa >> 3] & ~mask) | ((dat << sc) & mask);
    STATS_WRITEPH(ctx, pa, dat, catr);
    return TRUE;
    }
if (CALL_WRITEIO (ctx, pa, dat, L_HALF)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_stat WritePW (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr)
{
if (global_lock) lock_write (pa);
if (PA_IS_MEM (pa)) {
    if (pa & 4) M[pa >> 3] = (M[pa >> 3] & M32) |
        (dat << 32);
    else M[pa >> 3] = (M[pa >> 3] & ~((t_uint64) M32)) | (dat & M32);
    STATS_WRITEPW(ctx, pa, dat, catr);
    return TRUE;
    }
if (CALL_WRITEIO (ctx, pa, dat, L_WORD)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

t_stat WritePD (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr)
{
if (global_lock) lock_write (pa);
if (PA_IS_MEM (pa)) {
    M[pa >> 3] = dat;
    STATS_WRITEPD(ctx, pa, dat, catr);
    return TRUE;
    }
if (CALL_WRITEIO (ctx, pa, dat, L_DOUB)) return TRUE;
ctx->traps |= TRAP_DBE;
return FALSE;
}

/* Prefetch, cache, and synchronization routines - stubs for now */

void mem_pref (CORECTX *ctx, t_uint64 pa, uint32 catr, uint32 hint)
{
return;
}

__WEAK t_bool mem_cache (CORECTX *ctx, uint32 ir, t_uint64 va, uint32 hint)
{
uint32 catr;
t_uint64 pa;

if (Q_MD_U32) va = SEXT_W_D (va);
if (!xlate_va (ctx, va, VA_DR, &pa, &catr)) return FALSE; 
return TRUE;
}

__WEAK void mem_sync (CORECTX *ctx)
{
STATS_SYNC(ctx);
return;
}

/* Lock routines */

t_bool lock_reset (uint32 num)
{
lock_clear(num);
if (num < NUM_CORES) {
    lock_try[num]  = 0; 
    lock_last[num] = 0;
    lock_addr[num] = 0;
    }
return TRUE;
}

t_bool lock_test (uint32 num)
{
if ((num < NUM_CORES) && (global_lock & (1u << num))) {
    lock_try[num] = 0; lock_last[num] = 0;
    return TRUE;
    }
return FALSE;
}

t_bool lock_clear (uint32 num)
{
if (num < NUM_CORES) global_lock &= ~(1u << num);
return FALSE;
}

t_bool lock_set (uint32 num, t_uint64 addr, uint32 catr)
{
if (num < NUM_CORES) {
    global_lock |= (1u << num);
    lock_addr[num] = addr;
    if (CA_UNCACHED (catr)) {
        // Load lock to uncached space is undefined in the architecture.
	// Gripe if anyone tries this.
	fprintf (stderr, "SIMH : Warning: LL or LLD from uncached address space, addr=%08llx, catr=%d.\r\n", addr, catr);
    }

    if (lock_last[num] == addr) {
	lock_try[num]++;
#ifdef SIMH_CPUSIMH
        if (simhLLStallCpu && (lock_try[num] == simhLLStallCpu)) {
	    fprintf (stderr, "SIMH: %dth ll to same address causing stall until end of epoch\n", (int)(simhLLStallCpu));
	    cpu_ctx[num]->events |= EVT_STALL_EPOCH;
	    lock_try[num] = 0;
        }
#endif
        if (lock_try[num] > MAX_STARVE) {
            fprintf(stderr, "SIMH : Possible lock starve: cpu %d has done %lld load-locks at addr 0x%llx\n\r", num, lock_try[num], addr);
	    lock_try[num] = 0;
            }
        }
    else {
	lock_try[num] = 0;
	lock_last[num] = addr; 
        }

    return TRUE;
    }
return FALSE;
}

t_bool lock_write (t_uint64 addr)
{
uint32 i;

for (i = 0; i < NUM_CORES; i++) {
    if (((global_lock >> i) & 1) && (lock_addr[i] ^ addr) == 0) {
        global_lock &= ~(1u << i);
        lock_addr[i] = 0;
        }
    }
return TRUE;
}

#ifdef SIMH_CPUSIMH

/* Memory reset */

extern int simhMem;		/* the "fast" linux test uses simh memory */

t_stat mem_reset (DEVICE *dptr)
{
    int32 i;
    t_stat r;

    sim_brk_types = sim_brk_dflt = SWMASK ('E');
    global_lock = 0;

    if (M == NULL && simhMem) {
        M = (t_uint64 *) calloc ((uint32) (mem_unit.capac >> 3), sizeof (t_uint64));
        if (M == NULL) return SCPE_MEM;
    }

    /* We leak here on reset */
    for (i = 1; i < NUM_CORES; i++) {
        if ((r = cpu_create (i))) return r;
    }

    return SCPE_OK;
}

/* Memory allocation */

t_stat mem_set_size (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    t_uint64 i, clim, mc = 0, sz;
    t_uint64 *nM = NULL;

    if ((val == 0) || (val & 0xFFFFFF)) return SCPE_ARG;

    if (simhMem) {
        sz = val;
        for (i = sz; i < MEMSIZE; i = i + 8) mc = mc | M[i >> 3];
        if ((mc != 0) && !get_yn ("Really truncate memory [N]?", FALSE))
            return SCPE_OK;
        nM = (t_uint64 *) calloc ((uint32) (sz >> 3), sizeof (t_uint64));
        if (nM == NULL) return SCPE_MEM;
        clim = (sz < MEMSIZE)? sz: MEMSIZE;
        for (i = 0; i < clim; i = i + 8) nM[i >> 3] = M[i >> 3];
        free (M);
        M = nM;
    }

    MEMSIZE = val;
    return SCPE_OK;
}

#else // SIMH_CPUSIMH

/* Memory reset */

t_stat mem_reset (DEVICE *dptr)
{
int32 i;
t_stat r;

sim_brk_types = sim_brk_dflt = SWMASK ('E');
global_lock = 0;
if (M == NULL) {
    M = (t_uint64 *) calloc ((uint32) (mem_unit.capac >> 3), sizeof (t_uint64));
    if (M == NULL) return SCPE_MEM;
    for (i = 1; i < NUM_CORES; i++) {
        if ((r = cpu_create (i))) return r;
        }
    }
if (sim_switches & SWMASK ('P'))                        /* power up? */
    total_count = 0;                                    /* clear global time */
return SCPE_OK;
}

/* Memory allocation */

t_stat mem_set_size (UNIT *uptr, int32 val, char *cptr, void *desc)
{
t_uint64 i, clim, mc = 0, sz;
t_uint64 *nM = NULL;

if ((val == 0) || (val & 0xFFFFFF)) return SCPE_ARG;
sz = (uint32) val; // don't you dare sign extend me
if (sz > (1ULL << 32)) {
    fprintf (stderr, "%%Error: int32 vs uint64 rounding bug. now memory is going to be %lld MB!\n", sz/1024/1024);
}
for (i = sz; i < MEMSIZE; i = i + 8) mc = mc | M[i >> 3];
if ((mc != 0) && !get_yn ("Really truncate memory [N]?", FALSE))
    return SCPE_OK;
nM = (t_uint64 *) calloc ((uint32) (sz >> 3), sizeof (t_uint64));
if (nM == NULL) return SCPE_MEM;
clim = (sz < MEMSIZE)? sz: MEMSIZE;
for (i = 0; i < clim; i = i + 8) nM[i >> 3] = M[i >> 3];
free (M);
M = nM;
MEMSIZE = sz;
return SCPE_OK;
}

#endif // SIMH_CPUSIMH

/* Memory examine */

t_stat mem_ex (t_value *vptr, t_addr pa, UNIT *uptr, int32 sw)
{
DEVICE *dptr = find_dev_from_unit (uptr);

if (dptr == NULL) return SCPE_IERR;
if (vptr == NULL) return SCPE_ARG;
if (PA_IS_MEM (pa) && CALL_READPD ((CORECTX *) dptr->ctxt, pa, vptr, CA_CACHED)) return SCPE_OK;
else if (CALL_READIO ((CORECTX *) dptr->ctxt, pa, vptr, L_DOUB)) return SCPE_OK;
return SCPE_NXM;
}

/* Memory deposit */

t_stat mem_dep (t_value val, t_addr pa, UNIT *uptr, int32 sw)
{
DEVICE *dptr = find_dev_from_unit (uptr);

if (dptr == NULL) return SCPE_IERR;
if (PA_IS_MEM (pa) && CALL_WRITEPD ((CORECTX *) dptr->ctxt, pa, val, CA_CACHED)) return SCPE_OK;
else if (CALL_WRITEIO ((CORECTX *) dptr->ctxt, pa, val, L_DOUB)) return SCPE_OK;
return SCPE_NXM;
}
