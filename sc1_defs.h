/* sc1_defs.h: SiCortex 1 definitions file

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   28-Sep-07    RMS     Added Mips64R2 support
   16-Jan-06    RMS     Added TRACE capability
   29-Dec-05    RMS     Added L2 segment/CAC definitions
   05-Dec-05    RMS     Revised interrupt levels, UART reg spacing per chip spec
   18-Nov-05    RMS     Added WriteTest prototype
                        Fixed definitions of XKPHYS mbz, XKSEG end
                        Added hack for 32b user mode
   16-Nov-05    RMS     Fixed definitions in COP1X enum
   14-Oct-05    RMS     Fixed performance counter select definitions
   13-Oct-05    RMS     Revised for new memory map
   08-Sep-05    RMS     Reworked TLB for greater accuracy
                        Bit offset for coprocessor unit number misdefined
   07-Sep-05    RMS     Fixed WATCHI read mask
                        Added XCONTEXT read mask
   01-Sep-05    RMS     Added "trap on trap" support
   18-Aug-05    RMS     Changed counter, compare to 32b
   17-Aug-05    RMS     Changed initial memory size to 64MB, removed extraneous
                        context variables from old counter implementation

   Note: _MIPS64_R2_ is used to control Mips64 R2 features through the CPU */

#ifndef _SC1_DEFS_H_
#define _SC1_DEFS_H_    0

#include "sim_defs.h"
#if defined (_WIN32)
# define SIM_ULL(c)     (c##ui64)
#include "ice9_addr_spec_sw.h"
#else
# define SIM_ULL(c)     (c##ULL)
#include "sicortex/ice9/ice9_addr_spec_sw.h"
#endif

#ifdef __GNUC__
#define __WEAK __attribute__ ((weak))
#else
#define __WEAK
#endif

/* Simulation parameters */

#if !defined(SCX_PROD_BASE_ICE9A) && !defined(SCX_PROD_BASE_ICE9B) && !defined(SCX_PROD_BASE_TWC9A)
# define SCX_PROD_BASE_ICE9B	// Default it
#endif

/* TWC9 conditionals

   _MIPS_PS_            if defined, enables paired single floating point
   _MIPS64_R2_          if defined, enables Mips64 R2 features
   _MIPS64_R2_VI_       if defined, enables Mips64 R2 vectored interrupts
   _MIPS_HCVT_          if defined, enables full range hardware processing on fp converts
   _MIPS_SCTX_          if defined, enables SiCortex specific extensions
   _MIPS64_BVA_         if defined, enables large virtual address
   _MIPS64_BPA_         if defined, enables large physical address */

#if defined (SCX_PROD_BASE_TWC9A)
#define NUM_CORES       10
#define NUM_PERF        4
#define TLB_LNT         16

#define _MIPS_PS_       0                               /* uncomment when ready */
#define _MIPS64_R2_     0                               /* uncomment when ready */
//#define _MIPS64_R2_VI_  0                               /* uncomment when ready */
//#define _MIPS_HCVT_     0                               /* uncomment when ready */
#define _MIPS_SCTX_     0                               /* uncomment when ready */
//#define _MIPS64_BVA_    0                               /* uncomment when ready */
//#define _MIPS64_BPA_    0                               /* uncomment when ready */
#else
#define NUM_CORES       6
#define NUM_PERF        2
#define TLB_LNT         48
#endif

#if defined (_MIPS64_BVA_)
#define VA_W            58                              /* virt addr width */
#else
#define VA_W            40                              /* virt addr width */
#endif
#if defined (_MIPS64_BPA_)
#define PA_W            40                              /* phys addr width */
#else
#define PA_W            36                              /* phys addr width */
#endif

#if defined (_MIPS_SCTX_)
#define FTLB_LNT        64
#define FTLB_SETS       4
#else
#define FTLB_LNT        0
#define FTLB_SETS       0
#endif

#define DEV_MAX         40
#define INLINE
#define DEV_64B         (1u << (DEV_V_UF + 0))          /* 64b def length */
#define DEV_DIB         (1u << (DEV_V_UF + 1))          /* takes a DIB */
#define DEV_CORE        (1u << (DEV_V_UF + 2))          /* replicated per core */
#define MAX_NOPS        (1<<13)				            /* detect runaway */
#define MAX_STARVE      (256*1024)

/* MipsSim compatibility */

#define SLTIU_HOOKAB  	0xAB00	                        /* MIPS assigned magic */
#define SLTIU_HOOKAD  	0xAD00                          /* SiCortex assigned magic */

/* Bit patterns */

#define M8              0xFF
#define M16             0xFFFF
#define M32             0xFFFFFFFF
#define M64             SIM_ULL(0xFFFFFFFFFFFFFFFF)
#define B_SIGN          0x80
#define H_SIGN          0x8000
#define W_SIGN          0x80000000
#define D_SIGN          SIM_ULL(0x8000000000000000)

/* Simulator stops */
/* Be sure to update sim_stop_messages also */

#define STOP_HALT       1                               /* halt */
#define STOP_IBKPT      2                               /* breakpoint */
#define STOP_MME        3                               /* ex/dep mmgt err */
#define STOP_WAIT       4                               /* wait state */
#define STOP_FAIL       5                               /* test done, fail */
#define STOP_PASS       6                               /* test done, pass */
#define STOP_TRAP       7                               /* stop on trap */
#define STOP_HOOK	    8				                /* simulator hooks */

/* Vector bases and offsets */

#define VEC_RESET       SIM_ULL(0xFFFFFFFFBFC00000)
#define VEC_TLB_BEV     SIM_ULL(0xFFFFFFFFBFC00200)
#define VEC_DEBUG       SIM_ULL(0xFFFFFFFFBFC00480)
#define VEC_DEBUG_PT    SIM_ULL(0xFFFFFFFFFF200200)
#define VEC_CERR        SIM_ULL(0xFFFFFFFFA0000000)
#define VEC_CERR_BEV    SIM_ULL(0xFFFFFFFFBFC00200)
#define VEC_GEN         SIM_ULL(0xFFFFFFFF80000000)
#define VEC_GEN_BEV     SIM_ULL(0xFFFFFFFFBFC00200)

#define OFF_TLB         0x000
#define OFF_XTLB        0x080
#define OFF_CERR        0x100
#define OFF_GEN         0x180
#define OFF_INT         0x200

/* Instruction formats */

#define I_V_OP          26                              /* opcode */
#define I_M_OP          0x3F
#define I_V_JT          0                               /* jump target */
#define I_M_JT          0x3FFFFFF
#define I_V_RS          21                              /* Rs/fmt */
#define I_M_RS          0x1F
#define I_V_RT          16                              /* Rt/Ft */
#define I_M_RT          0x1F
#define I_V_DISP        0                               /* displacement */
#define I_M_DISP        0xFFFF
#define I_V_RD          11                              /* Rd/Fs */
#define I_M_RD          0x1F
#define I_V_SA          6                               /* shift/Fd */
#define I_M_SA          0x1F
#define I_V_FNC         0                               /* function */
#define I_M_FNC         0x3F
#define J_REGION        SIM_ULL(0xFFFFFFFFF0000000)     /* jump region */

#define I_GETOP(x)      (((x) >> I_V_OP) & I_M_OP)
#define I_GETRS(x)      (((x) >> I_V_RS) & I_M_RS)
#define I_GETRT(x)      (((x) >> I_V_RT) & I_M_RT)
#define I_GETRD(x)      (((x) >> I_V_RD) & I_M_RD)
#define I_GETSA(x)      (((x) >> I_V_SA) & I_M_SA)
#define I_GETFNC(x)     (((x) >> I_V_FNC) & I_M_FNC)
#define I_GETDISP(x)    (((x) >> I_V_DISP) & I_M_DISP)
#define I_GETJT(x)      (((x) >> I_V_JT) & I_M_JT)

#define I_GETFMT(x)     I_GETRS(x)
#define I_GETFR(x)      I_GETRS(x)
#define I_GETFT(x)      I_GETRT(x)
#define I_GETFS(x)      I_GETRD(x)
#define I_GETFD(x)      I_GETSA(x)

/* Floating memory format (IEEE S) */

#define S_V_SIGN        31
#define S_V_EXP         23
#define S_M_EXP         0xFF
#define S_SIGN          (1u << S_V_SIGN)
#define S_EXP           (S_M_EXP << S_V_EXP)
#define S_FRAC          SIM_ULL(0x00000000007FFFFF)
#define S_BIAS          0x7F
#define S_GETSIGN(x)    ((uint32) (((x) >> S_V_SIGN) & 1))
#define S_GETEXP(x)     ((uint32) (((x) >> S_V_EXP) & S_M_EXP))
#define S_GETFRAC(x)    ((x) & S_FRAC)

/* Floating point memory format (IEEE D) */

#define D_V_SIGN        63
#define D_V_EXP         52
#define D_M_EXP         0x7FF
#define D_SIGN          SIM_ULL(0x8000000000000000)
#define D_EXP           SIM_ULL(0x7FF0000000000000)
#define D_FRAC          SIM_ULL(0x000FFFFFFFFFFFFF)
#define D_BIAS          0x3FF
#define D_GETSIGN(x)    (((uint32) ((x) >> 63)) & 1)
#define D_GETEXP(x)     (((uint32) ((x) >> D_V_EXP)) & D_M_EXP)
#define D_GETFRAC(x)    ((x) & D_FRAC)

/* Virtual address */

#define VA_V_RGN        62                              /* region */
#define VA_M_RGN        0x3
#define  VA_USEG        0
#define  VA_SSEG        1
#define  VA_XKPHYS      2
#define  VA_XKSEG       3
#define VA_V_VPN        12
#define VA_M_VPN        0xFFFFFFF
#define VA_M_OFF        SIM_ULL(0x0000000000000FFF)
#define USEG_END        SIM_ULL(0x0000000080000000)
#define XUSEG_END       SIM_ULL(0x0000010000000000)
#define XSSEG_END       SIM_ULL(0x4000010000000000)
#define XKPHYS_MBZ      SIM_ULL(0x07FFFFF000000000)
#define XKSEG_END       SIM_ULL(0xC00000FF80000000)
#define XKSEG_COMP      SIM_ULL(0xFFFFFFFF80000000)
#define XKSEG_SSEG      SIM_ULL(0xFFFFFFFFC0000000)

#define VA_CON          4                               /* console access */
#define VA_INS          2                               /* instruction access */
#define VA_WRT          1                               /* write access */
#define VA_DAT          0                               /* data access */
#define VA_DR           (VA_DAT)
#define VA_DW           (VA_DAT|VA_WRT)
#define VA_IR           (VA_INS)
// no VA_IW
#define VA_CR           (VA_CON)
#define VA_CW           (VA_CON|VA_WRT)

#define VA_GETRGN(x)    (((uint32) ((x) >> VA_V_RGN)) & VA_M_RGN)
#define VA_GETVPN2(x)   (((uint32) ((x) >> (VA_V_VPN + 1))) & (VA_M_VPN >> 1))
#define VA_GETOFF(m,x)  ((x) & ((t_uint64) (VA_M_OFF | (m))))

/* Memory map

   000000000 - 7FFFFFFFF        physical memory
   800000000 - DFFFFFFFF        PCI-Express
   E00000000 - EFFFFFFFF        internal CSRs */

#define PA_MAX          SIM_ULL(0x0000001000000000)
#define PA_MASK         SIM_ULL(0x0000000FFFFFFFFF)
#define PA_MASK_LOCK    SIM_ULL(0x0000000FFFFFFFC0)     /* 64B granularity */
#define PA_MASK_29      SIM_ULL(0x000000001FFFFFFF)     /* 29b PA limit */
#define PA_IO           SIM_ULL(0x0000000800000000)     /* bit 35 PA is IO space */

#define INITMEMSIZE     (1 << 26)                       /* 64MB */
#define MEMSIZE         (mem_unit.capac)
#define PA_IS_MEM(x)    ((x) < MEMSIZE)
#define PA_IS_IO(x)     ((x) & PA_IO)

/* TLB - structure must be multiple of 8 bytes */

typedef struct {
    t_uint64            tag;                            /* tag */
    t_uint64            mask;                           /* mask */
    t_uint64            sel;                            /* select left/right */
    t_uint64            pfn0;
    t_uint64            pfn1;
    uint16              indx;                           /* entry number */
    uint8               f0;
    uint8               f1;
    uint32              pad;
    } TLBENT;

/* Fixed TLB is assumed to support 64KB pages only */

#define FTLB_PGSIZE     SIM_ULL(0x000000000001E000)
#define FTLB_M_VAIDX    0x3F
#define FTLB_V_VAIDX    17
#define FTLB_VAIDX(x)   ((((uint32) x) >> FTLB_V_VAIDX) & FTLB_M_VAIDX)

#define TLBT_MASK_VPN2  SIM_ULL(0xC00000FFFFFFE000)
#define TLBT_MASK_ASID  ((t_uint64) (CP0_EHI_M_ASID))
#define TLBT_MASK_G     ((t_uint64) ((CP0_EHI_M_ASID) + 1))
#define TLBF_G          0x01
#define TLBF_V          0x02
#define TLBF_D          0x04
#define TLBF_V_CA       3
#define TLBF_M_CA       0x7
#define TLBF_GETCA(x)   (((x) >> TLBF_V_CA) & TLBF_M_CA)

/* Utility macro for building register value from fields */
/* TODO add assert that value fits in (highb-lowb+1) bits */
#define REGFLD(name,highb,lowb,val)  ((val) << (lowb))

/* CP0 registers */

#define CP0_TLBI_R      0x8000003F                      /* 00 */
#define CP0_TBLI_W      0x0000003F                      /* 01 */
#define CP0_TLBR_R      0x0000003F
#define CP0_CTXT_R      SIM_ULL(0xFFFFFFFFFFFFFFF0)     /* 04 */
#define CP0_CTXT_VPN2   SIM_ULL(0x00000000007FFFF0)
#define CP0_CTXT_W      SIM_ULL(0xFFFFFFFFFF800000)
#define CP0_EPA_FIX     0x20000000                      /* 05-1 */
#define CP0_WIRED_RW    0x0000003F                      /* 06 */
#define CP0_HWRENA_RW   0x4000000F                      /* 07 */
#define CP0_WHI_RW      0x40FF0FF8                      /* 19 */
#define CP0_XCTXT_R     SIM_ULL(0xFFFFFFFFFFFFFFF0)     /* 20 */
#define CP0_XCTXT_W     SIM_ULL(0xFFFFFFFE00000000)      
#define CP0_XCTXT_RVPN2 SIM_ULL(0x00000001FFFFFFF0)

#define CP0_PID_V_WHAMI 24

#if defined(SCX_PROD_BASE_ICE9A)
# define CP0_PID         ((14 << 16) | (ICE9_E_AddrProduct_ICE9  << 8) | (1 << 0))
#elif defined(SCX_PROD_BASE_ICE9B)
# define CP0_PID         ((14 << 16) | (ICE9_E_AddrProduct_ICE9B << 8) | (1 << 0))
#elif defined(SCX_PROD_BASE_TWC9A)
# define CP0_PID         ((14 << 16) | (ICE9_E_AddrProduct_TWC9A << 8) | (1 << 0))
#else
# error Unknown SCX_PROD_BASE setting
#endif

/* CP0 entry low register (02, 03) */

#define CP0_ELO_V_PFN   6                               /* PFN */
#define CP0_ELO_FLAGS   0x3F
#define CP0_ELO_PFN     SIM_ULL(0x3FFFFFC0)
#define CP0_ELO_RW      SIM_ULL(0x3FFFFFFF)

/* Page mask register (05) */

#if defined (_MIPS_SCTX_)
#define CP0_MASK_RW     SIM_ULL(0x00000001FFFFE000)
#else
#define CP0_MASK_RW     SIM_ULL(0x000000001FFFE000)
#endif

/* CP0 entry hi register (10) */

#define CP0_EHI_M_ASID  0xFF
#define CP0_EHI_RVPN2   SIM_ULL(0xC00000FFFFFFE000)
#define CP0_EHI_RW      SIM_ULL(0xC00000FFFFFFE0FF)

/* CP0 status register (12) */

#define CP0_SR_IE       (1u << 0)                       /* int enable */
#define CP0_SR_EXL      (1u << 1)                       /* exc level */
#define CP0_SR_ERL      (1u << 2)                       /* err level */
#define CP0_SR_V_MD     3                               /* mode */
#define CP0_SR_M_MD     0x3
#define CP0_SR_MD       (CP0_SR_M_MD << CP0_SR_V_MD)
#define  MD_K           0
#define  MD_S           1
#define  MD_U           2
#define CP0_SR_UX       (1u << 5)                       /* user 64b addr */
#define CP0_SR_SX       (1u << 6)                       /* supv 64b addr */
#define CP0_SR_KX       (1u << 7)                       /* kern 64b addr */
#define CP0_SR_V_IM     8                               /* intr mask */
#define CP0_SR_M_IM     0xFF
#define CP0_SR_IM       (CP0_SR_M_IM << CP0_SR_V_IM)
#define CP0_SR_NMI      (1u << 19)                      /* non-mask intr */
#define CP0_SR_SR       (1u << 20)                      /* soft reset */
#define CP0_SR_TS       (1u << 21)                      /* TLB shutdown */
#define CP0_SR_BEV      (1u << 22)                      /* bootstrap vector */
#define CP0_SR_PX       (1u << 23)                      /* proc extension */
#define CP0_SR_FR       (1u << 26)                      /* 32b/64b F reg */
#define CP0_SR_CP0E     (1u << 28)                      /* coproc enables */
#define CP0_SR_CP1E     (1u << 29)
#define CP0_SR_CP2E     (1u << 30)
#define CP0_SR_CP3E     (1u << 31)
#define CP0_SR_RW       0xFEF8FFFF
#define CP0_SR_GETINT(x) (((x) >> CP0_SR_V_IM) & CP0_SR_M_IM)

/* CP0 interrupt control, Mips R2 only (12-1) */

#define CP0_INTCTL_V_TI 29
#define CP0_INTCTL_V_PI 26
#define CP0_INTCTL_V_VS 5
#define CP0_INTCTL_M_VS 0x7
#define CP0_INTCTL_GETVS(x) (((x) >> CP0_INTCTL_V_VS) & CP0_INTCTL_M_VS)
#define CP0_INTCTL_FIX  ((IRQ_N_COUNT << CP0_INTCTL_V_TI) | \
                         (IRQ_N_PERF << CP0_INTCTL_V_PI))
#define CP0_INTCTL_RW   0x000003E0

/* CP0 cause register (13) */

#define CP0_CAUSE_V_EXC 2                               /* exc cause */
#define CP0_CAUSE_M_EXC 0x1F
#define CP0_CAUSE_EXC   (CP0_CAUSE_M_EXC << CP0_CAUSE_V_EXC)
#define  CAUSE_INT      0                               /* interrupt */
#define  CAUSE_MOD      1                               /* TLB mod */
#define  CAUSE_TLBL     2                               /* TLB exc load/fetch */
#define  CAUSE_TLBS     3                               /* TLB exc store */
#define  CAUSE_ADEL     4                               /* addr err load/fetch */
#define  CAUSE_ADES     5                               /* addr err store */
#define  CAUSE_IBE      6                               /* bus err load/fetch */
#define  CAUSE_DBE      7                               /* bus err store */
#define  CAUSE_SYS      8                               /* SYSCALL */
#define  CAUSE_BRK      9                               /* BREAK */
#define  CAUSE_RI       10                              /* reserved inst */
#define  CAUSE_CPU      11                              /* coproc unusable */
#define  CAUSE_OV       12                              /* int overflow */
#define  CAUSE_TR       13                              /* TRAP instr */
#define  CAUSE_FPE      15                              /* fp exception */
#define  CAUSE_C2E      18                              /* coproc 2 exc */
#define  CAUSE_MDMX     22                              /* MDMX exc */
#define  CAUSE_WATCH    23                              /* watch exc */
#define  CAUSE_MCHK     24                              /* machine check */
#define  CAUSE_CERR     30                              /* cache err */
#define CP0_CAUSE_V_SIP 8                               /* interrupt pending */
#define CP0_CAUSE_M_SIP 0x3
#define CP0_CAUSE_V_HIP 10
#define CP0_CAUSE_M_HIP 0x3F
#define CP0_CAUSE_HIP   (CP0_CAUSE_M_HIP << CP0_CAUSE_V_HIP)
#define CP0_CAUSE_WP    (1u << 22)
#define CP0_CAUSE_IV    (1u << 23)
#define CP0_CAUSE_V_COP 28                              /* coprocessor number */
#define CP0_CAUSE_M_COP 0x3
#define CP0_CAUSE_COP   (CP0_CAUSE_M_COP << CP0_CAUSE_V_COP)
#define CP0_CAUSE_BD    (1u << 31)                      /* in delay slot */
#if defined (_MIPS64_R2_)
#define CP0_CAUSE_R     0xF4C0FF7C
#define CP0_CAUSE_W     0x00C00300
#define CP0_CAUSE_PCI   (1u << 26)                      /* R2 - perfc intr */
#define CP0_CAUSE_DC    (1u << 27)                      /* R2 - disable count */
#define CP0_CAUSE_TI    (1u << 30)                      /* R2 - timer intr */
#else
#define CP0_CAUSE_R     0xB0C0FF7C
#define CP0_CAUSE_W     0x00C00300
#define CP0_CAUSE_PCI   0
#define CP0_CAUSE_DC    0
#define CP0_CAUSE_TI    0
#endif

/* Interrupt request/mask flags - common to cp0_sr and cp0_cause */

#define INT_N_HLVLS     6                               /* # hardware ints */
#define INT_N_SLVLS     2                               /* # software ints */
#define CP0_IRQ2        (1u << (CP0_CAUSE_V_HIP + 0))
#define CP0_IRQ3        (1u << (CP0_CAUSE_V_HIP + 1))
#define CP0_IRQ4        (1u << (CP0_CAUSE_V_HIP + 2))
#define CP0_IRQ5        (1u << (CP0_CAUSE_V_HIP + 3))
#define CP0_IRQ6        (1u << (CP0_CAUSE_V_HIP + 4))
#define CP0_N_IRQ6      6
#define CP0_IRQ7        (1u << (CP0_CAUSE_V_HIP + 5))
#define CP0_N_IRQ7      7

/* CP0 exception base, R2 only (15-1) */

#define CP0_EBASE_RW    0x3FFFF000                      /* <29:12> */
#define CP0_EBASE_FIX   SIM_ULL(0xFFFFFFFF80000000)     /* fixed bits */
#define CP0_EBASE_FIXCE SIM_ULL(0xFFFFFFFFA0000000)     /* for cache err */

/* CP0 configuration registers (16, 16-1, 16-2, 16-3) */

#define CP0_CNF_TIWR    (1u << 21)                      /* global time wr */
#define CP0_CNF_SDTR    (1u << 20)                      /* disable SDBBP */
#define CP0_CNF_V_K0    0
#define CP0_CNF_M_K0    0x7
#define CP0_CNF_GETK0(x)    (((x) >> CP0_CNF_V_K0) & CP0_CNF_M_K0)
#define CP0_CNF_INIT    0x00000002

#if defined (SCX_PROD_BASE_TWC9A)                       /* Twice9 */

#define CP0_CNF_MBO     (REGFLD(MORE,31, 31,  1) | \
                        REGFLD(AT,  14, 13,  2) | \
                        REGFLD(AR,  12, 10,  1) | \
                        REGFLD(MT,   9,  7,  1))

#define CP0_CNF_R       (CP0_CNF_MBO|CP0_CNF_W)

#if defined (_MIPS_SCTX_)
#define CP0_CNF_W       (REGFLD(LLT, 24, 22,  7) | \
                        REGFLD(TIWR, 21, 21,  1) | \
                        REGFLD(SDTR, 20, 20,  1) | \
                        REGFLD(K0,   2,  0,  7))
#else
#define CP0_CNF_W       (REGFLD(LLT, 24, 22,  7) | \
                        REGFLD(K0,   2,  0,  7))
#endif

#define CP0_CNF1        (REGFLD(MORE,31, 31,  1) | \
                        REGFLD(MMU, 30, 25,  TLB_LNT - 1) | \
                        REGFLD(IS,  24, 22,  1) | \
                        REGFLD(IL,  21, 19,  5) | \
                        REGFLD(IA,  18, 16,  3) | \
                        REGFLD(DS,  15, 13,  1) | \
                        REGFLD(DL,  12, 10,  5) | \
                        REGFLD(DA,   9,  7,  3) | \
                        REGFLD(C2,   6,  6,  0) | \
                        REGFLD(MD,   5,  5,  0) | \
                        REGFLD(PC,   4,  4,  1) | \
                        REGFLD(WR,   3,  3,  1) | \
                        REGFLD(CA,   2,  2,  0) | \
                        REGFLD(EP,   1,  1,  1) | \
                        REGFLD(FP,   0,  0,  1))

#if 0 // AVP's don't like secondary cache defined
#define CP0_CNF2        (REGFLD(MORE,31, 31,  1) | \
                        REGFLD(SS,  11,  8,  4) | \
                        REGFLD(SL,   7,  4,  5) | \
                        REGFLD(SA,   3,  0,  3))

#else
#define CP0_CNF2        0x80000000
#endif

#define CP0_CNF3        (REGFLD(MORE,31, 31,  0) | \
                        REGFLD(LPA,  7,  7,  (PA_W > 36)) | \
                        REGFLD(VEIC, 6,  6,  0) | \
                        REGFLD(VI,   5,  5,  0))

#else                                                   /* ICE9 */
#define CP0_CNF_MBO     0x80004480
#define CP0_CNF_R       0xFC33FF87
#define CP0_CNF_W       0x7E120007

#define CP0_CNF1        (REGFLD(MORE,31, 31,  0) | \
                        REGFLD(MMU, 30, 25,  TLB_LNT - 1) | \
                        REGFLD(IS,  24, 22,  2) | \
                        REGFLD(IL,  21, 19,  4) | \
                        REGFLD(IA,  18, 16,  3) | \
                        REGFLD(DS,  15, 13,  2) | \
                        REGFLD(DL,  12, 10,  4) | \
                        REGFLD(DA,   9,  7,  3) | \
                        REGFLD(C2,   6,  6,  0) | \
                        REGFLD(MD,   5,  5,  0) | \
                        REGFLD(PC,   4,  4,  1) | \
                        REGFLD(WR,   3,  3,  1) | \
                        REGFLD(CA,   2,  2,  0) | \
                        REGFLD(EP,   1,  1,  1) | \
                        REGFLD(FP,   0,  0,  1))

#define CP0_CNF2        0

#define CP0_CNF3        0

#endif

#define CP0_CNF6_RW     0
#define CP0_CNF7_RW     0

#define Q_CP0           (get_cp0_sr() & CP0_SR_CP0E)
#define Q_CP1           (get_cp0_sr() & CP0_SR_CP1E)
#define Q_CP2           (get_cp0_sr() & CP0_SR_CP2E)
#define Q_MD_K          (((get_cp0_sr() & CP0_SR_MD) == 0) || (get_cp0_sr() & (CP0_SR_EXL|CP0_SR_ERL)))
#define Q_MD_U          ((get_cp0_sr() & (CP0_SR_MD|CP0_SR_EXL|CP0_SR_ERL)) == (MD_U << CP0_SR_V_MD))
#define Q_MD_U32        ((get_cp0_sr() & (CP0_SR_MD|CP0_SR_EXL|CP0_SR_ERL|CP0_SR_UX)) == (MD_U << CP0_SR_V_MD))
#define Q_64BOP         (!Q_MD_U || (get_cp0_sr() & (CP0_SR_UX|CP0_SR_PX)))

/* CP0 debug register (23) */

#define CP0_DEBUG_DBD	(1u << 31)	
#define CP0_DEBUG_DM  	(1u << 30)
#define CP0_DEBUG_IEXI	(1u << 20)
#define CP0_DEBUG_DINT	(1u <<  5)		                /* debug int */
#define CP0_DEBUG_DIB   (1u <<  4)		                /* debug instr brk */
#define CP0_DEBUG_DDBS	(1u <<  3)		
#define CP0_DEBUG_DDBL	(1u <<  2)		
#define CP0_DEBUG_DBP	(1u <<  1)		                /* sdbbp instr */
#define CP0_DEBUG_DSS	(1u <<  0)		                /* debug single step */
#define CP0_DEBUG_V_EXC 10                              /* exc cause */
#define CP0_DEBUG_M_EXC 0x1F
#define CP0_DEBUG_EXC   (CP0_DEBUG_M_EXC << CP0_DEBUG_V_EXC)
#define CP0_DEBUG_ENTER (CP0_DEBUG_DINT|CP0_DEBUG_DBP|CP0_DEBUG_DSS|CP0_DEBUG_DDBS|CP0_DEBUG_DDBL)
#define CP0_DEBUG_R     0xFEF7FD3F                      /* 23 */
#define CP0_DEBUG_W     0x10F00100

/* CP0 performance counters (25) */

#define CP0_PCTRL0      0
#define CP0_PCNTR0      1
#define CP0_PCTRL1      2
#define CP0_PCNTR1      3

#define CP0_PCNTR_OVF   0x80000000
#define CP0_PCTRL_V_EVT 5
#define CP0_PCTRL_M_EVT 0xF
#define  CP0_PEVT_CYC    0
#define  CP0_PEVT_INS    1
#define CP0_PCTRL_IE    0x10
#define CP0_PCTRL_U     0x08
#define CP0_PCTRL_S     0x04
#define CP0_PCTRL_K     0x02
#define CP0_PCTRL_EXL   0x01
#define CP0_PCTRL_RW    0x1FF
#define CP0_PCTRL_GETEVT(x) (((x) >> CP0_PCTRL_V_EVT) & CP0_PCTRL_M_EVT)

/* CP0 cache error control (26) */

#define CP0_ERRCTL_RW   0xFC0000FF
#define CP0_ERRCTL_ECCE 0x10000000                      /* enable ECC reporting */
#define CP0_CCHERR_R    0xB370FFFF                      /* 27 */

/* CP0 tag low (28) */

#if defined (_MIPS_SCTX_)

#define CP0_TLO_RW      SIM_ULL(0xFFFFFFFFFFFFFFE1)

#else

#define CP0_TLO_RW      SIM_ULL(0x00000000FFFFFFE1)

#endif

/* Coprocessor 1 (floating point) */

#define CP1_FIR         0
#define CP1_FCCR        25
#define CP1_FEXR        26
#define CP1_FENR        28
#define CP1_FPCR        31

/* FP ID register (CP1 0) */

#if defined (_MIPS_PS_)
#define FIR_VAL         0x00078101
#else
#define FIR_VAL         0x00038101
#endif

/* FP enables (CP 28) */

#define FENR_FS         0x00000004

/* FP control register (CP1 31) */

#define FPCR_V_RNDM     0
#define FPCR_M_RNDM     0x3
#define FPCR_RNDM       (FPCR_M_RNDM << FPCR_V_RNDM)
#define  FPCR_RN        0
#define  FPCR_RZ        1
#define  FPCR_RP        2
#define  FPCR_RM        3
#define FPCR_V_FLAGS    2
#define FPCR_M_FLAGS    0x1F
#define FPCR_FLAGS      (FPCR_M_FLAGS << FPCR_V_FLAGS)
#define  FPCR_INE       0x01
#define  FPCR_UNF       0x02
#define  FPCR_OVF       0x04
#define  FPCR_DZE       0x08
#define  FPCR_INV       0x10
#define  FPCR_UNI       0x20
#define FPCR_V_ENBLS    7
#define FPCR_M_ENBLS    0x1F
#define FPCR_ENBLS      (FPCR_M_ENBLS << FPCR_V_ENBLS)
#define FPCR_V_CAUSE    12
#define FPCR_M_CAUSE    0x3F
#define FPCR_CAUSE      (FPCR_M_CAUSE << FPCR_V_CAUSE)
#define FPCR_FN         (1u << 21)
#define FPCR_FO         (1u << 22)
#define FPCR_CC0        (1u << FPCR_V_CC0)
#define FPCR_FS         (1u << 24)
#define FPCR_V_CC0      23
#define FPCR_V_CC1      25
#define FPCR_M_CC1      0x7F
#define FPCR_CC1        (FPCR_M_CC1 << FPCR_V_CC1)
#define FPCR_CC         (FPCR_CC0|FPCR_CC1)
#define FPCR_GETCC(x)   ((((x) >> (FPCR_V_CC1 - 1)) & (FPCR_M_CC1 << 1)) | \
                        (((x) >> FPCR_V_CC0) & 1))
#define FPCR_GETENB(x)  (((x) >> FPCR_V_ENBLS) & FPCR_M_ENBLS)
#define FPCR_GETCAU(x)  (((x) >> FPCR_V_CAUSE) & FPCR_M_CAUSE)
#define FPCR_RW         0xFFE3FFFF

/* Cache attributes */

#define CA_UNCA         7				                /* uncached, accelerated */
#define CA_UNCA1        2				                /* uncached */
#define CA_CACHED       5				                /* cached, write-back, write-alloc */
#define CA_UNCACHED(x)	((x) == CA_UNCA || (x) == CA_UNCA1)

/* Macros - all arguments must be 64b! */

#define SEXT_B_D(x)     (((x) & B_SIGN)? ((x) | ~((t_uint64) M8)): ((x) & M8))
#define SEXT_H_D(x)     (((x) & H_SIGN)? ((x) | ~((t_uint64) M16)): ((x) & M16))
#define SEXT_W_D(x)     (((x) & W_SIGN)? ((x) | ~((t_uint64) M32)): ((x) & M32))
#define NEG_D(x)        (~(x) + 1)
#define ABS_D(x)        (((x) & D_SIGN)? NEG_D (x): (x))

#define SIGN_DISP       0x008000
#define SEXT_DISP(x)    (((x) & SIGN_DISP)? \
                        ((x) | ~((t_uint64) I_M_DISP)): ((x) & I_M_DISP))

/* Internal interrupts */

#define IRQ_PERF        CP0_IRQ7                        /* performance counters */
#define IRQ_N_PERF      CP0_N_IRQ7
#define IRQ_COUNT       CP0_IRQ6                        /* counter overflow */
#define IRQ_N_COUNT     CP0_N_IRQ6

/* Legacy global interrupts - this mechanism will be removed when all IO
   emulators transition to the L2/CAC interrupt API's */

#define INT_UART        CP0_IRQ5
#define INT_DMA         CP0_IRQ4
#define INT_ETH         CP0_IRQ3

/* I/O devices */

#define L_BYTE          0                               /* IO request lengths */
#define L_HALF          1
#define L_WORD          2
#define L_DOUB          3

/* Device information block */

struct sc1_dib {                                        /* device info block */
    t_uint64            low;                            /* low addr */
    t_uint64            high;                           /* high addr */
    t_bool              (*read)(t_uint64 pa, t_uint64 *val, uint32 lnt);
    t_bool              (*write)(t_uint64 pa, t_uint64 val, uint32 lnt);
    uint32              ipl;
    };

typedef struct sc1_dib DIB;

/* PC change queue and instruction history */

#define PCQ_SIZE        64                              /* must be 2**n */
#define PCQ_MASK        (PCQ_SIZE - 1)
#define PCQ_ENTRY       ctx->pcq[ctx->pcq_p = (ctx->pcq_p - 1) & PCQ_MASK] = ctx->PC - 4

#define HIST_PC         0x1
#define HIST_MIN        64
#define HIST_MAX        (1 << 20)

struct Hist {
    t_uint64            pc;
    uint32              ir;
    uint32              filler;
    t_uint64            rs;
    t_uint64            rt;
    t_uint64            rd;
    };

/* Processor core context */

struct core_ctx {
    t_uint64            PC;                             /* prog counter */
    t_uint64            delay_PC;                       /* delay slot PC */
    t_uint64            last_PC;                        /* previous PC */
    t_uint64            pcq[PCQ_SIZE];
    t_uint64            R[32];                          /* integer reg */
    t_uint64            F[32];                          /* floating reg */
    t_uint64            mhi;                            /* mhi */
    t_uint64            mlo;                            /* mlo */
    REG                 *pcq_r;                         /* addr of PC queue reg */
    struct Hist         *hist;                          /* instruction history */
    TLBENT              tlb[TLB_LNT];                   /* TLB */
#if (FTLB_LNT)
    TLBENT              ftlb[FTLB_LNT*FTLB_SETS];       /* TWC9 - FTLB */
    uint8               ftlb_lru[FTLB_LNT];             /* TWC9 - FTLB LRU */
#endif
    t_uint64            cp0_mask;                       /* CP0 mask */
    t_uint64            cp0_ctxt;                       /* CP0 context */
    t_uint64            cp0_xctxt;                      /* CP0 ext context */
    t_uint64            cp0_badva;                      /* CP0 bad VA */
    t_uint64            cp0_entlo0;                     /* CP0 entry low 0 */
    t_uint64            cp0_entlo1;                     /* CP0 entry low 1 */
    t_uint64            cp0_enthi;                      /* CP0 entry hi */
    t_uint64            cp0_epc;                        /* CP0 exc prog ctr */
    t_uint64            cp0_depc;                       /* CP0 debug exc PC */
    t_uint64            cp0_errpc;                      /* CP0 error PC */
    t_uint64            cp0_watchlo;                    /* CP0 watch low */
    t_uint64            cp0_taglo;                      /* CP0 tag low */
    t_uint64            cp0_datlo;                      /* CP0 data low */
    t_uint64            cp0_dathi;                      /* CP0 data high */
    t_uint64            cp0_desave;                     /* CP0 debug save */
    t_uint64            cp0_time;                       /* TWC9 - CP0 time */
    t_uint64            cp0_scr0;                       /* TWC9 - CP0 scratch 0 */
    t_uint64            cp0_scr1;                       /* TWC9 - CP0 scratch 1 */
    t_uint64            cp0_perf_ad[NUM_PERF * 2];      /* TWC9 - CP0 perf addr */
    t_uint64            cac_L2EccAddr;                  /* CAC L2 error addr */
    t_uint64            cac_CSWEccAddr;                 /* CAC CSW error addr */
    t_uint64            cac_TagEccAddr;                 /* CAC tag error addr */
    t_uint64	        d_mtlb_tag;                     /* d mini-TLB */
    t_uint64	        d_mtlb_pfn;
    t_uint64	        i_mtlb_tag;                     /* i mini-TLB */
    t_uint64	        i_mtlb_pfn;
    uint32              i_mtlb_fl;
    uint32              d_mtlb_fl;
    uint32              cp0_count;                      /* CP0 counter */
    uint32              cp0_compr;                      /* CP0 compare */
    uint32              cp0_tlbi;                       /* CP0 TLB index */
    uint32              cp0_tlbr;                       /* CP0 TLB random */
    uint32              cp0_wired;                      /* CP0 wired */
    uint32              cp0_sr;                         /* CP0 status reg */
    uint32              cp0_cause;                      /* CP0 cause reg */
    uint32              cp0_cnf;                        /* CP0 configuration */
    uint32              cp0_watchhi;                    /* CP0 watch high */
    uint32              cp0_debug;                      /* CP0 debug reg */
    uint32              cp0_perf[NUM_PERF * 2];         /* CP0 perf regs */
    uint32              cp0_errctl;                     /* CP0 error ctrl */
    uint32              cp0_ccherr;                     /* CP0 cache error */
    uint32              cp0_ebase;                      /* TWC9 - CP0 exc base */
    uint32              cp0_hwrena;                     /* TWC9 - CP0 RDHWR enb */
    uint32              cp0_intctl;                     /* TWC9 - CP0 int ctl */
    uint32              cp0_cnf6;                       /* TWC9 - CP0 config6 */
    uint32              cp0_cnf7;                       /* TWC9 - CP0 config7 */
    uint32              cpu_num;                        /* core number */
    uint32              fpcr;                           /* FP control reg */
    uint32              events;                         /* fetch events */
    uint32              delay;                          /* delay slot flag */
    uint32              taken;                          /* branch taken flag */
    uint32              traps;                          /* traps */
    uint32              trapbit;                        /* trap on trap bit */
    uint32              debug;                          /* debug flags */
    uint32              pcq_p;                          /* PC queue ptr */
    uint32              hst_p;                          /* history pointer */
    uint32              hst_lnt;                        /* history length */
    uint32              irq_count;                      /* counter interrupt */
    uint32              irq_pins;                       /* core intr req pins */
    uint32              cac_slow_mask;                  /* slow interrupt mask */
    uint32              cac_slow_local;                 /* slow local interrupts */
    uint32              cac_EccMode;                    /* CAC ECC mode */
    uint32              cac_EccStat;                    /* CAC ECC status */
    uint32              cac_EccSynd;                    /* CAC ECC syndrome */
    uint16              cac_icr[2 * INT_N_HLVLS];       /* int req registers */
    };

typedef struct core_ctx CORECTX;

#if defined(RAVEN_INTERFACE)

// Among other things, raven_interface.h defines all the accessor macros like
// setgpr, set_cp0_cause, etc. to both read/write the register and also to
// record which registers were read and written.
#include "raven_interface.h"

#elif defined(SIMH_EMULATION_LIBRARY)

#include "simh_emu.h"

#else /* the normal case */
// Define accessor macros to read/write architectural registers.
// The macros typically map to a simple read or write of the data value.
// But they can also be redefined to call functions that track when each
// register is read and written.
//
// This file contains the "normal" definitions of each macro.
// When RAVEN_INTERFACE is defined, skip over this section and the macros
// will be provided from raven_interface.h instead.

//Accessors for PC
#define get_pc() (ctx->PC)
#define set_pc(data) do { ctx->PC = (data); } while(0)

//Accessors for int and fp registers
#define gpr(regnum) (ctx->R[regnum])
#define fpr(regnum) (ctx->F[regnum])
#define setgpr(regnum,data) do{ (ctx->R[regnum]) = (data); } while (0)
#define setfpr(regnum,data) do{ (ctx->F[regnum]) = (data); } while (0)

// cp0_perf and cp0_perf_ad are arrays, so they need special treatment
#define get_cp0_perf(index) (ctx->cp0_perf[index])
#define get_cp0_perf_ad(index) (ctx->cp0_perf_ad[index])
#define set_cp0_perf(index,data) do { ctx->cp0_perf[(index) * 2] = (data); \
 ctx->cp0_perf[((index) * 2) + 1] = (data); } while(0)
#define set_cp0_perf_ad(index,data) do{ ctx->cp0_perf_ad[index] = (data); } while (0)

//Read accessors for everything else
#define get_mhi() (ctx->mhi)
#define get_mlo() (ctx->mlo)
#define get_cp0_ctxt() (ctx->cp0_ctxt)
#define get_cp0_badva() (ctx->cp0_badva)
#define get_cp0_entlo0() (ctx->cp0_entlo0)
#define get_cp0_entlo1() (ctx->cp0_entlo1)
#define get_cp0_enthi() (ctx->cp0_enthi)
#define get_cp0_epc() (ctx->cp0_epc)
#define get_cp0_depc() (ctx->cp0_depc)
#define get_cp0_errpc() (ctx->cp0_errpc)
#define get_cp0_watchlo() (ctx->cp0_watchlo)
#define get_cp0_taglo() (ctx->cp0_taglo)
#define get_cp0_datlo() (ctx->cp0_datlo)
#define get_cp0_dathi() (ctx->cp0_dathi)
#define get_cp0_desave() (ctx->cp0_desave)
#define get_cp0_count() (ctx->cp0_count)
#define get_cp0_compr() (ctx->cp0_compr)
#define get_cp0_tlbi() (ctx->cp0_tlbi)
#define get_cp0_tlbr() (ctx->cp0_tlbr)
#define get_cp0_mask() (ctx->cp0_mask)
#define get_cp0_wired() (ctx->cp0_wired)
#define get_cp0_sr()    (ctx->cp0_sr)
#define get_cp0_cause() (ctx->cp0_cause)
#define get_cp0_cnf() (ctx->cp0_cnf)
#define get_cp0_watchhi() (ctx->cp0_watchhi)
#define get_cp0_debug() (ctx->cp0_debug)
#define get_cp0_errctl() (ctx->cp0_errctl)
#define get_cp0_ccherr() (ctx->cp0_ccherr)
#define get_cp0_ebase() (ctx->cp0_ebase)
#define get_cp0_hwrena() (ctx->cp0_hwrena)
#define get_cp0_intctl() (ctx->cp0_intctl)
#define get_cp0_time() (ctx->cp0_time)
#define get_cp0_scr0() (ctx->cp0_scr0)
#define get_cp0_scr1() (ctx->cp0_scr1)
#define get_cp0_cnf6() (ctx->cp0_cnf6)
#define get_cp0_cnf7() (ctx->cp0_cnf7)
#define get_cp0_xctxt() (ctx->cp0_xctxt)
#define get_fpcr() (ctx->fpcr)

// Write accessors for everything else
#define set_mhi(data) do { ctx->mhi = (data); } while(0)
#define set_mlo(data) do { ctx->mlo = (data); } while(0)
#define set_cp0_ctxt(data) do { ctx->cp0_ctxt = (data); } while(0)
#define set_cp0_badva(data) do { ctx->cp0_badva = (data); } while(0)
#define set_cp0_entlo0(data) do { ctx->cp0_entlo0 = (data); } while(0)
#define set_cp0_entlo1(data) do { ctx->cp0_entlo1 = (data); } while(0)
#define set_cp0_enthi(data) do { ctx->cp0_enthi = (data); } while(0)
#define set_cp0_epc(data) do { ctx->cp0_epc = (data); } while(0)
#define set_cp0_depc(data) do { ctx->cp0_depc = (data); } while(0)
#define set_cp0_errpc(data) do { ctx->cp0_errpc = (data); } while(0)
#define set_cp0_watchlo(data) do { ctx->cp0_watchlo = (data); } while(0)
#define set_cp0_taglo(data) do { ctx->cp0_taglo = (data); } while(0)
#define set_cp0_datlo(data) do { ctx->cp0_datlo = (data); } while(0)
#define set_cp0_dathi(data) do { ctx->cp0_dathi = (data); } while(0)
#define set_cp0_desave(data) do { ctx->cp0_desave = (data); } while(0)
#define set_cp0_count(data) do { ctx->cp0_count = (data); } while(0)
#define set_cp0_compr(data) do { ctx->cp0_compr = (data); } while(0)
#define set_cp0_tlbi(data) do { ctx->cp0_tlbi = (data); } while(0)
#define set_cp0_tlbr(data) do { ctx->cp0_tlbr = (data); } while(0)
#define set_cp0_mask(data) do { ctx->cp0_mask = (data); } while(0)
#define set_cp0_wired(data) do { ctx->cp0_wired = (data); } while(0)
#define set_cp0_sr(data)    do { ctx->cp0_sr = (data); } while(0)
#define set_cp0_cause(data) do { ctx->cp0_cause = (data); } while(0)
#define set_cp0_cnf(data) do { ctx->cp0_cnf = (data); } while(0)
#define set_cp0_watchhi(data) do { ctx->cp0_watchhi = (data); } while(0)
#define set_cp0_debug(data) do { ctx->cp0_debug = (data); } while(0)
#define set_cp0_errctl(data) do { ctx->cp0_errctl = (data); } while(0)
#define set_cp0_ccherr(data) do { ctx->cp0_ccherr = (data); } while(0)
#define set_cp0_ebase(data) do { ctx->cp0_ebase = (data); } while(0)
#define set_cp0_hwrena(data) do { ctx->cp0_hwrena = (data); } while(0)
#define set_cp0_intctl(data) do { ctx->cp0_intctl = (data); } while(0)
#define set_cp0_time(data) do { ctx->cp0_time = (data); } while(0)
#define set_cp0_scr0(data) do { ctx->cp0_scr0 = (data); } while(0)
#define set_cp0_scr1(data) do { ctx->cp0_scr1 = (data); } while(0)
#define set_cp0_cnf6(data) do { ctx->cp0_cnf6 = (data); } while(0)
#define set_cp0_cnf7(data) do { ctx->cp0_cnf7 = (data); } while(0)
#define set_cp0_xctxt(data) do { ctx->cp0_xctxt = (data); } while(0)
#define set_fpcr(data) do { ctx->fpcr = (data); } while(0)





#if 0
// Alternative implementation of setgpr/setfpr for debugging, which prints
// registers as they are written.
#define setgpr(regnum,data) do{ \
  extern FILE* sim_deb; \
  (ctx->R[regnum]) = (data); \
  if (sim_deb) fprintf(sim_deb, "%d: R[%d]=0x%016llx\n", (int)ctx->cpu_num, (int)regnum, (unsigned long long)(data)); \
  } while (0)
#define setfpr(regnum,data) do{ \
    extern FILE* sim_deb;  \
    (ctx->F[regnum]) = (data); \
    if (sim_deb) fprintf(sim_deb, "%d: F[%d]=0x%016llx\n",(int)ctx->cpu_num,(int)regnum,(unsigned long long)data); \
    } while (0)
#endif

#endif /* end of if RAVEN_INTERFACE is defined */

/* Register blocks */

#define SC1_REG_MASK    0x3FFFFF
#define SC1_REG_OFF(a)  (((a) >> 3) & SC1_REG_MASK)

/* Boot ROM */

#define ROMAWIDTH       20                              /* ROM addr width */
#define ROMSIZE         (1u << ROMAWIDTH)               /* ROM length */
#define ROMAMASK        (ROMSIZE - 1)                   /* ROM addr mask */
#define ROMBASE_29      0x1FC00000                      /* ROM base, 29b */
#define PA_IS_ROM(x)    (((x) >= ROMBASE_29) && ((x) < (ROMBASE_29 + ROMSIZE)))

/* MIPS Product Revision */

#define PRADDRESS       0x1FC00010                      /* Memory location */
#define PRATLASID       0x00000000                      /* Atlas A Product Revision */
#define PRSEADID        0x00000010                      /* SEAD Product Revision */
#define PRMALTAID       0x00000020                      /* Malta Product Revision */
#define PRSEAD2ID       0x00000030                      /* SEAD-2 Product Revision */
#define PR3RDPTYID      0x000000E0                      /* 3rd Party Product Revision */

#define PRSC1000ID      0x00000040                      /* SC1000 Product Revision */

/* Traps and events */

enum exceptions {
    TR_V_SIER,
    TR_V_IBE,   TR_V_DBE,
    TR_V_MCHK,  TR_V_DBP,
    TR_V_CPUN,  TR_V_SYSCALL,   TR_V_BREAK,     TR_V_RSVI,
    TR_V_IOV,   TR_V_TRAP,      TR_V_FPE,       TR_V_LAER,
    TR_V_SAER,  TR_V_LTLBM,     TR_V_STLBM,     TR_V_LTLBV,
    TR_V_STLBV, TR_V_STLBW,     TR_V_SIMTRC
    };

enum events {
    EVT_V_INT,  EVT_V_WAIT,     EVT_V_BKPT,     EVT_V_HIST,
    EVT_V_NLFY, EVT_V_TSTOP,    EVT_V_STALL,    EVT_V_STALL_EPOCH,
    EVT_V_CCHE, EVT_V_DINT,	EVT_V_DBBP
    };

#define TRAP_SIER       (1u << TR_V_SIER)
#define TRAP_IBE        (1u << TR_V_IBE)
#define TRAP_DBE        (1u << TR_V_DBE)
#define TRAP_MCHK       (1u << TR_V_MCHK)
#define TRAP_DBP        (1u << TR_V_DBP)
#define TRAP_CPUN       (1u << TR_V_CPUN)
#define TRAP_SYSCALL    (1u << TR_V_SYSCALL)
#define TRAP_BREAK      (1u << TR_V_BREAK)
#define TRAP_RSVI       (1u << TR_V_RSVI)
#define TRAP_IOV        (1u << TR_V_IOV)
#define TRAP_TRAP       (1u << TR_V_TRAP)
#define TRAP_FPE        (1u << TR_V_FPE)
#define TRAP_LAER       (1u << TR_V_LAER)
#define TRAP_SAER       (1u << TR_V_SAER)
#define TRAP_LTLBM      (1u << TR_V_LTLBM)
#define TRAP_STLBM      (1u << TR_V_STLBM)
#define TRAP_LTLBV      (1u << TR_V_LTLBV)
#define TRAP_STLBV      (1u << TR_V_STLBV)
#define TRAP_STLBW      (1u << TR_V_STLBW)
#define TRAP_SIMTRC     (1u << TR_V_SIMTRC)             /* pseudo trap for tracing */

#define EVT_WAIT        (1u << EVT_V_WAIT)
#define EVT_INT         (1u << EVT_V_INT)
#define EVT_BKPT        (1u << EVT_V_BKPT)
#define EVT_HIST        (1u << EVT_V_HIST)
#define EVT_NLFY        (1u << EVT_V_NLFY)
#define EVT_TSTOP       (1u << EVT_V_TSTOP)
#define EVT_STALL       (1u << EVT_V_STALL)
#define EVT_STALL_EPOCH (1u << EVT_V_STALL_EPOCH)       /* stall this cpu until the next epoch */
#define EVT_CCHE        (1u << EVT_V_CCHE)
#define EVT_DINT        (1u << EVT_V_DINT)  		    /* external DINT asserted */
#define EVT_DBBP        (1u << EVT_V_DBBP)  		    /* debug breakpoint executed */

#define TRAP_SIMH       (TRAP_SIER)
#define TRAP_REFILL     (TRAP_LTLBM|TRAP_STLBM)
#define TRAP_GEN        (TRAP_IBE|TRAP_DBE|\
                         TRAP_MCHK|TRAP_CPUN|TRAP_SYSCALL|\
                         TRAP_BREAK|TRAP_RSVI|TRAP_IOV|TRAP_TRAP|\
                         TRAP_FPE|TRAP_LAER|TRAP_SAER|TRAP_LTLBM|\
                         TRAP_STLBM|TRAP_LTLBV|TRAP_STLBV|TRAP_STLBW)

/* Opcodes */

enum opcodes {
    OP_SPECIAL,OP_REGIMM, OP_J,      OP_JAL,
    OP_BEQ,    OP_BNE,    OP_BLEZ,   OP_BGTZ,
    OP_ADDI,   OP_ADDIU,  OP_SLTI,   OP_SLTIU,
    OP_ANDI,   OP_ORI,    OP_XORI,   OP_LUI,
    OP_COP0,   OP_COP1,   OP_COP2,   OP_COP1X,
    OP_BEQL,   OP_BNEL,   OP_BLEZL,  OP_BGTZL,
    OP_DADDI,  OP_DADDIU, OP_LDL,    OP_LDR,
    OP_SPECIAL2,OP_JALX,  OP_MDMX,   OP_SPECIAL3,
    OP_LB,     OP_LH,     OP_LWL,    OP_LW,
    OP_LBU,    OP_LHU,    OP_LWR,    OP_LWU,
    OP_SB,     OP_SH,     OP_SWL,    OP_SW,
    OP_SDL,    OP_SDR,    OP_SWR,    OP_CACHE,
    OP_LL,     OP_LWC1,   OP_LWC2,   OP_PREF,
    OP_LLD,    OP_LDC1,   OP_LDC2,   OP_LD,
    OP_SC,     OP_SWC1,   OP_SWC2,   OP_3B,
    OP_SCD,    OP_SDC1,   OP_SDC2,   OP_SD
    };

enum special {
    SP_SLL,    SP_MOVCI,  SP_SRL,    SP_SRA,
    SP_SLLV,   SP_05,     SP_SRLV,   SP_SRAV,
    SP_JR,     SP_JALR,   SP_MOVZ,   SP_MOVN,
    SP_SYSCALL,SP_BREAK,  SP_0E,     SP_SYNC,
    SP_MFHI,   SP_MTHI,   SP_MFLO,   SP_MTLO,
    SP_DSLLV,  SP_15,     SP_DSRLV,  SP_DSRAV,
    SP_MULT,   SP_MULTU,  SP_DIV,    SP_DIVU,
    SP_DMULT,  SP_DMULTU, SP_DDIV,   SP_DDIVU,
    SP_ADD,    SP_ADDU,   SP_SUB,    SP_SUBU,
    SP_AND,    SP_OR,     SP_XOR,    SP_NOR,
    SP_28,     SP_29,     SP_SLT,    SP_SLTU,
    SP_DADD,   SP_DADDU,  SP_DSUB,   SP_DSUBU,
    SP_TGE,    SP_TGEU,   SP_TLT,    SP_TLTU,
    SP_TEQ,    SP_35,     SP_TNE,    SP_37,
    SP_DSLL,   SP_39,     SP_DSRL,   SP_DSRA,
    SP_DSLL32, SP_3D,     SP_DSRL32, SP_DSRA32
    };

enum regimm {
    RI_BLTZ,   RI_BGEZ,   RI_BLTZL,  RI_BGEZL,
    RI_04,     RI_05,     RI_06,     RI_07,
    RI_TGEI,   RI_TGEIU,  RI_TLTI,   RI_TLTIU,
    RI_TEQI,   RI_0D,     RI_TNEI,   RI_0F,
    RI_BLTZAL, RI_BGEZAL, RI_BLTZALL,RI_BGEZALL,
    RI_SYNCI = 0x3F
    };

enum special2 {
    SP2_MADD,  SP2_MADDU, SP2_MUL,
    SP2_MSUB = 0x04,      SP2_MSUBU,
    SP2_CLZ = 0x20,       SP2_CLO,
    SP2_DCLZ =0x24,       SP2_DCLO,
    SP2_POP = 0x2C,       SP2_DPOP,
    SP2_SDBBP = 0x3F
    };

enum special3 {
    SP3_EXT,   SP3_DEXTM, SP3_DEXTU, SP3_DEXT,
    SP3_INS,   SP3_DINSM, SP3_DINSU, SP3_DINS,
    SP3_BSH = 0x20,       SP3_DBSH = 0x24,
    SP3_RDHWR = 0x3B
    };

enum sp3bsh {
    SP3BSH_SBH = 0x02,    SP3BSH_SHD = 0x05,
    SP3BSH_SEB = 0x10,    SP3BSH_SEH = 0x18
    };

enum fpop {
    FP_ADD,    FP_SUB,    FP_MUL,    FP_DIV,
    FP_SQRT,   FP_ABS,    FP_MOV,    FP_NEG,
    FP_ROUND_L,FP_TRUNC_L,FP_CEIL_L, FP_FLOOR_L,
    FP_ROUND_W,FP_TRUNC_W,FP_CEIL_W, FP_FLOOR_W,
    FP_10,     FP_MOVCF,  FP_MOVZ,   FP_MOVN,
    FP_14,     FP_RECIP,  FP_RSQRT,
    FP_CVT_S = 0x20,      FP_CVT_D,
    FP_CVT_W = 0x24,      FP_CVT_L,  FP_CVT_PS,
    FP_CVT_S_PL = 0x28,
    FP_PLL = 0x2C,FP_PLU, FP_PUL,    FP_PUU,
    FP_C_F = 0x30,FP_C_UN,FP_C_EQ,   FP_C_UEQ,
    FP_C_OLT,  FP_C_ULT,  FP_C_OLE,  FP_C_ULE,
    FP_C_SF,   FP_C_NGLE, FP_C_SEQ,  FP_C_NGL,
    FP_C_LT,   FP_C_NGE,  FP_C_LE,   FP_C_NGT
    };

enum cp0 {
    CP0_MFC0 = 0x00,      CP0_DMFC0,
    CP0_MTC0 = 0x04,      CP0_DMTC0,
    CP0_RDPGPR = 0x0A,    CP0_MFMC0,
    CP0_WRPGPR = 0x0E
    };

#define CP0_MFMC0_XI    0x12

enum cp0t {
    CP0T_TLBR = 0x01, CP0T_TLBWI, CP0T_TLBWR = 0x06,
    CP0T_TLBP = 0x08,
    CP0T_ERET = 0x18, CP0T_DERET = 0x1F,
    CP0T_WAIT = 0x20
    };  

enum cp1 {
    C1_MFC1,   C1_DMFC1,  C1_CFC1,   C1_MFHC1,
    C1_MTC1,   C1_DMTC1,  C1_CTC1,   C1_MTHC1,
    C1_BC1,
    C1_S = 0x10, C1_D = 0x11,
    C1_W = 0x14, C1_L = 0x15,
    C1_PS = 0x16
    };

enum cp1x {
    C1X_LWXC1 =   0x00, C1X_LDXC1 =   0x01, C1X_LUXC1 = 0x05,
    C1X_SWXC1 =   0x08, C1X_SDXC1 =   0x09, C1X_SUXC1 = 0x0D,
    C1X_PREFX =   0x0F, C1X_ALNV =    0x1E,
    C1X_MADD_S =  0x20, C1X_MADD_D =  0x21, C1X_MADD_PS = 0x26,
    C1X_MSUB_S =  0x28, C1X_MSUB_D =  0x29, C1X_MSUB_PS = 0x2E,
    C1X_NMADD_S = 0x30, C1X_NMADD_D = 0x31, C1X_NMADD_PS = 0x36,
    C1X_NMSUB_S = 0x38, C1X_NMSUB_D = 0x39, C1X_NMSUB_PS = 0x3E,
    };

/* Function prototypes */

t_bool ReadB (CORECTX *ctx, t_uint64 va, t_uint64 *val);
t_bool ReadH (CORECTX *ctx, t_uint64 va, t_uint64 *val);
t_bool ReadW (CORECTX *ctx, t_uint64 va, t_uint64 *val);
t_bool ReadD (CORECTX *ctx, t_uint64 va, t_uint64 *val);
t_bool ReadI (CORECTX *ctx, t_uint64 va, t_uint64 *val);
t_bool ReadIO (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool ReadPB (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ReadPH (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ReadPW (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ReadPD (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ReadPI (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool WriteB (CORECTX *ctx, t_uint64 va, t_uint64 dat);
t_bool WriteH (CORECTX *ctx, t_uint64 va, t_uint64 dat);
t_bool WriteW (CORECTX *ctx, t_uint64 va, t_uint64 dat);
t_bool WriteD (CORECTX *ctx, t_uint64 va, t_uint64 dat);
t_bool WriteTest (CORECTX *ctx, t_uint64 va);
t_bool WriteIO (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 lnt);
t_bool WritePB (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool WritePH (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool WritePW (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool WritePD (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool lock_test (uint32 num);
t_bool lock_clear (uint32 num);
t_bool lock_reset (uint32 num);
t_bool lock_set (uint32 num, t_uint64 addr, uint32 catr);
t_bool xlate_va (CORECTX *ctx, t_uint64 va, uint32 mode, t_uint64 *pa, uint32 *catr);
void eval_intr (CORECTX *ctx);
void eval_intr_all (void);
uint32 sprint_sym_m (char *cptr, t_addr addr, uint32 inst);
__WEAK t_bool mem_cache (CORECTX *ctx, uint32 ir, t_uint64 va, uint32 hint);
__WEAK void mem_sync (CORECTX *ctx);
__WEAK uint32 cac_eval_intr (CORECTX *ctx);


/*
 *  When SIMH is used as the cpu model for the System-C (SCX) environment
 *  we use SCX accessors for IO and memory. The SIMH versions are still 
 *  defined -- and may still be used if a block is modeled in SIMH. That is,
 *  if the SCX accessors don't claim a block the access will be sent back to 
 *  SIMH. This allows arbitrary mixing of simh-modeled and scx-modeled blocks.
 *
 *  The SIMH_CPUSIMH define means SCX is using simh as the cpu model engine. If
 *  only SIMH_EMBEDDED is defined, then SCX is just using the disassembler and
 *  SCX accessors are not defined or used.
 */
#if defined(SIMH_EMBEDDED) && defined(CPUSIMH0_IS_1)
#define SIMH_CPUSIMH

#define CALL_READIO	ScxReadIO
#define CALL_READPB	ScxReadPB
#define CALL_READPH	ScxReadPH
#define CALL_READPW	ScxReadPW
#define CALL_READPD	ScxReadPD
#define CALL_READPI	ScxReadPI

#define CALL_WRITEIO	ScxWriteIO
#define CALL_WRITEPB	ScxWritePB
#define CALL_WRITEPH	ScxWritePH
#define CALL_WRITEPW	ScxWritePW
#define CALL_WRITEPD	ScxWritePD

#define CALL_LOAD_WRITEPB ScxWritePB
#define CALL_LOAD_WRITEPW ScxWritePW
#define CALL_LOAD_WRITEIO ScxWriteIO

t_bool ScxReadIO (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool ScxReadPB (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ScxReadPH (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ScxReadPW (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ScxReadPD (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool ScxReadPI (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);

t_bool ScxWriteIO (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 lnt);
t_bool ScxWritePB (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool ScxWritePH (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool ScxWritePW (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool ScxWritePD (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);

t_bool ScxLockTest(uint32 num);

#define LOCK_TEST ScxLockTest

#elif defined(SIMH_EMULATION_LIBRARY)

/*
 *  When SIMH_EMULATION_LIBRARY is defined, the memory routines are
 *  taken over in a different way.  They are all redirected to callback
 *  functions so that the caller can control the inputs and outputs to
 *  each instruction.
 */

#define CALL_READIO	SimhEmuReadIO
#define CALL_READPB	SimhEmuReadPB
#define CALL_READPH	SimhEmuReadPH
#define CALL_READPW	SimhEmuReadPW
#define CALL_READPD	SimhEmuReadPD
#define CALL_READPI	SimhEmuReadPI

#define CALL_WRITEIO	SimhEmuWriteIO
#define CALL_WRITEPB	SimhEmuWritePB
#define CALL_WRITEPH	SimhEmuWritePH
#define CALL_WRITEPW	SimhEmuWritePW
#define CALL_WRITEPD	SimhEmuWritePD

#define CALL_LOAD_WRITEPB SimhEmuWritePB
#define CALL_LOAD_WRITEPW SimhEmuWritePW
#define CALL_LOAD_WRITEIO SimhEmuWriteIO

t_bool SimhEmuReadIO (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool SimhEmuReadPB (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool SimhEmuReadPH (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool SimhEmuReadPW (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool SimhEmuReadPD (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);
t_bool SimhEmuReadPI (CORECTX *ctx, t_uint64 pa, t_uint64 *val, uint32 catr);

t_bool SimhEmuWriteIO (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 lnt);
t_bool SimhEmuWritePB (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool SimhEmuWritePH (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool SimhEmuWritePW (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);
t_bool SimhEmuWritePD (CORECTX *ctx, t_uint64 pa, t_uint64 dat, uint32 catr);

t_bool SimhEmuLockTest(uint32 num);

#define LOCK_TEST SimhEmuLockTest

#else

#define CALL_READIO	ReadIO
#define CALL_READPB	ReadPB
#define CALL_READPH	ReadPH
#define CALL_READPW	ReadPW
#define CALL_READPD	ReadPD
#define CALL_READPI	ReadPI

#define CALL_WRITEIO	WriteIO
#define CALL_WRITEPB	WritePB
#define CALL_WRITEPH	WritePH
#define CALL_WRITEPW	WritePW
#define CALL_WRITEPD	WritePD

#define CALL_LOAD_WRITEPB WritePB
#define CALL_LOAD_WRITEPW WritePW
#define CALL_LOAD_WRITEIO WriteIO

#define LOCK_TEST lock_test

#endif

// A very rough sense of nanoseconds / instruction cycles
extern t_uint64 total_count;

#ifdef SIMH_CPUSIMH
extern t_uint64 ScxGetCurrentSimTime();
#define TIMESTAMP() ScxGetCurrentSimTime()
#else
#define TIMESTAMP() (total_count * 2)
#endif


#endif

