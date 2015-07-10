/* sc1_cpu.c: SiCortex 1 CPU core simulator

   Copyright (c) 2005-2007, SiCortex, Inc.  All rights reserved.

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

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   cpu0..cpun   CPU cores

   25-Oct-07    RMS     Added PS support
   28-Sep-07    RMS     Added Mips64 R2 support
   16-Jan-06    RMS     Added TRACE capability
   04-Jan-06    RMS     Revised for new L2 interrupt mechanism
   13-Dec-05    RMS     CACHE instruction can cause TLB exceptions
   18-Nov-05    RMS     Fixed Windows sleep call
                        Removed address checks in LDXC1, SDXC1, LWXC1, SWXC1,
                         done in MMU subroutines
                        All COP1X instructions trap in 32b mode
                        Fixed SWL, SWR, SDL, SDL MMU exception codes
                        CAUSE<BD> does not update if SR<EXL> set
   16-Nov-05    RMS     Fixed SWXC1, SDXC1, SUXC1 source operand is fs
   24-Oct-05    RMS     LWU, SDL, SDR, DCLZ, DCLO trap in 32b mode
   16-Sep-05    RMS     Fixed stop on trap support
                        Coprocessor unit number not set on loads and stores
                        Coprocessor unusable not tested on MOVC1
   07-Sep-05    RMS     Fixed cause<IV> test in interrupts
   06-Sep-05    RMS     Fixed nullified branch not clearing delay count
   01-Sep-05    RMS     Added "stop on trap" support
   25-Aug-05    RMS     Fix DDIV case of largest negative / -1
   17-Aug-05    RMS     Fixed interrupts
   26-Jul-05    RMS     Wired up counter service

   The SiCortex 1 uses a modified Mips 5KF core, with the following state:

   PC<63:0>                     program counter
   R[0:31]<63:0>                integer registers
   F[0:31]<63:0>                floating registers
   MHI                          multiply/divide high
   MLO                          multiply/divide low
   FPCR<31:0>                   floating point control register
   lock_flag<varies>            load_locked flag

   All 5KF instructions are 32b wide.  There are four basic formats: jump,
   memory reference, integer operate, and floating operate.

    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           |                                                   |
   |   opcode  |                   jump offset                     | jump
   |           |                                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           |         |         |                               |
   |   opcode  |    Rs   |    Rt   |      address displacement     | memory
   |           |         |         |                               | reference
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           |         |         |         |         |           |
   |   opcode  |    Rs   |    Rt   |    Rd   |    SA   |  function | integer
   |           |         |         |         |         |           | operate
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
    1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           |         |         |         |         |           |
   |   opcode  |   fmt   |    Ft   |    Fs   |    Fd   |  function | floating
   |           |         |         |         |         |           | operate
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   Memory reference format is also used for some two-operand operates --
   the address displacement is a 16b immediate -- and for branches --
   the address displacement is a branch displacement

   This routine is the instruction decode routine for the SC1.  It
   is called from the main loop to execute one instruction.

   General notes:
*/

#include "sc1_defs.h"
#include "sc1_nvr.h"
#if !defined (_WIN32)
#include "sc1_eth.h"
#include "sc1_magic_pipe.h"
#include "sc1_gdb.h"
#endif
#include "sc1_stats.h"
#include "sc1_defs.h"
#include "sc1_sys.h"
#if defined (_WIN32)
# include "ice9_magic_spec_sw.h"
#elif defined (SIMX_NATIVE) /* Built into simx */
# include "ice9_magic_spec_sw.h"
# include "ScxGlobal.h"
#else
# include "sicortex/ice9/ice9_magic_spec_sw.h"
#endif

/* Instruction history field flags and instruction formats */

#define H_RS    0x01
#define H_RT    0x02
#define H_RD    0x04
#define H_BA    0x08
#define H_MA    0x10
#define H_JA    0x20
#define H_L16   0x40

#define H_OPR   (H_RS|H_RT|H_RD)
#define H_RGI   (H_RS|H_L16)
#define H_IMM   (H_RS|H_RT|H_L16)
#define H_JMP   (H_JA)
#define H_BRA   (H_RS|H_RT|H_BA)
#define H_BR1   (H_RS|H_BA)
#define H_MRF   (H_RS|H_RT|H_MA)
#define H_NOP   (0)

#define ADD_OV_W(r,s1,s2)       (((~(s1) ^ (s2)) & ((s1) ^ (r))) & W_SIGN)
#define ADD_OV_D(r,s1,s2)       (((~(s1) ^ (s2)) & ((s1) ^ (r))) & D_SIGN)
#define SUB_OV_W(r,s1,s2)       ((((s1) ^ (s2)) & (~(s2) ^ (r))) & W_SIGN)
#define SUB_OV_D(r,s1,s2)       ((((s1) ^ (s2)) & (~(s2) ^ (r))) & D_SIGN)

t_uint64 uemul64 (t_uint64 a, t_uint64 b, t_uint64 *hi);
t_uint64 cpu_ins (t_uint64 d, t_uint64 s, uint32 msb, uint32 lsb);
t_uint64 cpu_ext (t_uint64 s, uint32 sizem1, uint32 lsb);
t_stat cpu_reset (DEVICE *dptr);
t_stat cpu_boot (int32 unitno, DEVICE *dptr);
t_stat cpu_detach (UNIT *uptr);
t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_set_hist (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat cpu_show_virt (FILE *st, UNIT *uptr, int32 val, void *desc);
t_stat cpu_show_tlb (FILE *st, UNIT *uptr, int32 val, void *desc);
void cpu_fprint_one_inst (FILE *st, uint32 ir, t_uint64 pc,
    t_uint64 rs, t_uint64 rt, t_uint64 rd, uint32 coreno);

extern t_uint64 cp0_getspr (CORECTX *ctx, uint32 rn);
extern void cp0_putspr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern t_uint64 fp_getcr (CORECTX *ctx, uint32 rn);
extern t_uint64 fp_getspr (CORECTX *ctx, uint32 rn);
extern t_uint64 fp_getdpr (CORECTX *ctx, uint32 rn);
extern void fp_putcr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern void fp_putspr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern void fp_putdpr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern t_uint64 cp2_getspr (CORECTX *ctx, uint32 rn);
extern t_uint64 cp2_getdpr (CORECTX *ctx, uint32 rn);
extern void cp2_putspr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern void cp2_putdpr (CORECTX *ctx, uint32 rn, t_uint64 val);
extern t_bool fp_testcc (CORECTX *ctx, uint32 ccn);
extern t_stat op_cop0 (CORECTX *ctx, uint32 ir);
extern t_stat op_cop1 (CORECTX *ctx, uint32 ir);
extern t_stat op_cop1x (CORECTX *ctx, uint32 ir);
extern t_stat op_cop1x_alnv (CORECTX *ctx, uint32 ir);
extern t_stat op_cop2 (CORECTX *ctx, uint32 ir);
extern void mem_pref (CORECTX *ctx, t_uint64 pa, uint32 catr, uint32 hint);
extern t_bool mem_cache (CORECTX *ctx, uint32 ir, t_uint64 va, uint32 hint);
extern void mem_sync (CORECTX *ctx);
extern t_bool tlb_set_aer (CORECTX *ctx, t_uint64 va, uint32 mode);
extern void tlb_init (CORECTX *ctx);
extern uint32 trace_fetch_reg (CORECTX *ctx, uint32 inst, t_uint64 *reg);
extern void trace_fprint_one_inst (FILE *st, uint32 inst, t_uint64 pc,
    uint32 fl, t_uint64 *reg, uint32 coreno);

extern int sim_isatty (void);

extern uint32 sim_brk_summ;
extern int32  sim_switches;
extern DEVICE *sim_devices[];
extern CORECTX *cpu_ctx[NUM_CORES];
extern DEVICE *sim_dflt_dev;
extern UNIT mem_unit;
extern uint32 global_sleep;
extern uint32 global_stall;
extern t_uint64 total_count;
extern FILE *sim_deb;
#ifdef ENABLE_SC1_PERFMODEL
t_uint64 perfmodel_magic_instr (int cpu, t_uint64 r4, t_uint64 r5, t_uint64 r6);
#endif

t_stat trap_cause[31] = {
 SCPE_IERR,
 CAUSE_IBE, CAUSE_DBE,
 CAUSE_MCHK, 0,
 CAUSE_CPU, CAUSE_SYS, CAUSE_BRK, CAUSE_RI,
 CAUSE_OV, CAUSE_TR, CAUSE_FPE, CAUSE_ADEL,
 CAUSE_ADES, CAUSE_TLBL, CAUSE_TLBS, CAUSE_TLBL,
 CAUSE_TLBS, CAUSE_MOD, SCPE_IERR, SCPE_IERR,
 SCPE_IERR, SCPE_IERR, SCPE_IERR, SCPE_IERR,
 SCPE_IERR, SCPE_IERR, SCPE_IERR, SCPE_IERR,
 SCPE_IERR, SCPE_IERR
 };

char *causes[] = {
/* 0 */     "interrupt",
/* 1 */     "TLB mod",
/* 2 */     "TLB ld/ex",
/* 3 */     "TLB store",
/* 4 */     "addr err load/fetch",
/* 5 */     "addr err store",
/* 6 */     "bus err load/fetch",
/* 7 */     "bus err store",
/* 8 */     "SYSCALL",
/* 9 */     "BREAK",
/* 10*/     "reserved inst",
/* 11*/     "coproc unusable",
/* 12*/     "int overflow",
/* 13*/     "TRAP instr",
/* 14*/     "  ",
/* 15*/     "fp exception",
/* 16*/     "  ",
/* 17*/     "  ",
/* 18*/     "coproc 2 exc",
/* 19*/     "  ",
/* 20*/     "  ",
/* 21*/     "  ",
/* 22*/     "MDMX exc",
/* 23*/     "watch exc",
/* 24*/     "machine check",
/* 25*/     "  ",
/* 26*/     "  ",
/* 27*/	    "  ",
/* 28*/     "  ",
/* 29*/     "  ",
/* 30*/     "cache err" };

char sc1_stop_trap[32];

/* Vectored interrupt priority encoding table */

#if defined (_MIPS64_R2_VI_)
static uint8 vec_pri_enc[256] = {
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };
#endif

/* CPU0 data structures (all others are created dynamically)

   cpu0_dev     CPU0 device descriptor
   cpu0_unit    CPU0 unit
   cpu0_reg     CPU0 register list
   cpu0_mod     CPU0 modifier list
   cpu0_deb     CPU0 debug list
*/

DEBTAB cpu0_deb[] = {
    { "SIER", TRAP_SIER },
    { "IBE", TRAP_IBE },
    { "DBE", TRAP_DBE },
    { "MCHK", TRAP_MCHK },
    { "DBP", TRAP_DBP },
    { "CPUN", TRAP_CPUN },
    { "SYSCALL", TRAP_SYSCALL },
    { "BREAK", TRAP_BREAK },
    { "RSVI", TRAP_RSVI },
    { "IOV", TRAP_IOV },
    { "TRAP", TRAP_TRAP },
    { "FPE", TRAP_FPE },
    { "LAER", TRAP_LAER },
    { "SAER", TRAP_SAER },
    { "LTLBM", TRAP_LTLBM },
    { "STLBM", TRAP_STLBM },
    { "LTLBV", TRAP_LTLBV },
    { "STLBV", TRAP_STLBV },
    { "STLBW", TRAP_STLBW },
    { "TRACE", TRAP_SIMTRC },
    { NULL, 0 },
    };

CORECTX cpu0_ctx;

UNIT cpu0_unit = { UDATA (NULL, 0, 0) };

REG cpu0_reg[] = {
    { HRDATA (PC, cpu0_ctx.PC, 64), PV_LEFT },
    { HRDATA (DELAY_PC, cpu0_ctx.delay_PC, 64), PV_LEFT },
    { HRDATA (R0, cpu0_ctx.R[0], 64), REG_RO },
    { HRDATA (R1, cpu0_ctx.R[1], 64) },
    { HRDATA (R2, cpu0_ctx.R[2], 64) },
    { HRDATA (R3, cpu0_ctx.R[3], 64) },
    { HRDATA (R4, cpu0_ctx.R[4], 64) },
    { HRDATA (R5, cpu0_ctx.R[5], 64) },
    { HRDATA (R6, cpu0_ctx.R[6], 64) },
    { HRDATA (R7, cpu0_ctx.R[7], 64) },
    { HRDATA (R8, cpu0_ctx.R[8], 64) },
    { HRDATA (R9, cpu0_ctx.R[9], 64) },
    { HRDATA (R10, cpu0_ctx.R[10], 64) },
    { HRDATA (R11, cpu0_ctx.R[11], 64) },
    { HRDATA (R12, cpu0_ctx.R[12], 64) },
    { HRDATA (R13, cpu0_ctx.R[13], 64) },
    { HRDATA (R14, cpu0_ctx.R[14], 64) },
    { HRDATA (R15, cpu0_ctx.R[15], 64) },
    { HRDATA (R16, cpu0_ctx.R[16], 64) },
    { HRDATA (R17, cpu0_ctx.R[17], 64) },
    { HRDATA (R18, cpu0_ctx.R[18], 64) },
    { HRDATA (R19, cpu0_ctx.R[19], 64) },
    { HRDATA (R20, cpu0_ctx.R[20], 64) },
    { HRDATA (R21, cpu0_ctx.R[21], 64) },
    { HRDATA (R22, cpu0_ctx.R[22], 64) },
    { HRDATA (R23, cpu0_ctx.R[23], 64) },
    { HRDATA (R24, cpu0_ctx.R[24], 64) },
    { HRDATA (R25, cpu0_ctx.R[25], 64) },
    { HRDATA (R26, cpu0_ctx.R[26], 64) },
    { HRDATA (R27, cpu0_ctx.R[27], 64) },
    { HRDATA (R28, cpu0_ctx.R[28], 64) },
    { HRDATA (R29, cpu0_ctx.R[29], 64) },
    { HRDATA (R30, cpu0_ctx.R[30], 64) },
    { HRDATA (R31, cpu0_ctx.R[31], 64) },
    { HRDATA (F0, cpu0_ctx.F[0], 64) },
    { HRDATA (F1, cpu0_ctx.F[1], 64) },
    { HRDATA (F2, cpu0_ctx.F[2], 64) },
    { HRDATA (F3, cpu0_ctx.F[3], 64) },
    { HRDATA (F4, cpu0_ctx.F[4], 64) },
    { HRDATA (F5, cpu0_ctx.F[5], 64) },
    { HRDATA (F6, cpu0_ctx.F[6], 64) },
    { HRDATA (F7, cpu0_ctx.F[7], 64) },
    { HRDATA (F8, cpu0_ctx.F[8], 64) },
    { HRDATA (F9, cpu0_ctx.F[9], 64) },
    { HRDATA (F10, cpu0_ctx.F[10], 64) },
    { HRDATA (F11, cpu0_ctx.F[11], 64) },
    { HRDATA (F12, cpu0_ctx.F[12], 64) },
    { HRDATA (F13, cpu0_ctx.F[13], 64) },
    { HRDATA (F14, cpu0_ctx.F[14], 64) },
    { HRDATA (F15, cpu0_ctx.F[15], 64) },
    { HRDATA (F16, cpu0_ctx.F[16], 64) },
    { HRDATA (F17, cpu0_ctx.F[17], 64) },
    { HRDATA (F18, cpu0_ctx.F[18], 64) },
    { HRDATA (F19, cpu0_ctx.F[19], 64) },
    { HRDATA (F20, cpu0_ctx.F[20], 64) },
    { HRDATA (F21, cpu0_ctx.F[21], 64) },
    { HRDATA (F22, cpu0_ctx.F[22], 64) },
    { HRDATA (F23, cpu0_ctx.F[23], 64) },
    { HRDATA (F24, cpu0_ctx.F[24], 64) },
    { HRDATA (F25, cpu0_ctx.F[25], 64) },
    { HRDATA (F26, cpu0_ctx.F[26], 64) },
    { HRDATA (F27, cpu0_ctx.F[27], 64) },
    { HRDATA (F28, cpu0_ctx.F[28], 64) },
    { HRDATA (F29, cpu0_ctx.F[29], 64) },
    { HRDATA (F30, cpu0_ctx.F[30], 64) },
    { HRDATA (F31, cpu0_ctx.F[31], 64) },
    { HRDATA (MLO, cpu0_ctx.mlo, 64) },
    { HRDATA (MHI, cpu0_ctx.mhi, 64) },
    { HRDATA (CTXT, cpu0_ctx.cp0_ctxt, 64) },
    { HRDATA (XCTXT, cpu0_ctx.cp0_xctxt, 64) },
    { HRDATA (BADVA, cpu0_ctx.cp0_badva, 64) },
    { HRDATA (EPC, cpu0_ctx.cp0_epc, 64) },
    { HRDATA (DEPC, cpu0_ctx.cp0_depc, 64) },
    { HRDATA (ERRPC, cpu0_ctx.cp0_errpc, 64) },
    { HRDATA (IMTLB_TAG, cpu0_ctx.i_mtlb_tag, 64) },
    { HRDATA (IMTLB_PFN, cpu0_ctx.i_mtlb_pfn, 64) },
    { HRDATA (IMTLB_FL, cpu0_ctx.i_mtlb_fl, 8) },
    { HRDATA (DMTLB_TAG, cpu0_ctx.d_mtlb_tag, 64) },
    { HRDATA (DMTLB_PFN, cpu0_ctx.d_mtlb_pfn, 64) },
    { HRDATA (DMTLB_FL, cpu0_ctx.d_mtlb_fl, 8) },
    { HRDATA (TLBI, cpu0_ctx.cp0_tlbi, 32) },
    { HRDATA (TLBR, cpu0_ctx.cp0_tlbr, 6) },
    { HRDATA (WIRED, cpu0_ctx.cp0_wired, 6) },
    { HRDATA (ENTLO0, cpu0_ctx.cp0_entlo0, 64) },
    { HRDATA (ENTLO1, cpu0_ctx.cp0_entlo1, 64) },
    { HRDATA (ENTHI, cpu0_ctx.cp0_enthi, 64) },
    { HRDATA (MASK, cpu0_ctx.cp0_mask, 33) },
    { HRDATA (COUNT, cpu0_ctx.cp0_count, 32) },
    { HRDATA (COMPARE, cpu0_ctx.cp0_compr, 32) },
    { HRDATA (SR, cpu0_ctx.cp0_sr, 32) },
    { HRDATA (CAUSE, cpu0_ctx.cp0_cause, 32) },
    { HRDATA (CONFIG, cpu0_ctx.cp0_cnf, 32) },
    { HRDATA (WATCHLO, cpu0_ctx.cp0_watchlo, 64) },
    { HRDATA (WATCHHI, cpu0_ctx.cp0_watchhi, 32) },
    { HRDATA (DEBUG, cpu0_ctx.cp0_debug, 32) },
#if defined (_MIPS64_R2_)
    { HRDATA (EBASE, cpu0_ctx.cp0_ebase, 30) },
    { HRDATA (HWRENA, cpu0_ctx.cp0_hwrena, 32) },
    { HRDATA (INTCTL, cpu0_ctx.cp0_intctl, 32) },
#else
    { BRDATA (PERF_AD, cpu0_ctx.cp0_perf_ad, 16, 64, NUM_PERF * 2) },
    { BRDATA (PERF_REG, cpu0_ctx.cp0_perf, 16, 32, NUM_PERF * 2) },
    { HRDATA (PERF_CTRL0, cpu0_ctx.cp0_perf[CP0_PCTRL0], 32) },
    { HRDATA (PERF_CNTR0, cpu0_ctx.cp0_perf[CP0_PCNTR0], 32) },
    { HRDATA (PERF_CTRL1, cpu0_ctx.cp0_perf[CP0_PCTRL1], 32) },
    { HRDATA (PERF_CNTR1, cpu0_ctx.cp0_perf[CP0_PCNTR1], 32) },
#endif
#if defined (_MIPS_SCTX_)
    { HRDATA (TIME, cpu0_ctx.cp0_time, 64) },
    { HRDATA (SCR0, cpu0_ctx.cp0_scr0, 64) },
    { HRDATA (SCR1, cpu0_ctx.cp0_scr1, 64) },
    { HRDATA (CNF6, cpu0_ctx.cp0_cnf6, 32) },
    { HRDATA (CNF7, cpu0_ctx.cp0_cnf7, 32) },
#endif
    { HRDATA (ERRCTL, cpu0_ctx.cp0_errctl, 32) },
    { HRDATA (CCHERR, cpu0_ctx.cp0_ccherr, 32) },
    { HRDATA (DATLO, cpu0_ctx.cp0_datlo, 64) },
    { HRDATA (DATHI, cpu0_ctx.cp0_dathi, 64) },
    { HRDATA (TAGLO, cpu0_ctx.cp0_taglo, 64) },
    { HRDATA (DESAVE, cpu0_ctx.cp0_desave, 64) },
    { HRDATA (FPCR, cpu0_ctx.fpcr, 32) },
    { HRDATA (DELAY, cpu0_ctx.delay, 2) },
    { FLDATA (TAKEN, cpu0_ctx.taken, 0) },
    { FLDATA (NULLIFY, cpu0_ctx.events, EVT_V_NLFY) },
    { HRDATA (TRAPS, cpu0_ctx.traps, 32) },
    { HRDATA (EVENTS, cpu0_ctx.events, 8) },
    { BRDATA (ICR, cpu0_ctx.cac_icr, 16, 16, 2 * INT_N_HLVLS) },
    { HRDATA (SLOW_MASK, cpu0_ctx.cac_slow_mask, 16) },
    { GRDATA (SLOW_LOCAL, cpu0_ctx.cac_slow_local, 16, 4, 8) },
    { HRDATA (L2ERRADDR, cpu0_ctx.cac_L2EccAddr, 36) },
    { HRDATA (CSWERRADDR, cpu0_ctx.cac_CSWEccAddr, 36) },
    { HRDATA (TAGERRADDR, cpu0_ctx.cac_TagEccAddr, 36) },
    { HRDATA (ECCMODE, cpu0_ctx.cac_EccMode, 6) },
    { HRDATA (ECCSTAT, cpu0_ctx.cac_EccStat, 9) },
    { HRDATA (ECCSYND, cpu0_ctx.cac_EccSynd, 16) },
    { BRDATA (TLB, cpu0_ctx.tlb, 16, 64, TLB_LNT * (sizeof (TLBENT) / 8)) },
#if (FTLB_LNT)
    { BRDATA (FTLB, cpu0_ctx.ftlb, 16, 64, FTLB_LNT * FTLB_SETS * (sizeof (TLBENT) / 8)) },
    { BRDATA (FLRU, cpu0_ctx.ftlb_lru, 16, 8, FTLB_LNT * FTLB_SETS) },
#endif
    { BRDATA (PCQ, cpu0_ctx.pcq, 16, 64, PCQ_SIZE), REG_RO+REG_CIRC },
    { HRDATA (PCQP, cpu0_ctx.pcq_p, 6), REG_HRO },
    { NULL }
    };

MTAB cpu0_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "HISTORY", "HISTORY",
      &cpu_set_hist, &cpu_show_hist },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "VIRTUAL", NULL,
      NULL, &cpu_show_virt },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "TLB", NULL,
      NULL, &cpu_show_tlb },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 1, "FTLB", NULL,
      NULL, &cpu_show_tlb },
    { 0 }
    };

DEVICE cpu0_dev = {
    "CPU0", &cpu0_unit, cpu0_reg, cpu0_mod,
    1, 16, 64, 8, 16, 64,
    &cpu_ex, &cpu_dep, &cpu_reset,
    &cpu_boot, NULL, &cpu_detach,
    (void *) &cpu0_ctx, DEV_64B|DEV_DEBUG, 0,
    cpu0_deb,
    };

/* Detect excessive consecutive nops as runaway train */
t_uint64 num_nops    = 0;		/* consecutive nops */
t_bool   msg_nops    = FALSE;		

t_stat cpu_one_inst (CORECTX *ctx)
{
static t_stat reason;
static int32 i;
static uint32 ir, op, rs, rt, rd, fd, fs, fnc, sa, catr;
static t_int64 s1, s2, sres;
static t_uint64 ea, dsp, res, us1, us2, t64, mask, pa;
#ifdef TRAP_PRINT
static int tdebug = 1;
#endif

#ifndef ENABLE_SC1_PERFMODEL
/* Performance model wants to increment the cycle counter on its
   own terms. */
set_cp0_time ((get_cp0_time() + 1));
set_cp0_count ((get_cp0_count() + 1) & M32);
if (get_cp0_count() == get_cp0_compr()) {                 /* cntr overflow? */
    ctx->irq_count = 1;
    eval_intr (ctx);
    }
#endif

#if defined (PERF_COUNTER)
if (Q_MD_U) {                                           /* user mode? */
    if (ctx->cp0_perf[CP0_PCTRL0] & CP0_PCTRL_U) {
        uint32 evt = CP0_PCTRL_GETEVT (ctx->cp0_perf[CP0_PCTRL0]);
        if (evt == CP0_PEVT_CYC) ctx->cp0_perf[CP0_PCNTR0] += 2;
        if (evt == CP0_PEVT_INS) ctx->cp0_perf[CP0_PCNTR0] += 1;
        if (ctx->cp0_perf[CP0_PCNTR0] & W_SIGN) eval_intr (ctx);
        }
    if (ctx->cp0_perf[CP0_PCTRL1] & CP0_PCTRL_U) {
        uint32 evt = CP0_PCTRL_GETEVT (ctx->cp0_perf[CP0_PCTRL1]);
        if (evt == CP0_PEVT_CYC) ctx->cp0_perf[CP0_PCNTR1] += 2;
        if (evt == CP0_PEVT_INS) ctx->cp0_perf[CP0_PCNTR1] += 1;
        if (ctx->cp0_perf[CP0_PCNTR1] & W_SIGN) eval_intr (ctx);
        }
    }
#endif

if (!(ctx->events & (EVT_STALL|EVT_STALL_EPOCH))) {     /* not waiting for a stalled ld/sd */

if (ctx->events & (EVT_TSTOP|EVT_INT|EVT_WAIT|EVT_BKPT|EVT_CCHE|EVT_DINT)) { /* event pending? */
    if (ctx->events & EVT_TSTOP) {                      /* stop on trap? */
        ctx->events &= ~EVT_TSTOP;
        for (i = 0; !((ctx->trapbit >> i) & 1); i++) ;
        sprintf (sc1_stop_trap, "%s trap", cpu0_deb[i].name);
        ctx->trapbit = 0;
        return STOP_TRAP;
        }

    if (ctx->events & EVT_CCHE) {                       /* cache error? */
        t_uint64 vec;
#ifdef TRAP_PRINT
        if (tdebug) fprintf(stderr, "    HWERR\n\r");
#endif
        set_cp0_cause (get_cp0_cause() & ~(CP0_CAUSE_COP|CP0_CAUSE_EXC));
        set_cp0_cause (get_cp0_cause() | (CAUSE_CERR << CP0_CAUSE_V_EXC));
        if (ctx->delay) {
            if ((get_cp0_sr() & CP0_SR_EXL) == 0)        /* if ~exl, upd BD */
                set_cp0_cause (get_cp0_cause() | CP0_CAUSE_BD);
            set_cp0_errpc (ctx->PC - 4);
            }
        else {
            if ((get_cp0_sr() & CP0_SR_EXL) == 0)        /* if ~exl, upd BD */
                set_cp0_cause (get_cp0_cause() & ~CP0_CAUSE_BD);
            set_cp0_errpc (ctx->PC);
            }
        if (get_cp0_sr() & CP0_SR_BEV)                   /* BEV set? */
            vec = VEC_CERR_BEV + OFF_CERR;
#if !defined (_MIPS64_R2_)                              /* MIPS R1 */
        else vec = VEC_CERR + OFF_CERR;
#else                                                   /* MIPS R2 */
        else vec = (get_cp0_ebase() | CP0_EBASE_FIXCE) + OFF_CERR;
#endif
        set_pc (vec);
	    set_cp0_sr (get_cp0_sr() | CP0_SR_ERL);
        ctx->delay = 0;
        ctx->taken = 0;
        ctx->events &= ~(EVT_NLFY|EVT_CCHE|EVT_INT|EVT_WAIT);
        ctx->traps = 0;
        }

    if (ctx->events & EVT_INT) {                        /* interrupt? */
        t_uint64 vec;
        set_cp0_cause (get_cp0_cause() & ~(CP0_CAUSE_COP|CP0_CAUSE_EXC|CP0_CAUSE_BD));
        set_cp0_cause (get_cp0_cause() | (CAUSE_INT << CP0_CAUSE_V_EXC));
        if (ctx->delay) {
            set_cp0_cause (get_cp0_cause() | CP0_CAUSE_BD); /* ERL, EXL must be zero */
            set_cp0_epc (ctx->PC - 4);
            }
        else set_cp0_epc (ctx->PC);
        if (get_cp0_sr() & CP0_SR_BEV)                   /* BEV set? */
            vec = VEC_GEN_BEV + ((get_cp0_cause() & CP0_CAUSE_IV)? OFF_INT: OFF_GEN);
#if !defined (_MIPS64_R2_)                              /* MIPS R1 */
        else vec = VEC_GEN + ((get_cp0_cause() & CP0_CAUSE_IV)? OFF_INT: OFF_GEN);
#elif !defined (_MIPS64_R2_VI_)                         /* MIPS R2 no vec int*/
        else vec = (get_cp0_ebase() | CP0_EBASE_FIX) +
            ((get_cp0_cause() & CP0_CAUSE_IV)? OFF_INT: OFF_GEN);
#else                                                   /* MIPS R2 vec int */
        else if (((sa = CP0_INTCTL_GETVS (get_cp0_intctl())) == 0) ||
            ((get_cp0_cause() & CP0_CAUSE_IV) == 0))     /* vectors off? */
            vec = (get_cp0_ebase() | CP0_EBASE_FIX) +
                ((get_cp0_cause() & CP0_CAUSE_IV)? OFF_INT: OFF_GEN);
        else {                                          /* vectored interrupts */
            uint32 req = CP0_SR_GETINT (get_cp0_sr() & get_cp0_cause());
            uint32 offset = vec_pri_enc[req];
            vec = (get_cp0_ebase() | CP0_EBASE_FIX) +
                (offset << (sa + 4));
            }
#endif
        set_pc (vec);
        set_cp0_sr (get_cp0_sr() | CP0_SR_EXL);
        ctx->delay = 0;
        ctx->taken = 0;
        ctx->events &= ~(EVT_NLFY|EVT_INT|EVT_WAIT);
        ctx->traps = 0;
        }

    if ((ctx->events & EVT_BKPT) &&                     /* breakpoint? */
         sim_brk_test (ctx->PC, SWMASK ('E') | (ctx->cpu_num << SIM_BKPT_V_SPC)))
        return STOP_IBKPT;                              /* stop simulation */

    if (ctx->events & EVT_WAIT) {                       /* WAIT instruction? */
        global_sleep++;                                 /* count sheep */
	// This is required by performance model to ensure that time passes
	// during a wait instruction.
	STATS_TICK (ctx, 1);
        return SCPE_OK;
        }

    while (ctx->events & EVT_DINT) {        		    /* DINT asserted? */

	if (get_cp0_debug() & CP0_DEBUG_DM) {
	    if (ctx->events & EVT_DBBP) {
	        set_cp0_debug (get_cp0_debug() & ~CP0_DEBUG_EXC);
	        set_cp0_debug (get_cp0_debug() | (CAUSE_BRK << CP0_DEBUG_V_EXC));
		}
            else
	        break;					/* ignore if already in DM */
	    }

        fprintf(stderr, "DINT evt 0x%x debug 0x%x\n\r", ctx->events, get_cp0_debug());

        set_cp0_depc (ctx->PC);

	if (ctx->events & EVT_DBBP) {
        set_cp0_depc (get_cp0_depc() - 4);
	    }

	if (ctx->delay) {
        set_cp0_depc (get_cp0_depc() - 4);
	    set_cp0_debug (get_cp0_debug() | CP0_DEBUG_DBD);
	    }
	else {
	    set_cp0_debug (get_cp0_debug() & ~CP0_DEBUG_DBD);
	    }

	set_cp0_debug (get_cp0_debug() & ~(CP0_DEBUG_ENTER));
	set_cp0_debug (get_cp0_debug() |  (CP0_DEBUG_DM));

    if (ctx->events & EVT_DBBP) 
	    set_cp0_debug (get_cp0_debug() | CP0_DEBUG_DBP);
	else 
	    set_cp0_debug (get_cp0_debug() | CP0_DEBUG_DINT);

        set_pc (VEC_DEBUG);				/* check DCR for ProbeEnab */
        ctx->delay = 0;
        ctx->taken = 0;
        ctx->traps = 0;
        ctx->events &= ~(EVT_DINT|EVT_DBBP);
        }

    }

ctx->last_PC = ctx->PC;
STATS_IFETCH_START(ctx);
if (ReadI (ctx, ctx->PC, &t64)) {                       /* fetch instr */
    STATS_IFETCH_DONE(ctx, t64);
    ir = (uint32) t64;                                  /* work with 32b */
    op = I_GETOP (ir);                                  /* get opcode */
    rs = I_GETRS (ir);                                  /* get rs */
    rt = I_GETRT (ir);                                  /* get rt */

    if (ctx->events & (EVT_HIST|EVT_NLFY)) {            /* more events? */
 
        if (sim_deb && (ctx->debug & TRAP_SIMTRC)) {    /* tracing? */
            uint32 fmt;
            t_uint64 reg[4];
            fmt = trace_fetch_reg (ctx, ir, reg);
            trace_fprint_one_inst (sim_deb, ir, ctx->PC,
                fmt, reg, ctx->cpu_num + 1);
            }
        if (ctx->hst_lnt) {                             /* history enabled? */
            ctx->hst_p = (ctx->hst_p + 1);              /* next entry */
            if (ctx->hst_p >= ctx->hst_lnt) ctx->hst_p = 0;
            ctx->hist[ctx->hst_p].pc = ctx->PC |HIST_PC;/* save PC */
            ctx->hist[ctx->hst_p].ir = ir;              /* save ir */
            ctx->hist[ctx->hst_p].rs = gpr(rs);      /* save Rs */
            ctx->hist[ctx->hst_p].rt = gpr(rt);      /* save Rt */
            ctx->hist[ctx->hst_p].rd = gpr(I_GETRD (ir)); /* save Rd */
            }

        if (ctx->events & EVT_NLFY) {                   /* nullify cycle? */
            ctx->events &= ~EVT_NLFY;
            ctx->delay = 0;                             /* done with branch */
	    set_pc (ctx->PC + 4);                      /* advance PC */
            return SCPE_OK;
            }
        }

    set_pc (ctx->PC + 4);                              /* advance PC */
    if (ir) num_nops = 0;

    switch (op) {

/* Memory reference instructions */

    case OP_LB:                                         /* LB */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadB (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, SEXT_B_D (t64));
        break;

    case OP_LBU:                                        /* LBU */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadB (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, t64);
        break;

    case OP_LH:                                         /* LH */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadH (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, SEXT_H_D (t64));
        break;

    case OP_LHU:                                        /* LHU */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadH (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, t64);
        break;

    case OP_LW:                                         /* LW */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, SEXT_W_D (t64));
        break;

    case OP_LWU:                                        /* LWU */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, t64);
        break;
    case OP_LL:                                         /* LL */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea, &t64)) {
	    t_uint64 pa; 
	    uint32   catr;
            if (rt != 0) setgpr(rt, SEXT_W_D (t64));
            (void) xlate_va (ctx, ea, VA_DR, &pa, &catr);
            lock_set (ctx->cpu_num, pa, catr);
            }
        break;

    case OP_LWL:                                        /* LWL */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea & ~3, &t64) && (rt != 0)) {
            uint32 sc = 24 - ((((uint32) ea) & 3) * 8);
            mask = M32 << sc;
            setgpr(rt, (gpr(rt) & ~mask) |
                ((t64 << sc) & mask));
            setgpr(rt, SEXT_W_D (gpr(rt)));
            }
        break;

    case OP_LWR:                                        /* LWR */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea & ~3, &t64) && (rt != 0)) {
            uint32 sc = (((uint32) ea) & 3) * 8;
            mask = M32 >> sc;
            setgpr(rt, (gpr(rt) & ~mask) |
                ((t64 >> sc) & mask));
            setgpr(rt, SEXT_W_D (gpr(rt)));
            }
        break;

    case OP_LWC1:                                       /* LWC1 */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea, &t64))
            fp_putspr (ctx, rt, t64);
        break;

    case OP_LWC2:                                       /* LWC2 */
        if (!Q_CP2) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadW (ctx, ea, &t64))
            cp2_putspr (ctx, rt, t64);
        break;

    case OP_PREF:                                       /* prefetch */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (xlate_va (ctx, ea, VA_CR, &pa, &catr))      /* in TLB? */
            mem_pref (ctx, pa, catr, rt);               /* let mem handle */
        break;

    case OP_LD:                                         /* LD */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea, &t64) && (rt != 0))
            setgpr(rt, t64);
        break;

    case OP_LLD:                                        /* LLD */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea, &t64)) {
	    t_uint64 pa; 
	    uint32   catr;
            if (rt != 0) setgpr(rt, t64);
            (void) xlate_va (ctx, ea, VA_DR, &pa, &catr);
            lock_set (ctx->cpu_num, pa, catr);
            }
        break;

    case OP_LDL:                                        /* LDL */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea & ~7, &t64) && (rt != 0)) {
            uint32 sc = 56 - ((((uint32) ea) & 7) * 8);
            mask = M64 << sc;
            setgpr(rt, (gpr(rt) & ~mask) |
                ((t64 << sc) & mask));
            }
        break;

    case OP_LDR:                                        /* LDR */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea & ~7, &t64) && (rt != 0)) {
            uint32 sc = (((uint32) ea) & 7) * 8;
            mask = M64 >> sc;
            setgpr(rt, (gpr(rt) & ~mask) |
                ((t64 >> sc) & mask));
            }
        break;

    case OP_LDC1:                                       /* LDC1 */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea, &t64))
            fp_putdpr (ctx, rt, t64);
        break;

    case OP_LDC2:                                       /* LDC2 */
        if (!Q_CP2) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ReadD (ctx, ea, &t64))
            cp2_putdpr (ctx, rt, t64);
        break;

    case OP_SB:                                         /* STB */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteB (ctx, ea, gpr(rt));
        break;

    case OP_SH:                                         /* SH */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteH (ctx, ea, gpr(rt));
        break;

    case OP_SW:                                         /* SW */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteW (ctx, ea, gpr(rt));
        break;

    case OP_SWL:                                        /* SWL */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (WriteTest (ctx, ea)) {                      /* test addr */
            uint32 sc = 24 - ((((uint32) ea) & 3) * 8);
            if (ReadW (ctx, ea & ~3, &t64)) {
		mask = M32 >> sc;
		t64 = (t64 & ~mask) |
		    ((gpr(rt) >> sc) & mask);
		WriteW (ctx, ea & ~3, t64 & M32);
	        }
       	    }
        break;

    case OP_SWR:                                        /* SWR */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (WriteTest (ctx, ea)) {                      /* test addr */
            uint32 sc = (((uint32) ea) & 3) * 8;
            if (ReadW (ctx, ea & ~3, &t64)) {
		mask = M32 << sc;
		t64 = (t64 & ~mask) |
		    ((gpr(rt) << sc) & mask);
		WriteW (ctx, ea & ~3, t64 & M32);
	        }
            }
        break;

    case OP_SC:                                         /* SC */
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ea & 3) tlb_set_aer (ctx, ea, VA_DW);
        else if (LOCK_TEST (ctx->cpu_num)) {
            if (WriteW (ctx, ea, gpr(rt))) {
		if (rt != 0) setgpr(rt, 1);
	    }
	} else {
	    setgpr(rt, 0);
	}
	lock_clear (ctx->cpu_num);
        break;

    case OP_SWC1:                                       /* SWC1 */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteW (ctx, ea, fp_getspr (ctx, rt));
        break;

    case OP_SWC2:                                       /* SWC2 */
        if (!Q_CP2) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteW (ctx, ea, cp2_getspr (ctx, rt));
        break;

    case OP_SD:                                         /* SD */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteD (ctx, ea, gpr(rt));
        break;

    case OP_SDL:                                        /* SDL */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (WriteTest (ctx, ea)) {                      /* test addr */
            uint32 sc = 56 - ((((uint32) ea) & 7) * 8);
            if (ReadD (ctx, ea & ~7, &t64)) {
		mask = M64 >> sc;
		t64 = (t64 & ~mask) |
		    ((gpr(rt) >> sc) & mask);
		WriteD (ctx, ea & ~7, t64);
	        }
            }
        break;

    case OP_SDR:                                        /* SDR */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (WriteTest (ctx, ea)) {                      /* test addr */
            uint32 sc = (((uint32) ea) & 7) * 8;
            if (ReadD (ctx, ea & ~7, &t64)) {
		mask = M64 << sc;
		t64 = (t64 & ~mask) |
		    ((gpr(rt) << sc) & mask);
		WriteD (ctx, ea & ~7, t64);
  	        }
            }
        break;

    case OP_SCD:                                        /* SCD */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
	}
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        if (ea & 7) tlb_set_aer (ctx, ea, VA_DW);
        else if (LOCK_TEST (ctx->cpu_num)) {
            if (WriteD (ctx, ea, gpr(rt))) {
		if (rt != 0) setgpr(rt, 1);
            }
	} else {
	    setgpr(rt, 0);
	}
	lock_clear (ctx->cpu_num);
        break;

    case OP_SDC1:                                       /* SDC1 */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteD (ctx, ea, fp_getdpr (ctx, rt));
        break;

    case OP_SDC2:                                       /* SDC2 */
        if (!Q_CP2) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        WriteD (ctx, ea, cp2_getdpr (ctx, rt));
        break;

    case OP_CACHE:                                      /* CACHE */
        if (!Q_CP0 && !Q_MD_K) {                        /* disabled, not kern? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        dsp = I_GETDISP (ir);
        ea = gpr(rs) + SEXT_DISP (dsp);
        mem_cache (ctx, ir, ea, rt);                    /* let mem handle */
        break;

/* Immediate instructions */

    case OP_LUI:                                        /* LUI */
        if (rt != 0) {
            dsp = I_GETDISP (ir) << 16;
            setgpr(rt, SEXT_W_D (dsp));
            }
        break;

    case OP_ANDI:                                       /* ANDI */
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            setgpr(rt, gpr(rs) & dsp);
            }
        break;

    case OP_ORI:                                        /* ORI */
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            setgpr(rt, gpr(rs) | dsp);
            }
        break;

    case OP_XORI:                                       /* XORI */
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            setgpr(rt, gpr(rs) ^ dsp);
            }
        break;

    case OP_ADDI:                                       /* ADDI */
        dsp = I_GETDISP (ir);
        t64 = SEXT_DISP (dsp);
        res = gpr(rs) + t64;
        if (ADD_OV_W (res, gpr(rs), t64))
            ctx->traps |= TRAP_IOV;
        else if (rt != 0) setgpr(rt, SEXT_W_D (res));
        break;

    case OP_ADDIU:                                      /* ADDUI */
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            res = gpr(rs) + t64;
            setgpr(rt, SEXT_W_D (res));
            }
        break;

    case OP_DADDI:                                      /* DADDI */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        dsp = I_GETDISP (ir);
        t64 = SEXT_DISP (dsp);
        res = gpr(rs) + t64;
        if (ADD_OV_D (res, gpr(rs), t64))
            ctx->traps |= TRAP_IOV;
        else if (rt != 0) setgpr(rt, res);
        break;

    case OP_DADDIU:                                     /* DADDIU */
        if (!Q_64BOP) {
            ctx->traps |= TRAP_RSVI;
            break;
            }
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            setgpr(rt, gpr(rs) + t64);
            }
        break;

    case OP_SLTI:                                       /* SLTI */
        if (rt != 0) {
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (((t_int64) gpr(rs)) < ((t_int64) t64))
                setgpr(rt, 1);
            else setgpr(rt, 0);
            }
        break;

    case OP_SLTIU:                                      /* SLTIU */
        dsp = I_GETDISP (ir);
        if (rt != 0) {
            t64 = SEXT_DISP (dsp);
            if (gpr(rs) < t64) setgpr(rt, 1);
            else setgpr(rt, 0);
            }
        else if (rs == 0) {                             /* MipsSim hack */
            if (ir == ICE9_E_MipsMagicInstrs_HALTFAIL) return STOP_FAIL;
            else if (ir == ICE9_E_MipsMagicInstrs_HALTPASS) return STOP_PASS;
            else if (ir == ICE9_E_MipsMagicInstrs_DUMPCTB) {
#ifdef SIMH_CPUSIMH	
		        extern void ScxDumpCTB();
		        ScxDumpCTB();
#endif
		        }
            else if (ir == ICE9_E_MipsMagicInstrs_SIMHTRACEON) {
                ctx->debug |= TRAP_SIMTRC;
                ctx->events |= EVT_HIST;
                }
            else if (ir == ICE9_E_MipsMagicInstrs_SIMHTRACEOFF)
                ctx->debug &= ~TRAP_SIMTRC;
#ifdef ENABLE_SC1_PERFMODEL
            else if (ir == ICE9_E_MipsMagicInstrs_CSIMON) {
		// Call into the perfmodel.  
		// Treat registers $4,$5,$6 as inputs and write to $2.
		ctx->R[2] = perfmodel_magic_instr (ctx->cpu_num, ctx->R[4], ctx->R[5], ctx->R[6]);
            } else if (ir == ICE9_E_MipsMagicInstrs_CSIMOFF) {
	        // unused
            } else if (ir == ICE9_E_MipsMagicInstrs_CSIMREPORT) {
		// unused
	    }
#endif
	    else if (((dsp & ~0xff) == SLTIU_HOOKAB) ||
		        ((dsp & ~0xff) == SLTIU_HOOKAD)) {      // Other AD opcodes
#ifdef SIMX_NATIVE  /* Built into simx */
		        ScxGlobal::cpuInstrMagic(ctx->cpu_num, 0, ir,
					 gpr(rs), gpr(rt));
#else
		    // Not an error, there are other MIPS codes - see ScxGlobal.sp
#endif
	            }
            }
        break;

/* Control instructions */

    case OP_J:                                          /* J */
       if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay_PC = (ctx->PC & J_REGION) |
            (I_GETJT (ir) << 2);
        ctx->delay = 2;
        ctx->taken = 1;
        break;

    case OP_JAL:                                        /* JAL */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        setgpr(31, ctx->PC + 4);
        ctx->delay_PC = (ctx->PC & J_REGION) |
            (I_GETJT (ir) << 2);
        ctx->delay = 2;
        ctx->taken = 1;
        break;

    case OP_BEQ:                                        /* BEQ */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = (gpr(rs) == gpr(rt));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        break;

    case OP_BEQL:                                       /* BEQL */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = (gpr(rs) == gpr(rt));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        else ctx->events |= EVT_NLFY;
        break;

    case OP_BNE:                                        /* BNE */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = (gpr(rs) != gpr(rt));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        break;

    case OP_BNEL:                                       /* BNEL */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = (gpr(rs) != gpr(rt));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        else ctx->events |= EVT_NLFY;
        break;

    case OP_BGTZ:                                       /* BGTZ */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = ((gpr(rs) != 0) && !(gpr(rs) & D_SIGN));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        break;

    case OP_BGTZL:                                      /* BGTZL */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = ((gpr(rs) != 0) && !(gpr(rs) & D_SIGN));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        else ctx->events |= EVT_NLFY;
        break;

    case OP_BLEZ:                                       /* BLEZ */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = ((gpr(rs) == 0) || (gpr(rs) & D_SIGN));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        break;

    case OP_BLEZL:                                      /* BLEZL */
        if (ctx->delay) {                               /* 5KF traps */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        ctx->delay = 2;
        ctx->taken = ((gpr(rs) == 0) || (gpr(rs) & D_SIGN));
        if (ctx->taken) {
            dsp = I_GETDISP (ir);
            ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
            }
        else ctx->events |= EVT_NLFY;
        break;

/* Special operate instructions (00) */

    case OP_SPECIAL:                                    /* special operate */
        fnc = I_GETFNC (ir);                            /* get function */
        rd = I_GETRD (ir);                              /* get rd */
        sa = I_GETSA (ir);                              /* shift amount */
        switch (fnc) {                                  /* case on function */

        case SP_SLL:                                    /* SLL */
            res = gpr(rt) << sa;
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
	    if (!ir) {					                    /* detect runaway */
		    num_nops++;
		    if (!msg_nops && num_nops > MAX_NOPS) {
		        msg_nops = TRUE;
		        fprintf(stderr,"%%Error: SIMH: max nops exceeded\n"); 
		        }
	        }
            break;
        case SP_SLLV:                                   /* SLLV */
            res = gpr(rt) << (gpr(rs) & 0x1F);
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;

        case SP_SRL:                                    /* SRL/ROTRL */
            if (rd != 0) {
#if defined (_MIPS64_R2_)
                if (ir & (1u << 21))                    /* ir<21>? */
                    res = (((gpr(rt) & M32) >> sa) | (gpr(rt) << (32 - sa))) & M32;
                else
#endif
                res = (gpr(rt) & M32) >> sa;
                setgpr(rd, SEXT_W_D (res));
                }
            break;
        case SP_SRLV:                                   /* SRLV/ROTRV */
            if (rd != 0) {
                uint32 sc = ((uint32) gpr(rs)) & 0x1F;
#if defined (_MIPS64_R2_)
                if (ir & (1u << 6))                     /* ir<6>? */
                    res = (((gpr(rt) & M32) >> sc) | (gpr(rt) << (32 - sc))) & M32;
                else
#endif
                res = (gpr(rt) & M32) >> sc;
                setgpr(rd, SEXT_W_D (res));
                }
            break;

        case SP_SRA:                                    /* SRA */
            res = SEXT_W_D (gpr(rt)) >> sa;
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;
        case SP_SRAV:                                   /* SRAV */
            res = SEXT_W_D (gpr(rt)) >> (gpr(rs) & 0x1F);
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;

        case SP_DSLL:                                   /* DSLL */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, gpr(rt) << sa);
            break;
        case SP_DSLL32:                                 /* DSLL32 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, gpr(rt) << (sa + 32));
            break;
        case SP_DSLLV:                                  /* DSLLV */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, gpr(rt) << (gpr(rs) & 0x3F));
            break;

        case SP_DSRL:                                   /* DSRL/DROTR */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
#if defined (_MIPS64_R2_)
                if (ir & (1u << 21))                    /* ir<21>? */
                    setgpr (rd, ((gpr(rt) >> sa) | (gpr(rt) << (64 - sa))) & M64);
                else
#endif
                setgpr(rd, gpr(rt) >> sa);
                }
            break;
        case SP_DSRL32:                                 /* DSRL32/DROTR32 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
#if defined (_MIPS64_R2_)
                if (ir & (1u << 21))                    /* ir<21>? */
                    setgpr (rd, ((gpr(rt) >> (sa + 32)) | (gpr(rt) << (32 - sa))) & M64);
                else
#endif
                setgpr(rd, gpr(rt) >> (sa + 32));
                }
            break;
        case SP_DSRLV:                                  /* DSRLV/DROTRV */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
                uint32 sc = ((uint32) gpr(rs)) & 0x3F;
#if defined (_MIPS64_R2_)
                if (ir & (1u << 6))                     /* ir<6>? */
                    setgpr (rd, ((gpr(rt) >> sc) | (gpr(rt) << (64 - sc))) & M64);
                else
#endif
                setgpr(rd, gpr(rt) >> sc);
                }
            break;

        case SP_DSRA:                                   /* DSRA */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, ((t_int64) gpr(rt)) >> sa);
            break;
        case SP_DSRA32:                                 /* DSRA32 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, ((t_int64) gpr(rt)) >> (sa + 32));
            break;
        case SP_DSRAV:                                  /* DSRAV */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, ((t_int64) gpr(rt)) >> (gpr(rs) & 0x3F));
            break;

        case SP_MOVCI:                                  /* MOVC1 */
            if (!Q_CP1) {                               /* disabled? */
                ctx->traps |= TRAP_CPUN;
                break;
                }
            if ((rd != 0) && fp_testcc (ctx, rt))
                setgpr(rd, gpr(rs));
            break;

        case SP_MOVN:                                   /* MOVN */
            if ((rd != 0) && (gpr(rt) != 0))
                setgpr(rd, gpr(rs));
            break;
        case SP_MOVZ:                                   /* MOVZ */
            if ((rd != 0) && (gpr(rt) == 0))
                setgpr(rd, gpr(rs));
            break;

        case SP_ADD:                                    /* ADD */
            res = gpr(rs) + gpr(rt);
            if (ADD_OV_W (res, gpr(rs), gpr(rt)))
                ctx->traps |= TRAP_IOV;
            else if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;
        case SP_ADDU:                                   /* ADDU */
            res = gpr(rs) + gpr(rt);
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;

        case SP_SUB:                                    /* SUB */
            res = gpr(rs) - gpr(rt);
            if (SUB_OV_W (res, gpr(rs), gpr(rt)))
                ctx->traps |= TRAP_IOV;
            else if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;
        case SP_SUBU:                                   /* SUBU */
            res = gpr(rs) - gpr(rt);
            if (rd != 0) setgpr(rd, SEXT_W_D (res));
            break;

        case SP_DADD:                                   /* DADD */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            res = gpr(rs) + gpr(rt);
            if (ADD_OV_D (res, gpr(rs), gpr(rt)))
                ctx->traps |= TRAP_IOV;
            else if (rd != 0) setgpr(rd, res);
            break;
        case SP_DADDU:                                  /* DADDU */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, gpr(rs) + gpr(rt));
            break;

        case SP_DSUB:                                   /* DSUB */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            res = gpr(rs) - gpr(rt);
            if (SUB_OV_D (res, gpr(rs), gpr(rt)))
                ctx->traps |= TRAP_IOV;
            else if (rd != 0) setgpr(rd, res);
            break;
        case SP_DSUBU:                                  /* DSUBU */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) setgpr(rd, gpr(rs) - gpr(rt));
            break;

        case SP_AND:                                    /* AND */
	        if (sa != 0) {                              /* magic instr? */
#ifdef SIMX_NATIVE                                      /* Built into simx */
			                                            /* Let SIMX deal with all magic */
			    uint64_t res = ScxGlobal::cpuInstrMagic(ctx->cpu_num, 0, ir,
								gpr(rs), gpr(rt));
			    if (rd != 0) setgpr(rd, res);
#else /* Simx standalone has some magic itself */
//		        if ((sa == I_GETSA (ICE9_E_MipsMagicInstrs_PUTCHAR)) && (rd == 0)) { /* putchar */
//		            if (gpr(rs) == '\n' && sim_isatty()) {
//			            sim_os_putchar((int32) '\r', ctx->cpu_num);
//			            }
//		            sim_putchar_s ((int32) gpr(rs), ctx->cpu_num); /* putchar a0 */
//	                }
# if !defined (_WIN32)
		        else if ((sa == I_GETSA (ICE9_E_MipsMagicInstrs_MPIGET)) && (rt == 0)) {
		            t_int64 res;
		            res = sim_magic_pipe_instruction(gpr(rs));
		            if (rd != 0) setgpr(rd, res);
		            }
# endif
                /* else */ if ((sa == I_GETSA (ICE9_E_MipsMagicInstrs_ECCINJ)) && (rd == 0)) {
                    extern void cac_ecc_event (t_uint64 val);
                    cac_ecc_event (gpr(rs));
                    }
		        else {
		            /* unknown magic instruction, ignore sa */
		            if (rd != 0) {
			            setgpr(rd, gpr(rs) & gpr(rt));
		                }
		            }
#endif
	            }
            else if (rd != 0) {
                setgpr(rd, gpr(rs) & gpr(rt));
	            }
            break;
        case SP_OR:                                     /* OR */
            if (rd != 0) setgpr(rd, gpr(rs) | gpr(rt));
            break;
        case SP_XOR:                                    /* XOR */
            if (rd != 0) setgpr(rd, gpr(rs) ^ gpr(rt));
            break;
        case SP_NOR:                                    /* NOR */
            if (rd != 0) setgpr(rd, ~(gpr(rs) | gpr(rt)));
            break;

        case SP_SLT:                                    /* SLT */
            if (rd != 0) setgpr(rd, (((t_int64) gpr(rs)) < ((t_int64) gpr(rt))));
            break;
        case SP_SLTU:                                   /* SLTU */
            if (rd != 0) setgpr(rd, gpr(rs) < gpr(rt));
            break;

        case SP_JR:                                     /* JR */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay_PC = gpr(rs);
            ctx->delay = 2;
            ctx->taken = 1;
            break;
        case SP_JALR:                                   /* JALR */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            t64 = gpr(rs);
            if (rd != 0) setgpr(rd, ctx->PC + 4);
            ctx->delay_PC = t64;
            ctx->delay = 2;
            ctx->taken = 1;
            break;

        case SP_MFHI:                                   /* MFHI */
            if (rd != 0) setgpr(rd, ctx->mhi);
            break;
        case SP_MFLO:                                   /* MFLO */
            if (rd != 0) setgpr(rd, ctx->mlo);
            break;
        case SP_MTHI:                                   /* MTHI */
            set_mhi (gpr(rs));
            break;
        case SP_MTLO:                                   /* MTLO */
            set_mlo (gpr(rs));
            break;

        case SP_MULT:                                   /* MULT */
            s1 = SEXT_W_D (gpr(rs));
            s2 = SEXT_W_D (gpr(rt));
            sres = s1 * s2;
            set_mlo (SEXT_W_D (sres));
            set_mhi (SEXT_W_D (sres >> 32));
            break;
        case SP_MULTU:                                  /* MULTU */
            us1 = gpr(rs) & M32;
            us2 = gpr(rt) & M32;
            res = us1 * us2;
            set_mlo (SEXT_W_D (res));
            set_mhi (SEXT_W_D (res >> 32));
            break;

        case SP_DIV:                                    /* DIV */
            if (gpr(rt) & M32) {
                s1 = SEXT_W_D (gpr(rs));
                s2 = SEXT_W_D (gpr(rt));
                sres = s1 / s2;
                set_mlo (SEXT_W_D (sres));
                sres = s1 % s2;
                set_mhi (SEXT_W_D (sres));
                }
            break;
        case SP_DIVU:                                   /* DIVU */
            if (gpr(rt) & M32) {
                us1 = gpr(rs) & M32;
                us2 = gpr(rt) & M32;
                res = us1 / us2;
                set_mlo (SEXT_W_D (res));
                res = us1 % us2;
                set_mhi (SEXT_W_D (res));
                }
            break;

        case SP_DMULT:                                  /* DMULT */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            us1 = ABS_D (gpr(rs));
            us2 = ABS_D (gpr(rt));
            set_mlo (uemul64 (us1, us2, &ctx->mhi));
            if ((gpr(rs) ^ gpr(rt)) & D_SIGN) {
                set_mlo (NEG_D (ctx->mlo));
                set_mhi (~ctx->mhi + (ctx->mlo == 0));
                }
            break;
        case SP_DMULTU:                                 /* DMULTU */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            set_mlo (uemul64 (gpr(rs), gpr(rt), &ctx->mhi));
            break;

        case SP_DDIV:                                   /* DDIV */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (gpr(rt)) {
                if ((gpr(rs) == SIM_ULL(0x8000000000000000)) &&
                    (gpr(rt) == SIM_ULL(0xFFFFFFFFFFFFFFFF))) {
                    set_mlo (gpr(rs));
                    set_mhi (0);
                    }
                else {
                    s1 = gpr(rs);
                    s2 = gpr(rt);
                    set_mlo (s1 / s2);
                    set_mhi (s1 % s2);
                    }
                }
            break;
        case SP_DDIVU:                                  /* DDIVU */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (gpr(rt)) {
                set_mlo (gpr(rs) / gpr(rt));
                set_mhi (gpr(rs) % gpr(rt));
                }
            break;

        case SP_TEQ:                                    /* TEQ */
            if (gpr(rs) == gpr(rt))
                ctx->traps |= TRAP_TRAP;
            break;
        case SP_TNE:                                    /* TNE */
            if (gpr(rs) != gpr(rt))
                ctx->traps |= TRAP_TRAP;
            break;
        case SP_TGE:                                    /* TGE */
            if (((t_int64) gpr(rs)) >= ((t_int64) gpr(rt)))
                ctx->traps |= TRAP_TRAP;
            break;
        case SP_TGEU:                                   /* TGEU */
            if (gpr(rs) >= gpr(rt))
                ctx->traps |= TRAP_TRAP;
            break;
        case SP_TLT:                                    /* TLT */
            if (((t_int64) gpr(rs)) < ((t_int64) gpr(rt)))
                ctx->traps |= TRAP_TRAP;
            break;
        case SP_TLTU:                                   /* TLTU */
            if (gpr(rs) < gpr(rt))
                ctx->traps |= TRAP_TRAP;
            break;

        case SP_SYSCALL:                                /* SYSCALL */
            ctx->traps |= TRAP_SYSCALL;
            break;
        case SP_BREAK:                                  /* BREAK */
            ctx->traps |= TRAP_BREAK;
            break;

        case SP_SYNC:                                   /* SYNC */
            mem_sync (ctx);                             /* let mem handle */
            break;

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }                                           /* end case special */
        break;

/* Register-immediate instructions (01) */

    case OP_REGIMM:                                     /* register immediate */
        switch (rt) {                                   /* case on rt */

        case RI_BLTZ:                                   /* BLTZ */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) != 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            break;
        case RI_BLTZL:                                  /* BLTZL */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) != 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            else ctx->events |= EVT_NLFY;
            break;

        case RI_BGEZ:                                   /* BGEZ */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) == 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            break;
        case RI_BGEZL:                                  /* BGEZL */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) == 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            else ctx->events |= EVT_NLFY;
            break;

        case RI_BLTZAL:                                 /* BLTZAL */
#if defined (_MIPS_SCTX_)
            if (rs == 31) {                             /* undefined? */
                ctx->traps |= TRAP_RSVI;
                break;
                }
#endif
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            setgpr(31, ctx->PC + 4);
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) != 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            break;
        case RI_BLTZALL:                                /* BLTZALL */
#if defined (_MIPS_SCTX_)
            if (rs == 31) {                             /* undefined? */
                ctx->traps |= TRAP_RSVI;
                break;
                }
#endif
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            setgpr(31, ctx->PC + 4);
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) != 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            else ctx->events |= EVT_NLFY;
            break;

        case RI_BGEZAL:                                 /* BGEZAL */
#if defined (_MIPS_SCTX_)
            if (rs == 31) {                             /* undefined? */
                ctx->traps |= TRAP_RSVI;
                break;
                }
#endif
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            setgpr(31, ctx->PC + 4);
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) == 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            break;
        case RI_BGEZALL:                                /* BGEZALL */
 #if defined (_MIPS_SCTX_)
            if (rs == 31) {                             /* undefined? */
                ctx->traps |= TRAP_RSVI;
                break;
                }
#endif
           if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            setgpr(31, ctx->PC + 4);
            ctx->delay = 2;
            ctx->taken = ((gpr(rs) & D_SIGN) == 0);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            else ctx->events |= EVT_NLFY;
            break;

        case RI_TGEI:                                   /* TGEI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (((t_int64) gpr(rs)) >= ((t_int64) t64))
                ctx->traps |= TRAP_TRAP;
            break;

        case RI_TGEIU:                                  /* TGEUI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (gpr(rs) >= t64)
                ctx->traps |= TRAP_TRAP;
            break;

        case RI_TLTI:                                   /* TLTI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (((t_int64) gpr(rs)) < ((t_int64) t64))
                ctx->traps |= TRAP_TRAP;
            break;

        case RI_TLTIU:                                  /* TLTUI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (gpr(rs) < t64)
                ctx->traps |= TRAP_TRAP;
            break;

        case RI_TEQI:                                   /* TEQI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (gpr(rs) == t64)
                ctx->traps |= TRAP_TRAP;
            break;

        case RI_TNEI:                                   /* TNEI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
            if (gpr(rs) != t64)
                ctx->traps |= TRAP_TRAP;
            break;

#if defined (_MIPS64_R2_)
        case RI_SYNCI:                                  /* SYNCI */
            dsp = I_GETDISP (ir);
            t64 = SEXT_DISP (dsp);
//          inv_icache (gpr(rs) + dsp);
            break;
#endif

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }                                           /* end reg imm */
        break;

/* Special 2 (opcode 1C) */

    case OP_SPECIAL2:                                   /* special 2 */
        fnc = I_GETFNC (ir);                            /* get function */
        rd = I_GETRD (ir);                              /* get rd */
        switch (fnc) {                                  /* case on function */

        case SP2_MADD:                                  /* MADD */
            s1 = SEXT_W_D (gpr(rs));
            s2 = SEXT_W_D (gpr(rt));
            t64 = ((ctx->mhi & M32) << 32) | (ctx->mlo & M32);
            sres = t64 + (s1 * s2);
            set_mlo (SEXT_W_D (sres));
            set_mhi (SEXT_W_D (sres >> 32));
            break;
        case SP2_MADDU:                                 /* MADDU */
            us1 = gpr(rs) & M32;
            us2 = gpr(rt) & M32;
            t64 = ((ctx->mhi & M32) << 32) | (ctx->mlo & M32);
            res = t64 + (us1 * us2);
            set_mlo (SEXT_W_D (res));
            set_mhi (SEXT_W_D (res >> 32));
            break;

        case SP2_MSUB:                                  /* MSUB */
            s1 = SEXT_W_D (gpr(rs));
            s2 = SEXT_W_D (gpr(rt));
            t64 = ((ctx->mhi & M32) << 32) | (ctx->mlo & M32);
            sres = t64 - (s1 * s2);
            set_mlo (SEXT_W_D (sres));
            set_mhi (SEXT_W_D (sres >> 32));
            break;
        case SP2_MSUBU:                                 /* MSUBU */
            us1 = gpr(rs) & M32;
            us2 = gpr(rt) & M32;
            t64 = ((ctx->mhi & M32) << 32) | (ctx->mlo & M32);
            res = t64 - (us1 * us2);
            set_mlo (SEXT_W_D (res));
            set_mhi (SEXT_W_D (res >> 32));
            break;

        case SP2_MUL:                                   /* MUL */
            s1 = SEXT_W_D (gpr(rs));
            s2 = SEXT_W_D (gpr(rt));
            sres = s1 * s2;
            if (rd != 0) {
                setgpr(rd, SEXT_W_D (sres));
                }
#if defined (_MIPS_SCTX_)
            set_mlo (SEXT_W_D (sres));
            set_mhi (SEXT_W_D (sres >> 32));
#endif
            break;

        case SP2_CLZ:                                   /* CLZ */
            if (rd != 0) {
		        t_uint64 temp = 32;
                for (i = 31; i >= 0; i--) {
                    if ((gpr(rs) >> i) & 1) {
                        temp = 31 - i;
                        break;
                        }
                    }
                setgpr(rd, temp);
                }
            break;
        case SP2_CLO:                                   /* CLO */
            if (rd != 0) {
		        t_uint64 temp = 32;
                for (i = 31; i >= 0; i--) {
                    if (((gpr(rs) >> i) & 1) == 0) {
                        temp = 31 - i;
                        break;
                        }
                    }
                setgpr(rd, temp);
                }
            break;
        case SP2_DCLZ:                                  /* DCLZ */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
		        t_uint64 temp = 64;
                for (i = 63; i >= 0; i--) {
                    if ((gpr(rs) >> i) & 1) {
                        temp = 63 - i;
                        break;
                        }
                    }
                setgpr(rd, temp);
                }
            break;
        case SP2_DCLO:                                  /* DCLO */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
		        t_uint64 temp = 64;
                for (i = 63; i >= 0; i--) {
                    if (((gpr(rs) >> i) & 1) == 0) {
                        temp = 63 - i;
                        break;
                        }
                    }
                setgpr(rd, temp);
                }
            break;

#if defined (_MIPS_SCTX_)
        case SP2_POP:
            if (rd != 0) {
                for (i = 0, res = 0; i < 32; i++) {
                    if ((gpr(rs) >> i) & 1) res++;
                    }
                setgpr(rd, res);
                }
            break;

        case SP2_DPOP:
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rd != 0) {
                for (i = 0, res = 0; i < 64; i++) {
                    if ((gpr(rs) >> i) & 1) res++;
                    }
                setgpr(rd, res);
                }
            break;

#endif

        case SP2_SDBBP:                                 /* SDBBP */
#if defined (_MIPS_SCTX_)
            if ((ctx->cp0_cnf & CP0_CNF_SDTR) && !Q_MD_K) {
                ctx->traps |= TRAP_RSVI;
                break;
            }
            else
#endif
	        {
            int cc;
	        ctx->events |= EVT_DBBP;
	        for (cc = 0; cc < NUM_CORES; cc++) 
                cpu_ctx[cc]->events |= EVT_DINT;
	        }
            break;

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }                                           /* end case special 2 */
        break;

/* Special 3 (opcode 1F) - Mips64 R2 only */

#if defined (_MIPS64_R2_)

    case OP_SPECIAL3:                                   /* special 3 */
        fnc = I_GETFNC (ir);                            /* get function */
        rd = I_GETRD (ir);                              /* msb */
        sa = I_GETSA (ir);                              /* position */
        switch (fnc) {                                  /* case on function */

        case SP3_EXT:                                   /* EXT */
            res = cpu_ext (gpr(rs) & M32, rd, sa);
            if (rt != 0) setgpr (rt, SEXT_W_D (res));
            break;
        case SP3_DEXT:                                  /* DEXT */
            res = cpu_ext (gpr(rs), rd, sa);
            if (rt != 0) setgpr (rt, res);
            break;
        case SP3_DEXTM:                                 /* DEXTM */
            res = cpu_ext (gpr(rs), rd + 32, sa);
            if (rt != 0) setgpr (rt, res);
            break;
        case SP3_DEXTU:                                 /* DEXTU */
            res = cpu_ext (gpr(rs), rd, sa + 32);
            if (rt != 0) setgpr (rt, res);
            break;

        case SP3_INS:                                   /* INS */
            res = cpu_ins (gpr(rt), gpr(rs), rd, sa);
            if (rt != 0) setgpr (rt, SEXT_W_D (res));
            break;
        case SP3_DINS:                                  /* DINS */
            res = cpu_ins (gpr(rt), gpr(rs), rd, sa);
            if (rt != 0) setgpr (rt, res);
            break;
        case SP3_DINSM:                                 /* DINSM */
            res = cpu_ins (gpr(rt), gpr(rs), rd + 32, sa);
            if (rt != 0) setgpr (rt, res);
            break;
        case SP3_DINSU:                                 /* DINSU */
            res = cpu_ins (gpr(rt), gpr(rs), rd + 32, sa + 32);
            if (rt != 0) setgpr (rt, res);
            break;

        case SP3_BSH:                                   /* BSH etc */
            switch (sa) {                               /* case on sa */

            case SP3BSH_SBH:                            /* WSBH */
                res = ((gpr(rt) << 8) & 0xFF00FF00) |
                      ((gpr(rt) >> 8) & 0x00FF00FF);
                if (rd != 0) setgpr (rd, SEXT_W_D (res));
                break;

            case SP3BSH_SEB:                            /* SEB */
                if (rd != 0) setgpr (rd, SEXT_B_D (gpr(rt)));
                break;

            case SP3BSH_SEH:                            /* SEH */
                if (rd != 0) setgpr (rd, SEXT_H_D (gpr(rt)));
                break;
                }

        case SP3_DBSH:                                  /* DBSH etc */
            switch (sa) {                               /* case on sa */

            case SP3BSH_SBH:                            /* DSBH */
                res = ((gpr(rt) << 8) & SIM_ULL(0xFF00FF00FF00FF00)) |
                      ((gpr(rt) >> 8) & SIM_ULL(0x00FF00FF00FF00FF));
                if (rd != 0) setgpr (rd, res);
                break;

            case SP3BSH_SHD:                            /* DSHD */
                res = ((gpr(rt) << 48) & SIM_ULL(0xFFFF000000000000)) |
                      ((gpr(rt) << 16) & SIM_ULL(0x0000FFFF00000000)) |
                      ((gpr(rt) >> 16) & SIM_ULL(0x00000000FFFF0000)) |
                      ((gpr(rt) >> 48) & SIM_ULL(0x000000000000FFFF));
                if (rd != 0) setgpr (rd, res);
                break;
                }

        case SP3_RDHWR:                                 /* RDHWR */
            if ((get_cp0_hwrena() & (1u << rd)) == 0)    /* not enabled? */
                ctx->traps |= TRAP_RSVI;
            else if (rt != 0) {
                switch (rd) {                           /* case on reg */

                case 0:                                 /* CPU num */
                    setgpr (rt, SEXT_W_D (ctx->cpu_num));
                    break;

                case 1:                                 /* SYNCI step */
                    setgpr (rt,  SIM_ULL(64));
                    break;

                case 2:                                 /* counter */
                    setgpr (rt, SEXT_W_D (get_cp0_count()));
                    break;

                case 3:                                 /* counterstep */
                    setgpr (rt, SIM_ULL(1));
                    break;

#if defined (_MIPS_SCTX_)
                case 30:
                    setgpr (rt, total_count);
                    break;
#endif
                    }                                   /* end switch */
                }                                       /* end else rt */

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }
        break;

#endif

/* Coprocessor 1 (opcode 11) */

    case OP_COP1:                                       /* coprocessor 1 (fp) */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        fs = I_GETFS (ir);                              /* fs */
        if (rs & 0x10) reason = op_cop1 (ctx, ir);      /* floating operate? */
        else switch (rs) {                              /* no, move or branch */

        case C1_MFC1:                                   /* MFC1 */
            if (rt != 0) {
                t64 = fp_getspr (ctx, fs);
                setgpr(rt, SEXT_W_D (t64));
                }
            break;
        case C1_DMFC1:                                  /* DMFC1 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            if (rt != 0) setgpr(rt, fp_getdpr (ctx, fs));
            break;
        case C1_CFC1:                                   /* CFC1 */
            if (rt != 0) {
                t64 = fp_getcr (ctx, fs);
                setgpr(rt, SEXT_W_D (t64));
                }
            break;

        case C1_MTC1:                                   /* MTC1 */
            fp_putspr (ctx, fs, gpr(rt));
            break;
        case C1_DMTC1:                                  /* DMTC1 */
            if (!Q_64BOP) {
                ctx->traps |= TRAP_RSVI;
                break;
                }
            fp_putdpr (ctx, fs, gpr(rt));
            break;
        case C1_CTC1:                                   /* CTC1 */
            fp_putcr (ctx, fs, gpr(rt));
            break;

        case C1_BC1:                                    /* BC1 */
            if (ctx->delay) {                           /* 5KF traps */
                ctx->traps |= TRAP_RSVI;
                break;
                }
            ctx->delay = 2;
            ctx->taken = fp_testcc (ctx, rt);
            if (ctx->taken) {
                dsp = I_GETDISP (ir);
                ctx->delay_PC = ctx->PC + (SEXT_DISP (dsp) << 2);
                }
            else if (rt & 2) ctx->events |= EVT_NLFY;
            break;

#if defined (_MIPS64_R2_)
        case C1_MFHC1:                                  /* MFHC1 */
            res = fp_getdpr (ctx, fs) >> 32;
            if (rt != 0) setgpr (rt, SEXT_W_D (res));
            break;

        case C1_MTHC1:                                  /* MTHC1 */
            res = fp_getdpr (ctx, fs);
            res = (res & M32) | (((t_uint64) (gpr(rt) & M32)) << 32);
            fp_putdpr (ctx, fs, res);
            break;
#endif

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }
        break;  

/* Coprocessor 1 extended (opcode 13) */

    case OP_COP1X:                                      /* coprocessor 1 ext (fp) */
        if (!Q_CP1) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        if (!Q_64BOP) {                                 /* no, 64b mode? */
            ctx->traps |= TRAP_RSVI;
            break;
            }
        fnc = I_GETFNC (ir);                            /* get function */
        fd = I_GETFD (ir);                              /* get fd */
        fs = I_GETFS (ir);                              /* fs */
        if (fnc & 0x20)                                 /* special MAC? */
            reason = op_cop1x (ctx, ir);
        else switch (fnc) {                             /* no, indexed load */

        case C1X_LWXC1:                                 /* LWXC1 */
            ea = gpr(rs) + gpr(rt);
            if (ReadW (ctx, ea, &t64))
                fp_putspr (ctx, fd, t64);
            break;
        case C1X_LDXC1:                                 /* LDXC1 */
            ea = gpr(rs) + gpr(rt);
            if (ReadD (ctx, ea, &t64))
                fp_putdpr (ctx, fd, t64);
            break;
        case C1X_LUXC1:                                 /* LUXC1 */
            ea = gpr(rs) + gpr(rt);
            if (ReadD (ctx, ea & ~7, &t64))
                fp_putdpr (ctx, fd, t64);
            break;

        case C1X_SWXC1:                                 /* SWXC1 */
            ea = gpr(rs) + gpr(rt);
            WriteW (ctx, ea, fp_getspr (ctx, fs));
            break;
        case C1X_SDXC1:                                 /* SDXC1 */
            ea = gpr(rs) + gpr(rt);
            WriteD (ctx, ea, fp_getdpr (ctx, fs));
            break;
        case C1X_SUXC1:                                 /* SUXC1 */
            ea = gpr(rs) + gpr(rt);
            WriteD (ctx, ea & ~7, fp_getdpr (ctx, fs));
            break;

        case C1X_PREFX:                                 /* PREFX */
            ea = gpr(rs) + gpr(rt);
            if (xlate_va (ctx, ea, VA_CR, &pa, &catr))  /* in TLB? */
                mem_pref (ctx, pa, catr, fd);           /* let mem handle */
            break;

#if defined (_MIPS_PS_)
        case C1X_ALNV:                                  /* ALNV.PS */
            op_cop1x_alnv (ctx, ir);
            break;
#endif

        default:
            ctx->traps |= TRAP_RSVI;
            break;
            }
        break;

/* External processors */

    case OP_COP0:                                       /* coprocessor 0 */
        if (!Q_CP0 && !Q_MD_K) {                        /* disabled, not kern? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        reason = op_cop0 (ctx, ir);
        break;

    case OP_COP2:                                       /* coprocessor 2 */
        if (!Q_CP2) {                                   /* disabled? */
            ctx->traps |= TRAP_CPUN;
            break;
            }
        reason = op_cop2 (ctx, ir);
        break;

    default:                                            /* undefined */
        ctx->traps |= TRAP_RSVI;
        break;
        }                                               /* end case op */
        STATS_EMULATION_DONE(ctx);
    }                                                   /* end if */
else set_pc(ctx->PC + 4);                               /* else PC incr */
} else {
    // count number of procs that are stalled
    global_stall++;
}

/* Post processing: traps and deferred branches */

if (ctx->traps) {                                       /* traps? */
    static uint32 trapno;
    static t_uint64 vec, backup_PC;

    for (trapno = 0; ((ctx->traps >> trapno) & 1) == 0; trapno++) ;

    if (ctx->traps & TRAP_SIMH)
        return trap_cause[trapno];
    ctx->trapbit = ctx->debug & (1u << trapno);
    if (ctx->trapbit) ctx->events |= EVT_TSTOP;
    else ctx->events &= ~EVT_TSTOP;
    set_cp0_cause (get_cp0_cause() & ~(CP0_CAUSE_COP|CP0_CAUSE_EXC));
    set_cp0_cause (get_cp0_cause() | (trap_cause[trapno] << CP0_CAUSE_V_EXC));
    if ((op == OP_COP2) ||
        (op == OP_LWC2) || (op == OP_LDC2) ||
        (op == OP_SWC2) || (op == OP_SDC2))
        set_cp0_cause (get_cp0_cause() | (2 << CP0_CAUSE_V_COP));
    else if ((op == OP_COP1) || (op == OP_COP1X) ||
        ((op == OP_SPECIAL) && (I_GETFNC (ir) == SP_MOVCI)) ||
        (op == OP_LWC1) || (op == OP_LDC1) ||
        (op == OP_SWC1) || (op == OP_SDC1))
        set_cp0_cause (get_cp0_cause() | (1 << CP0_CAUSE_V_COP));
    if (ctx->delay == 1) {
        if ((get_cp0_sr() & CP0_SR_EXL) == 0)            /* if ~exl, upd BD */
            set_cp0_cause (get_cp0_cause() | CP0_CAUSE_BD);
        backup_PC = ctx->PC - 8;
        }
    else {
        if ((get_cp0_sr() & CP0_SR_EXL) == 0)            /* if ~exl, upd BD */
            set_cp0_cause (get_cp0_cause() & ~CP0_CAUSE_BD);
        backup_PC = ctx->PC - 4;
        }

#ifdef TRAP_PRINT
    if (tdebug) fprintf(stderr, "[%d] TRAP %s cause %s PC %llx", ctx->cpu_num,
		    cpu0_deb[trapno].name, causes[trap_cause[trapno]], backup_PC);
#endif

    if (ctx->traps & TRAP_REFILL) {                     /* TLB miss? */
        uint32 rgn = VA_GETRGN (get_cp0_badva());
        if (get_cp0_sr() & CP0_SR_BEV)                   /* BEV set? */
            vec = VEC_TLB_BEV;
#if !defined (_MIPS64_R2_)                              /* MIPS R1 */
        else vec = VEC_GEN;
#else                                                   /* MIPS R2 */
        else vec = get_cp0_ebase() | CP0_EBASE_FIX;
#endif
#ifdef TRAP_PRINT
        if (tdebug) fprintf(stderr, "    TLBMISS va 0x%llx status 0x%x\n\r", get_cp0_badva(), get_cp0_sr());
#endif
        if (get_cp0_sr() & CP0_SR_EXL) vec += OFF_GEN;
        else {
            set_cp0_sr (get_cp0_sr() | CP0_SR_EXL);
            set_cp0_epc (backup_PC);
            if (((rgn == 0) && !(get_cp0_sr() & CP0_SR_UX)) ||
                ((rgn == 3) && !(get_cp0_sr() & CP0_SR_KX)))
                vec += OFF_TLB;
            else vec += OFF_XTLB;
            }
        }
#ifndef _WIN32
    else if ((gdb_socket_priv.state & SIMPSOCK_IO)&&
             (ctx->traps & TRAP_GEN) && 
             (ctx->traps & TRAP_BREAK)) {               /* gen err? */
        gdb_stub(ctx->cpu_num);
        set_pc(backup_PC);
        vec = backup_PC;
        }
#endif
    else if (ctx->traps & TRAP_GEN) {                   /* gen err? */
#ifdef TRAP_PRINT
        if (tdebug) fprintf(stderr, "    GEN     va 0x%llx status 0x%x\n\r", get_cp0_badva(), get_cp0_sr());
#endif
        if (get_cp0_sr() & CP0_SR_BEV)                   /* BEV set? */
            vec = VEC_TLB_BEV + OFF_GEN;
#if !defined (_MIPS64_R2_)                              /* MIPS R1 */
        else vec = VEC_GEN + OFF_GEN;
#else                                                   /* MIPS R2 */
        else vec = (get_cp0_ebase() | CP0_EBASE_FIX) + OFF_GEN;
#endif
        if (!(get_cp0_sr() & CP0_SR_EXL)) {
            set_cp0_sr (get_cp0_sr() | CP0_SR_EXL);
            set_cp0_epc (backup_PC);
            }   
        }
    else return trap_cause[trapno];                     
    ctx->delay = 0;
    ctx->taken = 0;
    ctx->events &= ~(EVT_NLFY|EVT_INT);
    ctx->traps = 0;
    PCQ_ENTRY;
    set_pc (vec);
    }

if (ctx->events & EVT_STALL) {                          /* ld/sd stalled */
    set_pc (ctx->last_PC);
} else {
if (ctx->delay) {                                       /* delay countdown? */
    if ((--ctx->delay == 0) && ctx->taken) {            /* countdown done? */
        PCQ_ENTRY;                                      /* yes, branch */
        set_pc (ctx->delay_PC);
        ctx->taken = 0;
        }
    }
}

return SCPE_OK;
}

/* 64b * 64b unsigned multiply */

t_uint64 uemul64 (t_uint64 a, t_uint64 b, t_uint64 *hi)
{
t_uint64 ahi, alo, bhi, blo, rhi, rmid1, rmid2, rlo;

ahi = (a >> 32) & M32;
alo = a & M32;
bhi = (b >> 32) & M32;
blo = b & M32;
rhi = ahi * bhi;
rmid1 = ahi * blo;
rmid2 = alo * bhi;
rlo = alo * blo;
rhi = rhi + ((rmid1 >> 32) & M32) + ((rmid2 >> 32) & M32);
rmid1 = (rmid1 << 32) & M64;
rmid2 = (rmid2 << 32) & M64;
rlo = (rlo + rmid1) & M64;
if (rlo < rmid1) rhi = rhi + 1;
rlo = (rlo + rmid2) & M64;
if (rlo < rmid2) rhi = rhi + 1;
if (hi) *hi = rhi & M64;
return rlo;
}

/* Byte extract */

t_uint64 cpu_ext (t_uint64 s, uint32 sizem1, uint32 lsb)
{
t_uint64 mask;

mask = (sizem1 < 63)? ((SIM_ULL(1) << (sizem1 + 1)) - 1): M64;
return ((s >> lsb) & mask);
}

/* Byte insert */

t_uint64 cpu_ins (t_uint64 d, t_uint64 s, uint32 msb, uint32 lsb)
{
uint32 sizem1 = msb - lsb;
t_uint64 mask;

if (msb >= lsb) {
    mask = (sizem1 < 63)? ((SIM_ULL(1) << (sizem1 + 1)) - 1): M64;
    return ((d & ~(mask << lsb)) | ((s & mask) << lsb));
    }
else return d;
}

/* Reset */

t_stat cpu_reset (DEVICE *dptr)
{
CORECTX *ctx = (CORECTX *) dptr->ctxt;
uint32 i;

setgpr(0, 0);
ctx->events = 0;
ctx->traps = 0;
ctx->trapbit = 0;
ctx->taken = 0;
ctx->delay = 0;
ctx->debug = 0;
set_cp0_tlbr (TLB_LNT - 1);
set_cp0_wired (0);
set_cp0_watchlo (0);
set_cp0_cnf (CP0_CNF_MBO|CP0_CNF_INIT);
set_cp0_sr (CP0_SR_BEV|CP0_SR_ERL);
set_cp0_count (0);
set_cp0_compr (0);
set_cp0_hwrena (0);                                    /* MipsR2 */
set_cp0_intctl (0);
set_cp0_ebase (0);
set_cp0_scr0 (0);
set_cp0_scr1 (0);
ctx->irq_count = 0;
ctx->irq_pins = 0;
ctx->cac_slow_mask = 0;
ctx->cac_slow_local = 0;
ctx->cac_EccMode = 0;
ctx->cac_EccStat = 0;
ctx->cac_EccSynd = 0;
ctx->cac_L2EccAddr = 0;
ctx->cac_CSWEccAddr = 0;
ctx->cac_TagEccAddr = 0;
for (i = 0; i < (2 * INT_N_HLVLS); i++)
    ctx->cac_icr[i] = 0;
for (i = 0; i < NUM_PERF; i++) {
    set_cp0_perf(i, 0);
    set_cp0_perf_ad(i, 0);
    set_cp0_perf_ad(i + NUM_PERF, 0);
    }
lock_reset (ctx->cpu_num);
cpu_ctx[ctx->cpu_num] = ctx;
if (sim_switches & SWMASK ('P')) {
    tlb_init (ctx);
    set_cp0_time (0);
    }
ctx->pcq_r = find_reg ("PCQ", NULL, dptr);
if (ctx->pcq_r)
    ctx->pcq_r->qptr = 0;
else return SCPE_IERR;
sim_dflt_dev = &cpu0_dev;
return SCPE_OK;
}

/* Bootstrap */

t_stat cpu_boot (int32 unitno, DEVICE *dptr)
{
CORECTX *ctx = (CORECTX *) dptr->ctxt;

set_pc (VEC_RESET);
ctx->delay = 0;
ctx->taken = 0;
ctx->events &= ~EVT_NLFY;
STATS_INIT();  // initialize statistics
return SCPE_OK;
}

t_stat cpu_detach (UNIT *uptr)
{
STATS_SHUTDOWN();
return SCPE_OK;
}

/* Memory examine */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 temp;
t_uint64 pa;
DEVICE *dptr;

if (vptr == NULL) return SCPE_ARG;
dptr = find_dev_from_unit (uptr);
if (dptr == NULL) return SCPE_IERR;
if (!xlate_va ((CORECTX *) dptr->ctxt, addr, VA_CR, &pa, &temp))
    return SCPE_REL;

#ifdef SIMH_CPUSIMH
*vptr = 0xfeedbeef;
return SCPE_OK;
#else
if (PA_IS_MEM (pa) && CALL_READPD ((CORECTX *) dptr->ctxt, pa, vptr, temp)) return SCPE_OK;
else if (CALL_READIO ((CORECTX *) dptr->ctxt, pa, vptr, L_DOUB)) return SCPE_OK;
#endif

return SCPE_NXM;
}

/* Memory deposit */

t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
uint32 temp;
t_uint64 pa;
DEVICE *dptr;

dptr = find_dev_from_unit (uptr);
if (dptr == NULL) return SCPE_IERR;
if (!xlate_va ((CORECTX *) dptr->ctxt, addr, VA_CW, &pa, &temp))
    return SCPE_REL;
if (CA_UNCACHED (temp)) {printf("addr %llx attr %d\n\r",addr,temp); return  SCPE_NOFNC; }
if (PA_IS_MEM (pa) && CALL_WRITEPD ((CORECTX *) dptr->ctxt, pa, val, temp)) return SCPE_OK;
else if (CALL_WRITEIO ((CORECTX *) dptr->ctxt, pa, val, L_DOUB)) return SCPE_OK;
return SCPE_NXM;
}

/* Show virtual address */

t_stat cpu_show_virt (FILE *of, UNIT *uptr, int32 val, void *desc)
{
t_stat r;
char *cptr = (char *) desc;
t_uint64 va, pa;
uint32 temp;

if (cptr) {
    DEVICE *dptr = find_dev_from_unit (uptr);
    if (dptr == NULL) return SCPE_IERR;
    va = get_uint (cptr, 16, M64, &r);
    if (r == SCPE_OK) {
        if (!xlate_va ((CORECTX *) dptr->ctxt, va, VA_CR, &pa, &temp)) {
            fprintf (of, "Translation error\n");
            return SCPE_OK;
            }
        fputs ("Virtual ", of);
        fprint_val (of, va, 16, 64, PV_LEFT);
        fputs (" = physical ", of);
        fprint_val (of, pa, 16, 64, PV_LEFT);
        fputc ('\n', of);
        return SCPE_OK;
        }
    }
fprintf (of, "Invalid argument\n");
return SCPE_OK;
}

/* Show TLB entry or entries */

t_stat cpu_show_tlb (FILE *of, UNIT *uptr, int32 val, void *desc)
{
DEVICE *dptr;
CORECTX *ctx;
char *cptr = (char *) desc;
TLBENT *tlbp;
t_addr lo, hi;

dptr = find_dev_from_unit (uptr);
if (dptr == NULL) return SCPE_IERR;
ctx = (CORECTX *) dptr->ctxt;
if (cptr) {
    cptr = get_range (dptr, cptr, &lo, &hi, 10, (val? (FTLB_LNT * FTLB_SETS): TLB_LNT), 0);
    if ((cptr == NULL) || (*cptr != 0)) return SCPE_ARG;
    }
else {
    lo = 0;
    hi = (val? (FTLB_LNT * FTLB_SETS): TLB_LNT) - 1;
    }
#if (FTLB_LNT)
tlbp = val? ctx->ftlb + lo: ctx->tlb + lo;
#else
if (val) return SCPE_NOFNC;
tlbp = &ctx->tlb[lo];
#endif

do {
    fprintf (of, "TLB %02X\tTAG=", (uint32) lo);
    fprint_val (of, tlbp->tag, 16, 64, PV_RZRO);
    fprintf (of, ", MASK=%X,", (uint32) tlbp->mask);
    fprintf (of, " SEL=");
    fprint_val (of, tlbp->sel, 16, 33, PV_LEFT);
    fprintf (of, ", INDX=%d,", tlbp->indx);
    fprintf (of, " FLAGS=%X/%X,", tlbp->f0, tlbp->f1);
    fprintf (of, " PFN0=");
    fprint_val (of, tlbp->pfn0, 16, 36, PV_RZRO);
    fprintf (of, ", PFN1=");
    fprint_val (of, tlbp->pfn1, 16, 36, PV_RZRO);
    fputc ('\n', of);
    tlbp++;
    lo++;
    } while (lo <= hi);

return SCPE_OK;
}

/* Set history */

t_stat cpu_set_hist (UNIT *uptr, int32 val, char *cptr, void *desc)
{
DEVICE *dptr;
CORECTX *ctx;
uint32 i, lnt;
t_stat r;

dptr = find_dev_from_unit (uptr);
if (dptr == NULL) return SCPE_IERR;
ctx = (CORECTX *) dptr->ctxt;
if (cptr == NULL) {
    for (i = 0; i < ctx->hst_lnt; i++) ctx->hist[i].pc = 0;
    ctx->hst_p = 0;
    return SCPE_OK;
    }
lnt = (uint32) get_uint (cptr, 10, HIST_MAX, &r);
if ((r != SCPE_OK) || (lnt && (lnt < HIST_MIN))) return SCPE_ARG;
ctx->hst_p = 0;
if (ctx->hst_lnt) {
    free (ctx->hist);
    ctx->hst_lnt = 0;
    ctx->hist = NULL;
    }
if (lnt) {
    ctx->hist = (struct Hist*)calloc (sizeof (struct Hist), lnt);
    if (ctx->hist == NULL) return SCPE_MEM;
    ctx->hst_lnt = lnt;
    }
return SCPE_OK;
}

/* Print one instruction */

void cpu_fprint_one_inst (FILE *st, uint32 ir, t_uint64 pc,
    t_uint64 rs, t_uint64 rt, t_uint64 rd, uint32 coreno)
{
t_uint64 ea;
t_value sim_val;
uint32 t, fmt;
uint32 rsreg = I_GETRS (ir);                                  /* get rs */
uint32 rtreg = I_GETRT (ir);                                  /* get rt */
uint32 rdreg = I_GETRD (ir);
extern t_stat fprint_sym (FILE *ofile, t_addr addr, t_value *val,
    UNIT *uptr, int32 sw);

static const uint16 h_fmt[64] = {
    H_OPR, H_RGI, H_JMP, H_JMP, H_BRA, H_BRA, H_BRA, H_BRA,
    H_IMM, H_IMM, H_IMM, H_IMM, H_IMM, H_IMM, H_IMM, H_IMM,
    H_OPR, H_OPR, H_OPR, H_OPR, H_BRA, H_BRA, H_BRA, H_BRA,
    H_IMM, H_IMM, H_MRF, H_MRF, H_OPR, H_NOP, H_NOP, H_NOP,
    H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF,
    H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF,
    H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF,
    H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF, H_MRF
    };

#ifdef SIMH_CPUSIMH
extern t_uint64 ScxGetCurrentSimTime();
fprintf (st, "[%lld] ", ScxGetCurrentSimTime());
#endif
if (coreno) fprintf (st, "#%d: ", coreno - 1);
pc = pc & ~3;
fmt = h_fmt[I_GETOP (ir)];                          /* get format */
if ((fmt == H_RGI) && !(ir & 0x80000))              /* reg/imm or branch? */
    fmt = H_BR1;                                    /* really a branch */
fprint_val (st, pc, 16, 64, PV_RZRO);
fputc (' ', st);
if (fmt & H_RS) {
    fprintf (st, (rsreg < 10)? " R%d=":"R%d=", rsreg);
    fprint_val (st, rs, 16, 64, PV_RZRO);
    }
else fputs ("                    ", st);
fputc (' ', st);
if (fmt & H_RT) {
    fprintf (st, (rtreg < 10)? " R%d=":"R%d=", rtreg);
    fprint_val (st, rt, 16, 64, PV_RZRO);
    }
else fputs ("                    ", st);
fputc (' ', st);
if (fmt & H_RD) {
    fprintf (st, (rdreg < 10)? " R%d=":"R%d=", rdreg);
    fprint_val (st, rd, 16, 64, PV_RZRO);
    }
else if (fmt & H_L16) {                             /* literal? */
    t = ir & M16;
    if (t & H_SIGN) fprintf (st, "               -%4X", 0x10000 - t);
    else fprintf (st, "                %4X", t);
    }
else if (fmt & H_BA) {                              /* branch? */
    ea = pc + 4 + (SEXT_DISP (ir) << 2);
    fputs (" ea=", st);
    fprint_val (st, ea, 16, 64, PV_RZRO);
    }
else if (fmt & H_JA) {                              /* jump? */
    ea = (pc & J_REGION) | (I_GETJT (ir) << 2);
    fputs (" ea=", st);
    fprint_val (st, ea, 16, 64, PV_RZRO);
    }
else if (fmt & H_MA) {                              /* mem ref? */
    ea = rs + SEXT_DISP (I_GETDISP (ir));
    fputs (" ea=", st);
    fprint_val (st, ea, 16, 64, PV_RZRO);
    }
else fputs ("                    ", st);
fputc (' ', st);
if (pc & 4) sim_val = ((t_uint64) ir) << 32;
else sim_val = ir;
if ((fprint_sym (st, pc, &sim_val, NULL, SWMASK ('M'))) > 0)
    fprintf (st, "(undefined) %08X", ir);
fputc ('\r', st);                                   /* end line */
fputc ('\n', st);                                   /* end line */
return;
}

/* Show history */

t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, void *desc)
{
DEVICE *dptr;
CORECTX *ctx;
int32 k, di, lnt;
t_stat r;
char *cptr = (char *) desc;
struct Hist *h;

dptr = find_dev_from_unit (uptr);                       /* find device */
if (dptr == NULL) return SCPE_IERR;
ctx = (CORECTX *) dptr->ctxt;                           /* find context */
if (ctx == NULL) return SCPE_IERR;
if (ctx->hst_lnt == 0) return SCPE_NOFNC;               /* enabled? */
if (cptr) {
    lnt = (int32) get_uint (cptr, 10, ctx->hst_lnt, &r);
    if ((r != SCPE_OK) || (lnt == 0)) return SCPE_ARG;
    }
else lnt = ctx->hst_lnt;
di = ctx->hst_p - lnt;                                  /* work forward */
if (di < 0) di = di + ctx->hst_lnt;
fprintf (st, "PC                Rs                   Rt                   Rd/ea               IR\n\n");
for (k = 0; k < lnt; k++) {                             /* print specified */
    h = &ctx->hist[(++di) % ctx->hst_lnt];              /* entry pointer */
    if (h->pc & HIST_PC)                                /* instruction? */
        cpu_fprint_one_inst (st, h->ir, h->pc, h->rs, h->rt, h->rd, 0);
    }                                                   /* end for */
return SCPE_OK;
}

/* Create a new core */

t_stat cpu_create (uint32 i)
{
DEVICE *dptr;
UNIT *uptr;
CORECTX *ctx;
REG *rptr, *wrptr;
char *nam;
size_t sz_d, sz_u, sz_c, sz_r;
t_uint64 offset;

dptr = (DEVICE*)malloc (sz_d = sizeof (cpu0_dev));
uptr = (UNIT*)malloc (sz_u = sizeof (cpu0_unit));
ctx = (CORECTX*)malloc (sz_c = sizeof (cpu0_ctx));
rptr = (REG*)malloc (sz_r = sizeof (cpu0_reg));
nam = (char*)malloc (8);
if (!dptr || !uptr || !ctx || !rptr || !nam) {
    if (dptr) free (dptr);
    if (uptr) free (uptr);
    if (ctx) free (ctx);
    if (rptr) free (rptr);
    if (nam) free (nam);
    return SCPE_MEM;
    }
memcpy (dptr, &cpu0_dev, sz_d);
memcpy (uptr, &cpu0_unit, sz_u);
memcpy (ctx, &cpu0_ctx, sz_c);
memcpy (rptr, &cpu0_reg, sz_r);
strcpy (nam, dptr->name);
cpu0_ctx.cpu_num = 0;
ctx->cpu_num = i;

nam[3] += i;                                            /* fixup name */
dptr->name = nam;                                       /* fixup DEVICE */
dptr->ctxt = (void *) ctx;
dptr->units = uptr;                             
dptr->registers = rptr;
dptr->lname = NULL;                                     /* no log name */
dptr->dctrl = 0;                                        /* no debugging */
dptr->flags |= (DEV_DIS|DEV_DISABLE);                   /* starts disabled */

for (wrptr = rptr; wrptr->name != NULL; wrptr++) {      /* fixup reg addr */
    offset = ((char *) wrptr->loc) - ((char *) &cpu0_ctx);
    wrptr->loc = ((char *) ctx) + offset;
    }

for (i = 0; i < DEV_MAX; i++) {                         /* add to dev list */
    if (sim_devices[i] == NULL) {
        sim_devices[i] = dptr;
        sim_devices[i + 1] = NULL;
        dptr->reset (dptr);                             /* reset device */
        return SCPE_OK;
        }
    }
return SCPE_IERR;
}
