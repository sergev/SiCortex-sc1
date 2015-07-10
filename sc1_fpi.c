/* sc1_fpi.c - SiCortex 1 floating point simulator

   Copyright (c) 2007, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   25-Oct-07    RMS     Added paired single
   30-Nov-05    RMS     Fixed cvtfi machine-dependent shift bug at limit of exponent range
   18-Nov-05    RMS     ROUND.L, TRUNC.L, CEIL.L, FLOOR.L, RECIP, RSQRT trap in 32b mode
   16-Nov-05    RMS     Fixed multiple masking errors converting SNaN to QNaN
                        Fixed subtract of equal operands in round down mode
                        Fixed NaN handling in subtract
                        Fixed inf/0 handling in divide
                        Fixed single-precision max/min values for overflow/underflow
                        Fixed single-precision underflow test
                        Fixed convert-floating-to-integer invalid operation result
   26-Oct-05    RMS     Fixed multiple bugs in mul-add instructions
   16-Sep-05    RMS     Revised fp register model

   This module contains the instruction simulators for

        - single precision floating point, S
        - double precision floating point, D

   Portions of this module (specifically, the convert floating to integer
   routine and the square root routine) are a derivative work from SoftFloat,
   written by John Hauser.  SoftFloat includes the following license terms:

   Written by John R. Hauser.  This work was made possible in part by the
   International Computer Science Institute, located at Suite 600, 1947 Center
   Street, Berkeley, California 94704.  Funding was partially provided by the
   National Science Foundation under grant MIP-9311980.  The original version
   of this code was written as part of a project to build a fixed-point vector
   processor in collaboration with the University of California at Berkeley,
   overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
   is available through the Web page 'http://www.cs.berkeley.edu/~jhauser/
   arithmetic/SoftFloat.html'.

   THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
   been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
   RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
   AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
   COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
   EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
   INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
   OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

   Derivative works are acceptable, even for commercial purposes, so long as
   (1) the source code for the derivative work includes prominent notice that
   the work is derivative, and (2) the source code includes prominent notice with
   these four paragraphs for those parts of this code that are retained.
*/

#include "sc1_defs.h"

#define UFT_TRAP        0                               /* trap, abort op */
#define UFT_ZERO        1                               /* unpacked: zero */
#define UFT_FIN         2                               /* finite */
#define UFT_DENORM      3                               /* denormal */
#define UFT_INF         4                               /* infinity */
#define UFT_NAN         5                               /* not a number */

#define Q_FINITE(x)     ((x) <= UFT_FIN)                /* finite */

/* Instruction data type */

#define FP_S            0
#define FP_D            1
#define FP_W            4
#define FP_L            5
#define FP_PS           6                               /* PS (lower) */
#define FP_PSU          7                               /* fake - PS upper */

#define CMP_UN          0x1                             /* compare funcions */
#define CMP_EQ          0x2
#define CMP_LT          0x4
#define CMP_TQNAN       0x8

#define FP_GUARD        11                              /* left justify 53 bits */

#define CRR             ((ctx->fpcr >> FPCR_V_RNDM) & FPCR_M_RNDM)

/* Unpacked floating point number */

struct ufp {
    uint32              sign;
    int32               exp;                            /* D-biased! */
    t_uint64            frac;
    };

typedef struct ufp UFP;

/* Unpacked constants */

#define UF_V_NM         63
#define UF_NM           SIM_ULL(0x8000000000000000)     /* normalized */
#define UF_SNAN         SIM_ULL(0x4000000000000000)     /* signaling NaN */
#define UF_SRND         SIM_ULL(0x0000008000000000)     /* S normal round */
#define UF_SINF         SIM_ULL(0x000000FFFFFFFFFF)     /* S infinity round */
#define UF_DRND         SIM_ULL(0x0000000000000400)     /* D normal round */
#define UF_DINF         SIM_ULL(0x00000000000007FF)     /* D infinity round */

#define UF_CQN_E        D_M_EXP                         /* canonical quiet NaN */
#define UF_CQN_F        SIM_ULL(0xBFFFFFFFFFFFFFFF)

#define UF_INF_E        D_M_EXP                         /* infinity */
#define UF_INF_F        0

#define UF_MAX_E_D      (D_M_EXP - 1)                   /* max finite double */
#define UF_MAX_E_S      ((S_M_EXP - 1) - S_BIAS + D_BIAS)
#define UF_MAX_F        SIM_ULL(0xFFFFFFFFFFFFFFFF)     /* don't round! */

#define UF_MIN_E_D      1                               /* min finite double */
#define UF_MIN_E_S      (1 - S_BIAS + D_BIAS)
#define UF_MIN_F        SIM_ULL(0x8000000000000000)

static const uint32 fp_ccmap[8] = {                     /* map cc# to FPCR bit */
    1u << FPCR_V_CC0, 1u << FPCR_V_CC1,
    1u << (FPCR_V_CC1 + 1), 1u << (FPCR_V_CC1 + 2),
    1u << (FPCR_V_CC1 + 3), 1u << (FPCR_V_CC1 + 4),
    1u << (FPCR_V_CC1 + 5), 1u << (FPCR_V_CC1 + 6)
    };

static UFP ufp_zero = {
    0, 0, 0
    };

static UFP ufp_one = {
    0, D_BIAS, UF_NM
    };

t_bool fp_testcc (CORECTX *ctx, uint32 ccf);
void fp_setcc (CORECTX *ctx, uint32 ccf, uint32 val);
void fp_add (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd, t_bool sub);
void fp_mul (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd);
void fp_muladd (CORECTX *ctx, uint32 fnc, uint32 fs, uint32 ft, uint32 fr, uint32 fd);
void fp_div (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd);
void fp_sqrt (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
void fp_abs (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
void fp_neg (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
void fp_recip (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
void fp_rsqrt (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
void fp_cvtfi (CORECTX *ctx, uint32 fmt, uint32 ifmt, uint32 fs, uint32 fd, uint32 rndm);
void fp_cvtif (CORECTX *ctx, uint32 ifmt, uint32 fmt, uint32 fs, uint32 fd);
void fp_cvtsd (CORECTX *ctx, uint32 fs, uint32 fd);
void fp_cvtds (CORECTX *ctx, uint32 fs, uint32 fd);
void fp_cvtss (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd);
int32 fp_cmp (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd, t_bool tnan);
uint32 fp_upadd (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b);
uint32 fp_upmul (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b);
uint32 fp_updiv (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b);
uint32 fp_upcvtfi (CORECTX *ctx, uint32 ffmt, uint32 ifmt, uint32 ftpa, UFP *a, uint32 rndm);
uint32 fp_upsqrt (CORECTX *ctx, uint32 ftpb, UFP *b);
uint32 fp_unpack (CORECTX *ctx, uint32 fmt, uint32 rn, UFP *r, uint32 qdnm);
void fp_norm (UFP *r);
INLINE t_bool fp_round (uint32 fmt, UFP *r, uint32 rndm);
INLINE uint32 fp_ovftest (CORECTX *ctx, uint32 fmt, UFP *r, uint32 rndm);
INLINE uint32 fp_unftest (CORECTX *ctx, uint32 fmt, UFP *r, uint32 rndm);
uint32 fp_rtest (CORECTX *ctx, uint32 fmt, uint32 ftpr, UFP *r, uint32 rndm);
void fp_pack (CORECTX *ctx, uint32 fmt, uint32 rn, UFP *r);
t_bool fp_trap (CORECTX *ctx, uint32 trap);
t_uint64 fp_getspr (CORECTX *ctx, uint32 rn);
t_uint64 fp_getdpr (CORECTX *ctx, uint32 rn);
void fp_putspr (CORECTX *ctx, uint32 rn, t_uint64 val);
void fp_putdpr (CORECTX *ctx, uint32 rn, t_uint64 val);
INLINE void fp_upset(UFP *a, uint32 s, uint32 x, t_uint64 f);
void fp_rest_ps (CORECTX *ctx, uint32 fd, t_uint64 sv_fd, uint32 sv_fpcr);
uint32 estimateSqrt32 (uint32 exp, uint32 a);
t_uint64 estimateDiv128 (t_uint64 hi, t_uint64 lo, t_uint64 dvr);

t_uint64 udiv64 (t_uint64 dvd, t_uint64 dvr, uint32 prec, uint32 *sticky);
extern t_uint64 uemul64 (t_uint64 a, t_uint64 b, t_uint64 *hi);

/* COP1 floating operate */

t_stat op_cop1 (CORECTX *ctx, uint32 ir)
{
uint32 fnc, fmt, ft, fs, fd, sv_fpcr;
t_uint64 t64, sv_fd;
int32 cmp, cmp1;
#if defined (_MIPS_PS_)
static uint8 fp_valid[8][64] = {
 { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,
   0,1,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
 { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,
   1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 1,1,1,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
   1,0,0,0,0,0,0,0,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
 };
#else
static uint8 fp_valid[8][64] = {
 { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,
   0,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
 { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,
   1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
 { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
 };
#endif

fnc = I_GETFNC (ir);                                    /* get function */
fmt = I_GETFMT (ir) & 0xF;                              /* get format (S,D,W,L,PS) */
if ((fmt & 0x8) || !fp_valid[fmt][fnc]) {               /* valid instruction? */
    ctx->traps |= TRAP_RSVI;
    return SCPE_OK;
    } 
ft = I_GETFT (ir);                                      /* get registers */
fs = I_GETFS (ir);
fd = I_GETFD (ir);
set_fpcr (ctx->fpcr & ~FPCR_CAUSE);                     /* clear causes */
sv_fpcr = ctx->fpcr;                                    /* save fpcr */
sv_fd = ctx->F[fd];                                     /* save fd */
switch (fnc) {                                          /* case on func */

    case FP_ADD:                                        /* ADD.fmt */
        fp_add (ctx, fmt, fs, ft, fd, 0);
        if (fmt == FP_PS) {                             /* PS? */
            fp_add (ctx, FP_PSU, fs, ft, fd, 0);        /* do upper */
            if (ctx->traps & TRAP_FPE)                  /* trap? */
                fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);   /* restore state */
            }
        break;

    case FP_SUB:                                        /* SUB.fmt */
        fp_add (ctx, fmt, fs, ft, fd, 1);
        if (fmt == FP_PS) {                             /* PS? */
            fp_add (ctx, FP_PSU, fs, ft, fd, 1);        /* do upper */
            if (ctx->traps & TRAP_FPE)                  /* trap? */
                fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);   /* restore state */
            }
        break;

    case FP_MUL:                                        /* MUL.fmt */
        fp_mul (ctx, fmt, fs, ft, fd);
        if (fmt == FP_PS) {                             /* PS? */
            fp_mul (ctx, FP_PSU, fs, ft, fd);           /* do upper */
            if (ctx->traps & TRAP_FPE)                  /* trap? */
                fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);   /* restore state */
            }
        break;

    case FP_DIV:                                        /* DIV.fmt */
        fp_div (ctx, fmt, fs, ft, fd);
        break;

    case FP_SQRT:                                       /* SQRT.fmt */
        fp_sqrt (ctx, fmt, fs, fd);
        break;

    case FP_ABS:                                        /* ABS.fmt */
        fp_abs (ctx, fmt, fs, fd);
        if (fmt == FP_PS) {                             /* PS? */
            fp_abs (ctx, FP_PSU, fs, fd);               /* do upper */
            if (ctx->traps & TRAP_FPE)                  /* trap? */
                fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);   /* restore state */
            }
        break;

    case FP_NEG:                                        /* NEG.fmt */
        fp_neg (ctx, fmt, fs, fd);
        if (fmt == FP_PS) {                             /* PS? */
            fp_neg (ctx, FP_PSU, fs, fd);               /* do upper */
            if (ctx->traps & TRAP_FPE)                  /* trap? */
                fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);   /* restore state */
            }
        break;

    case FP_RECIP:                                      /* RECIP.fmt */
        if (!Q_64BOP) ctx->traps |= TRAP_RSVI;
        else fp_recip (ctx, fmt, fs, fd);
        break;

    case FP_RSQRT:                                      /* RSQRT.fmt */
        if (!Q_64BOP) ctx->traps |= TRAP_RSVI;
        else fp_rsqrt (ctx, fmt, fs, fd);
        break;

    case FP_MOV:                                        /* MOV.fmt */
        if (fmt != FP_S)                                /* dp or ps? */
            fp_putdpr (ctx, fd, fp_getdpr (ctx, fs));
        else fp_putspr (ctx, fd, fp_getspr (ctx, fs));
        break;

    case FP_MOVCF:                                      /* MOVC.fmt */
        if (fp_testcc (ctx, ft)) {                      /* eval condition */
            if (fmt != FP_S)                            /* dp or ps? */
                fp_putdpr (ctx, fd, fp_getdpr (ctx, fs));
            else fp_putspr (ctx, fd, fp_getspr (ctx, fs));
            }
        break;

    case FP_MOVZ:                                       /* MOVZ.fmt */
        if (gpr(ft) == 0) {
            if (fmt != FP_S)                            /* dp or ps? */
                fp_putdpr (ctx, fd, fp_getdpr (ctx, fs));
            else fp_putspr (ctx, fd, fp_getspr (ctx, fs));
            }
        break;

    case FP_MOVN:                                       /* MOVN.fmt */
        if (gpr(ft) != 0) {
            if (fmt != FP_S)                            /* dp or ps? */
                fp_putdpr (ctx, fd, fp_getdpr (ctx, fs));
            else fp_putspr (ctx, fd, fp_getspr (ctx, fs));
            }
        break;

    case FP_CVT_W:                                      /* CVT.W.fmt */
        fp_cvtfi (ctx, fmt, FP_W, fs, fd, CRR);
        break;

    case FP_ROUND_W:                                    /* cvt.W.fmt */
    case FP_TRUNC_W:
    case FP_CEIL_W:
    case FP_FLOOR_W:
        fp_cvtfi (ctx, fmt, FP_W, fs, fd, fnc & 3);
        break;

    case FP_CVT_L:                                      /* CVT.L.fmt */
        if (!Q_64BOP) ctx->traps |= TRAP_RSVI;
        else fp_cvtfi (ctx, fmt, FP_L, fs, fd, CRR);
        break;

    case FP_ROUND_L:                                    /* cvt.L.fmt */
    case FP_TRUNC_L:
    case FP_CEIL_L:
    case FP_FLOOR_L:
        if (!Q_64BOP) ctx->traps |= TRAP_RSVI;
        else fp_cvtfi (ctx, fmt, FP_L, fs, fd, fnc & 3);
        break;

    case FP_CVT_S:                                      /* CVT.S.fmt */
        if (fmt == FP_PS)                               /* CVT.S.PU? */
            fp_cvtss (ctx, FP_PSU, fs, fd);
        else if (fmt & 4) {                             /* from integer */
            if ((fmt & 1) && !Q_64BOP)                  /* from L, !64b? */
                ctx->traps |= TRAP_RSVI;
            else fp_cvtif (ctx, fmt, FP_S, fs, fd);
            }
        else fp_cvtds (ctx, fs, fd);                    /* from double */
        break;

    case FP_CVT_D:                                      /* CVT.D.fmt */
        if (fmt & 4) {                                  /* from integer */
            if ((fmt & 1) && !Q_64BOP)                  /* from L, !64b? */
                ctx->traps |= TRAP_RSVI;
            else fp_cvtif (ctx, fmt, FP_D, fs, fd);
            }
        else fp_cvtsd (ctx, fs, fd);                    /* from single */
        break;

    case FP_CVT_S_PL:                                   /* CVT.S.PL */
        fp_cvtss (ctx, FP_PS, fs, fd);
        break;

    case FP_CVT_PS:                                     /* CVT.PS.S */
    case FP_PLL:                                        /* PLL.PS */
        t64 = ((fp_getdpr (ctx, fs) << 32) & M64) |
               (fp_getdpr (ctx, ft) & SIM_ULL(0x00000000FFFFFFFF));
        fp_putdpr (ctx, fd, t64);
        break;

    case FP_PLU:                                        /* PLU.PS */
        t64 = ((fp_getdpr (ctx, fs) << 32) & M64) |
               (fp_getdpr (ctx, ft) >> 32);
        fp_putdpr (ctx, fd, t64);
        break;

    case FP_PUL:                                        /* PUL.PS */
        t64 = (fp_getdpr (ctx, fs) & SIM_ULL(0xFFFFFFFF00000000)) |
              (fp_getdpr (ctx, ft) & SIM_ULL(0x00000000FFFFFFFF));
        fp_putdpr (ctx, fd, t64);
        break;

    case FP_PUU:                                        /* PUU.PS */
        t64 = (fp_getdpr (ctx, fs) & SIM_ULL(0xFFFFFFFF00000000)) |
              (fp_getdpr (ctx, ft) >> 32);
        fp_putdpr (ctx, fd, t64);
        break;

    case FP_C_F: case FP_C_UN: case FP_C_EQ: case FP_C_UEQ:     /* C.cond */
    case FP_C_OLT: case FP_C_ULT: case FP_C_OLE: case FP_C_ULE:
    case FP_C_SF: case FP_C_NGLE: case FP_C_SEQ: case FP_C_NGL:
    case FP_C_LT: case FP_C_NGE: case FP_C_LE: case FP_C_NGT:
        cmp = fp_cmp (ctx, fmt, fs, ft, fnc & CMP_TQNAN);
        if (cmp >= 0)                                   /* no traps? */
            fp_setcc (ctx, fd, fnc & cmp);              /* set cc */
        if (fmt == FP_PS) {                             /* PS? */
            cmp1 = fp_cmp (ctx, FP_PSU, fs, ft, fnc & CMP_TQNAN);
            if (cmp1 >= 0)                              /* no traps? */
                fp_setcc (ctx, fd|4, fnc & cmp1);       /* set cc */
            }
        else cmp1 = 0;
        if ((cmp < 0) || (cmp1 < 0))
            set_fpcr ((ctx->fpcr & ~(FPCR_CC|FPCR_FLAGS)) |
                      (sv_fpcr & (FPCR_CC|FPCR_FLAGS)));
        break;
        }

return SCPE_OK;
}

/* S to D convert */

void fp_cvtsd (CORECTX *ctx, uint32 fs, uint32 fd)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, FP_S, fs, &a, 0)))         /* unpack sp reg; trap? */
    return;
if (ftpa == UFT_NAN)                                    /* NaN? cvt to quiet */
    a.frac &= ~UF_SNAN;
fp_pack (ctx, FP_D, fd, &a);                            /* exact, pack */
return;
}

/* D to S convert */

void fp_cvtds (CORECTX *ctx, uint32 fs, uint32 fd)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, FP_D, fs, &a, 0)))         /* unpack dp reg; trap? */
    return;
if (ftpa == UFT_NAN)                                    /* NaN? cvt to quiet */
    a.frac &= ~UF_SNAN;
if (fp_rtest (ctx, FP_S, ftpa, &a, CRR))                /* round, test; trap? */
    fp_pack (ctx, FP_S, fd, &a);                        /* no, pack */
return;
}

/* "S to S" convert - used in paired single only */

void fp_cvtss (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack ps reg; trap? */
    return;
if (ftpa == UFT_NAN)                                    /* NaN? cvt to quiet */
    a.frac &= ~UF_SNAN;
if (fp_rtest (ctx, FP_S, ftpa, &a, CRR))                /* round, test; trap? */
    fp_pack (ctx, FP_S, fd, &a);                        /* no, pack */
return;
}

/* Absolute value */

void fp_abs (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 1)))          /* unpack sp reg; SNaN trap? */
    return;
if (ftpa == UFT_NAN)                                    /* NaN? cvt to QNaN */
    a.frac &= ~UF_SNAN;
else a.sign = 0;
fp_pack (ctx, fmt, fd, &a);                             /* exact, pack result */
return;
}

/* Negate */

void fp_neg (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 1)))          /* unpack sp reg; SNaN trap? */
    return;
if (ftpa == UFT_NAN)                                    /* NaN? cvt to QNaN */
    a.frac &= ~UF_SNAN;
else a.sign = a.sign ^ 1;
fp_pack (ctx, fmt, fd, &a);                             /* exact, pack result */
return;
}

/* IEEE floating compare

   - Take care of NaNs
   - Force -0 to +0
   - Then normal compare will work (even on inf and denorms) */

int32 fp_cmp (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, t_bool tnan)
{
UFP a, b;
uint32 ftpa, ftpb;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 1)))          /* unpack A; trap? */
    return -1;
if (!(ftpb = fp_unpack (ctx, fmt, ft, &b, 1)))          /* unpack B; trap */
    return -1;
if ((ftpa == UFT_NAN) || (ftpb == UFT_NAN)) {           /* NaN involved? */
    if (tnan && fp_trap (ctx, FPCR_INV))                /* trap and taken? */
        return -1;
    return CMP_UN;                                      /* unordered */
    }
else {
    if (ftpa == UFT_ZERO)                               /* only +0 allowed */
        a.sign = 0;
    if (ftpb == UFT_ZERO)
        b.sign = 0;
    if (a.sign != b.sign)                               /* unequal signs? */
        return a.sign? CMP_LT: 0;                       /* LT or GT */
    else if (a.exp != b.exp)                            /* unequal exp? */
        return (a.sign ^ (a.exp < b.exp))? CMP_LT: 0;
    else if (a.frac != b.frac)                          /* unequal frac? */
        return (a.sign ^ (a.frac < b.frac))? CMP_LT: 0;
    }
return CMP_EQ;
}

/* IEEE integer to floating convert */

void fp_cvtif (CORECTX *ctx, uint32 ifmt, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a;
t_uint64 val;
uint32 qd = (fmt == FP_D)? 1: 0;
static const t_uint64 icvt_lim[2] = {                   /* single/double */
    0x7FFFFF, SIM_ULL(0x7FFFFFFFFFFFF)
    };

if (ifmt == FP_L)                                       /* long? */
    val = fp_getdpr (ctx, fs);
else {
    val = fp_getspr (ctx, fs);                          /* no, int, sext to l */
    val = SEXT_W_D (val);
    }
if (val == 0) {                                         /* 0? result +0 */
    fp_pack (ctx, fmt, fd, &ufp_zero);
    return;
    }
if (val & D_SIGN) {                                     /* < 0? */
    a.sign = 1;                                         /* set sign */
    val = NEG_D (val);                                  /* |val| */
    }
else a.sign = 0;
#if !defined (_MIPS_HCVT_)                              /* original FPU? */
if (val > (icvt_lim[qd] + a.sign)) {                    /* outside range? */
    fp_trap (ctx, FPCR_UNI);                            /* unimplemented */
    return;
    }
#endif
a.exp = 63 + D_BIAS;                                    /* set exp */
a.frac = val;                                           /* set frac */
fp_norm (&a);                                           /* normalize */
if (fp_rtest (ctx, fmt, UFT_FIN, &a, CRR))              /* round, test; trap? */
    fp_pack (ctx, fmt, fd, &a);                         /* no, pack */
return;
}

/* IEEE floating to integer convert - rounding code from SoftFloat */

void fp_cvtfi (CORECTX *ctx, uint32 fmt, uint32 ifmt, uint32 fs, uint32 fd, uint32 rndm)
{
UFP a;
uint32 ftpa;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack; trap? */
    return;
if (!fp_upcvtfi (ctx, fmt, ifmt, ftpa, &a, rndm))       /* convert; trap? */
    return;
if (ifmt == FP_L)
    fp_putdpr (ctx, fd, a.frac);
else fp_putspr (ctx, fd, (uint32) a.frac);
return;
}

uint32 fp_upcvtfi (CORECTX *ctx, uint32 ffmt, uint32 ifmt, uint32 ftpa, UFP *a, uint32 rndm)
{
t_uint64 sticky;
uint32 ovf;
uint32 qd = (ffmt == FP_D)? 1: 0;
uint32 ql = (ifmt == FP_L)? 1: 0;
int32 ubexp;
int32 exp_lim[2][2] = {
    { 0x16, 0x16}, { 0x1D, 0x33 }
    };
t_uint64 cvtfi_lim[2] = {
    SIM_ULL(0x000000007FFFFFFF), SIM_ULL(0x7FFFFFFFFFFFFFFF)
    };

if (!Q_FINITE (ftpa)) {                                 /* non-finite? */
    if (fp_trap (ctx, FPCR_INV))                        /* inv operation */
        return UFT_TRAP;
    a->frac = cvtfi_lim[ql];                            /* default is always + */
    return UFT_FIN;
    }
if (ftpa != UFT_ZERO) {                                 /* non-zero? */
    ubexp = a->exp - D_BIAS;                            /* unbiased exp */
    if (ubexp < 0) {                                    /* < 1? */
        if (ubexp == -1) sticky = a->frac;              /* [.5,1)? */
        else sticky = 1;                                /* (0,.5) */
        a->frac = 0;
        ovf = 0;                                        /* no overflow */
        }
    else if (ubexp <= UF_V_NM) {                        /* in range? */
#if !defined (_MIPS_HVCT_)
        if (ubexp > exp_lim[ql][qd]) {                  /* hwre punts? */
            fp_trap (ctx, FPCR_UNI);                    /* let swre handle */
            return UFT_TRAP;
            }
#endif
        if (ubexp < UF_V_NM) {                          /* below max? */
            sticky = a->frac << (64 - (UF_V_NM - ubexp));
            a->frac = a->frac >> (UF_V_NM - ubexp);     /* result */
            }
        else sticky = 0;                                /* at max, no shift */
        ovf = 0;                                        /* no overflow */
        }
    else {                                              /* out of range */
        sticky = 0;                                     /* max result */
        ovf = 1;
        }
    if (((rndm == FPCR_RN) && (sticky & D_SIGN)) ||     /* nearest? */
        ((rndm == FPCR_RP) && !a->sign && sticky) ||    /* +inf and +? */
        ((rndm == FPCR_RM) && a->sign && sticky)) {     /* -inf and -? */
        a->frac = a->frac + 1;
        if (a->frac == 0)                               /* overflow? */
            ovf = 1;
        if ((rndm == FPCR_RN) && (sticky == D_SIGN))    /* round nearest hack */
            a->frac = a->frac & ~1;
        }
    if (ovf || (a->frac > (cvtfi_lim[ql] + a->sign))) { /* overflow? */
#if defined (_MIPS_HCVT_)
        if (fp_trap (ctx, FPCR_OVF|FPCR_INE))           /* ovfl, inex; trap? */
            return UFT_TRAP;
        a->frac = cvtfi_lim[ql];                        /* max value */
#else
        fp_trap (ctx, FPCR_UNI);                        /* let swre handle */
        return UFT_TRAP;
#endif
        }
    if (sticky && fp_trap (ctx, FPCR_INE))              /* INE trap? */
         return UFT_TRAP;
    }
if (a->sign)
    a->frac = NEG_D (a->frac);
return UFT_FIN;
}

/* Floating add */

void fp_add (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd, t_bool sub)
{
UFP a, b;
uint32 ftpa, ftpb, ftpr;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack A; trap? */
    return;
if (!(ftpb = fp_unpack (ctx, fmt, ft, &b, 0)))          /* unpack B; trap */
    return;
if (sub && (ftpb != UFT_NAN)) b.sign = b.sign ^ 1;
if ((ftpr = fp_upadd (ctx, ftpa, &a, ftpb, &b)) &&      /* add ok? */
     fp_rtest (ctx, fmt, ftpr, &a, CRR))                /* round ok? */
    fp_pack (ctx, fmt, fd, &a);                         /* yes, pack and store */
return;
}

/* Unpacked add - used by fadd and fmac 

   - Take care of NaNs and infinites
   - Test for zero (fast exit)
   - Sticky logic for floating add
        > If result normalized, sticky in right place
        > If result carries out, renormalize, retain sticky
   - Sticky logic for floating subtract
        > If shift < guard, no sticky bits; 64b result is exact
          If shift <= 1, result may require extensive normalization,
          but there are no sticky bits to worry about
        > If shift >= guard, there is a sticky bit,
          but normalization is at most 1 place, sticky bit is retained
          for rounding purposes (but not in low order bit)
   - Note special case processing of zero results for subtract and
     round to minus infinity */

uint32 fp_upadd (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b)
{
UFP t;
uint32 sticky;
int32 ediff;

if (ftpa == UFT_NAN) {                                  /* A = NaN? quiet A */
    a->frac &= ~UF_SNAN;
    return UFT_NAN;
    }
if (ftpb == UFT_NAN) {                                  /* B = NaN? quiet B */
    fp_upset (a, b->sign, b->exp, b->frac & ~UF_SNAN);
    return UFT_NAN;
    }
if (ftpb == UFT_INF) {                                  /* B = inf? */
    if ((ftpa == UFT_INF) && (a->sign ^ b->sign)) {     /* eff sub of inf? */
        if (fp_trap (ctx, FPCR_INV))                    /* inv op; trap? */
            return UFT_TRAP;
        fp_upset (a, 0, UF_CQN_E, UF_CQN_F);            /* no trap, CQNaN */
        return UFT_NAN;
        }
    else fp_upset (a, b->sign, UF_INF_E, UF_INF_F);     /* return B */
    return UFT_INF;
    }
if (ftpa == UFT_INF) return UFT_INF;                    /* A = inf? return A */
if (ftpa == UFT_ZERO) {                                 /* A = 0? */
    if (ftpb != UFT_ZERO) *a = *b;                      /* if B != 0, return B */
    else if (a->sign != b->sign)                        /* both 0, subtract? */
        a->sign = (CRR == FPCR_RM);                     /* +0 unless RM */
    }
else if (ftpb != UFT_ZERO) {                            /* B != 0? */
    if ((a->exp < b->exp) ||                            /* A < B? swap */
        ((a->exp == b->exp) && (a->frac < b->frac))) {
        t = *a;
        *a = *b;
        *b = t;
        }
    ediff = a->exp - b->exp;                            /* exp diff */
    if (ediff > 63) b->frac = 1;                        /* >63? retain sticky */
    else if (ediff) {                                   /* [1,63]? shift */
        sticky = (b->frac << (64 - ediff))? 1: 0;       /* lost bits */
        b->frac = (b->frac >> ediff) | sticky;
        }
    if (a->sign ^ b->sign) {                            /* eff sub? */
        a->frac = a->frac - b->frac;                    /* subtract fractions */
        if (a->frac == 0) {                             /* sub, result 0? */
            a->exp = 0;
            a->sign = (CRR == FPCR_RM);
            }
        else fp_norm (a);                               /* normalize */
        }
    else {                                              /* eff add */
        a->frac = a->frac + b->frac;                    /* add frac */
        if (a->frac < b->frac) {                        /* chk for carry */
            a->frac = UF_NM | (a->frac >> 1) |          /* shift in carry */
                (a->frac & 1);                          /* retain sticky */
            a->exp = a->exp + 1;                        /* skip norm */
            }                                           /* end if carry */
        }                                               /* end else add */
    }                                                   /* end else if nz */
return UFT_FIN;
}

/* Floating multiply */

void fp_mul (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd)
{
UFP a, b;
uint32 ftpa, ftpb, ftpr;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack A; trap? */
    return;
if (!(ftpb = fp_unpack (ctx, fmt, ft, &b, 0)))          /* unpack B; trap */
    return;
if ((ftpr = fp_upmul (ctx, ftpa, &a, ftpb, &b)) &&      /* multiply ok? */
     fp_rtest (ctx, fmt, ftpr, &a, CRR))                /* round ok? */
    fp_pack (ctx, fmt, fd, &a);                         /* yes, pack and store */
return;
}

/* Unpacked floating multiply - used by fadd and fmac

   - Take care of NaNs and infinites
   - Test for zero operands
   - 64b x 64b fraction multiply, yielding 128b result
   - Normalize (at most 1 bit)
   - Insert "sticky" bit in low order fraction, for rounding
   
   Because IEEE fractions have a range of [1,2), the result can have a range
   of [1,4).  Results in the range of [1,2) appear to be denormalized by one
   place, when in fact they are correct.  Results in the range of [2,4) appear
   to be in correct, when in fact they are 2X larger.  This problem is taken
   care of in the result exponent calculation. */

uint32 fp_upmul (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b)
{
t_uint64 resl;

if (ftpa == UFT_NAN) {                                  /* A = NaN? quiet A */
    a->frac &= ~UF_SNAN;
    return UFT_NAN;
    }
if (ftpb == UFT_NAN) {                                  /* B = NaN? quiet B */
    fp_upset (a, b->sign, b->exp, b->frac & ~UF_SNAN);
    return UFT_NAN;
    }
a->sign = a->sign ^ b->sign;                            /* sign of result */
if ((ftpa == UFT_ZERO) || (ftpb == UFT_ZERO)) {         /* A or B zero? */
    if ((ftpa == UFT_INF) || (ftpb == UFT_INF)) {       /* 0 * inf? */
        if (fp_trap (ctx, FPCR_INV)) return UFT_TRAP;   /* inv op trap */ 
        fp_upset (a, 0, UF_CQN_E, UF_CQN_F);            /* no trap? CQNaN */
        return UFT_NAN;
        }
    else fp_upset (a, a->sign, 0, 0);                   /* else signed 0 */
    return UFT_ZERO;
    }
if ((ftpa == UFT_INF) || (ftpb == UFT_INF)) {           /* A or B inf? */
    fp_upset (a, a->sign, UF_INF_E, UF_INF_F);          /* signed inf */
    return UFT_INF;
    }
a->exp = a->exp + b->exp + 1 - D_BIAS;                  /* add exponents */
resl = uemul64 (a->frac, b->frac, &a->frac);            /* multiply fracs */
fp_norm (a);                                            /* normalize */
a->frac = a->frac | (resl? 1: 0);                       /* sticky bit */
return UFT_FIN;
}

/* Floating divide */

void fp_div (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 ft, uint32 fd)
{
UFP a, b;
uint32 ftpa, ftpb, ftpr;

if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack A; trap? */
    return;
if (!(ftpb = fp_unpack (ctx, fmt, ft, &b, 0)))          /* unpack B; trap */
    return;
if ((ftpr = fp_updiv (ctx, ftpa, &a, ftpb, &b)) &&      /* divide ok? */
     fp_rtest (ctx, fmt, ftpr, &a, CRR))                /* round ok? */
    fp_pack (ctx, fmt, fd, &a);                         /* yes, pack and store */
return;
}

/* Floating reciprocal - treated like 1/rs */

void fp_recip (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a, b;
uint32 ftpb, ftpr;

if (!(ftpb = fp_unpack (ctx, fmt, fs, &b, 0)))          /* unpack B */
    return;
a = ufp_one;                                            /* init A = 1 */
if ((ftpr = fp_updiv (ctx, UFT_FIN, &a, ftpb, &b)) &&   /* divide ok? */
     fp_rtest (ctx, fmt, ftpr, &a, CRR))                /* round ok? */
    fp_pack (ctx, fmt, fd, &a);                         /* yes, pack it up */
return;
}

/* Unpacked floating divide routine - used by fdiv, frecip, frsqrt

   - Take care of NaNs and infinites
   - Check for zero cases
   - Divide fractions (55b to develop a rounding bit)
   - Set sticky bit if remainder non-zero
   
   Because IEEE fractions have a range of [1,2), the result can have a range
   of (.5,2).  Results in the range of [1,2) are correct.  Results in the
   range of (.5,1) need to be normalized by one place. */

uint32 fp_updiv (CORECTX *ctx, uint32 ftpa, UFP *a, uint32 ftpb, UFP *b)
{
uint32 sticky;

if (ftpa == UFT_NAN) {                                  /* A = NaN? quiet A */
    a->frac &= ~UF_SNAN;
    return UFT_NAN;
    }
if (ftpb == UFT_NAN) {                                  /* B = NaN? quiet B */
    fp_upset (a, b->sign, b->exp, b->frac & ~UF_SNAN);
    return UFT_NAN;
    }
a->sign = a->sign ^ b->sign;                            /* sign of result */
if (ftpb == UFT_INF) {                                  /* B = inf? */
    if (ftpa == UFT_INF) {                              /* inf/inf? */
        if (fp_trap (ctx, FPCR_INV)) return UFT_TRAP;   /* inv op; trap */
        fp_upset (a, 0, UF_CQN_E, UF_CQN_F);            /* no trap, CQNaN */
        return UFT_NAN;
        }
    fp_upset (a, a->sign, 0, 0);                        /* !inf/inf, signed 0 */
    return UFT_ZERO;
    }
if (ftpa == UFT_INF) return UFT_INF;                    /* A = inf? */
if (ftpb == UFT_ZERO) {                                 /* B = 0? */
    if (ftpa == UFT_ZERO) {                             /* 0/0? */
        if (fp_trap (ctx, FPCR_INV)) return UFT_TRAP;   /* inv op; trap */
        fp_upset (a, 0, UF_CQN_E, UF_CQN_F);            /* no trap, CQNaN */
        return UFT_NAN;
        }
    else {                                              /* fin/0 */
        if (fp_trap (ctx, FPCR_DZE)) return UFT_TRAP;   /* div by 0; trap? */
        fp_upset (a, a->sign, UF_INF_E, UF_INF_F);      /* no trap, signed inf */
        return UFT_INF;
        }
    }
if (ftpa == UFT_ZERO) return UFT_ZERO;                  /* A = 0? no change */
a->exp = a->exp - b->exp + D_BIAS;                      /* unbiased exp */
a->frac = a->frac >> 1;                                 /* allow 1 bit left */
b->frac = b->frac >> 1;
a->frac = udiv64 (a->frac, b->frac, 55, &sticky);       /* divide */
fp_norm (a);                                            /* normalize */
a->frac = a->frac | sticky;                             /* insert sticky */
return UFT_FIN;
}

/* Coprocessor 1X instructions - ALNV.PS */

t_stat op_cop1x_alnv (CORECTX *ctx, uint32 ir)
{
uint32 rs, fs, ft, fd;
t_uint64 t64, r_fs, r_ft;

fs = I_GETFS (ir);                                      /* extract reg */
ft = I_GETFT (ir);
fd = I_GETFD (ir);
rs = I_GETRS (ir);
r_fs = fp_getdpr (ctx, fs);
r_ft = fp_getdpr (ctx, ft);
if ((ctx->R[rs] & 0x7) == 0)                            /* test alignment */
    t64 = r_fs;
else t64 = ((r_ft << 32) & M64) |
    (r_fs >> 32);
fp_putdpr (ctx, fd, t64);
return SCPE_OK;
}

/* Coprocessor 1X instructions (multiply-add class) - fnc >= 0x20 */

t_stat op_cop1x (CORECTX *ctx, uint32 ir)
{
uint32 fnc, fs, fr, ft, fd, sv_fpcr;
t_uint64 sv_fd;
#if defined (_MIPS_PS_)
static uint8 cop1x_valid[32] = {
    1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0,
    1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0
    };
#else
static uint8 cop1x_valid[32] = {
    1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0
    };
#endif

fnc = I_GETFNC (ir) & 0x1F;                             /* mask function */
if (!cop1x_valid[fnc]) {                                /* valid? */
    ctx->traps |= TRAP_RSVI;
    return SCPE_OK;
    }
fs = I_GETFS (ir);                                      /* extract reg */
ft = I_GETFT (ir);
fd = I_GETFD (ir);
fr = I_GETFR (ir);
set_fpcr (ctx->fpcr & ~FPCR_CAUSE);                     /* clear causes */
sv_fpcr = ctx->fpcr;
sv_fd = ctx->F[fd];
fp_muladd (ctx, fnc, fs, ft, fr, fd);
if ((fnc & 0x7) == FP_PS) {                             /* PS? */
    fp_muladd (ctx, fnc|1, fs, ft, fr, fd);             /* do upper */
    if (ctx->traps & TRAP_FPE)                          /* trap? */
        fp_rest_ps (ctx, fd, sv_fd, sv_fpcr);           /* restore state */
    }
return SCPE_OK;
}

/* Multiply and add/subtract

   FS           FO              traps and actions

   0            0               unimplemented
   x            1               continue using full intermediate result
   1            0               underflow, inexact, flush to zero */

void fp_muladd (CORECTX *ctx, uint32 fnc, uint32 fs, uint32 ft, uint32 fr, uint32 fd)
{
UFP a, b, c;
uint32 ftpa, ftpb, ftpc, ftpr;
uint32 rndm = CRR;
uint32 fmt, inex;

fmt = fnc & 0x07;
if (!(ftpa = fp_unpack (ctx, fmt, fs, &a, 0)))          /* unpack A; trap? */
    return;
if (!(ftpb = fp_unpack (ctx, fmt, ft, &b, 0)))          /* unpack B; trap? */
    return;
if (!(ftpr = fp_upmul (ctx, ftpa, &a, ftpb, &b)))       /* A * B; trap? */
    return;
if (ftpr == UFT_FIN) {                                  /* finite, non-zero? */
    inex = fp_round (fmt, &a, rndm);                    /* round */
    if (!fp_ovftest (ctx, fmt, &a, rndm))               /* overflow and trap? */
        return;
    if ((a.exp <= 0) && !(ctx->fpcr & FPCR_FO)) {       /* underflow & ~FO? */
        if (ctx->fpcr & FPCR_FS) {                      /* FS? */
            if (fp_trap (ctx, FPCR_UNF|FPCR_INE))       /* unfl, inex; trap? */
                return;
            if (((rndm == FPCR_RP) && !a.sign) ||       /* RP and + or RM and -? */
                ((rndm == FPCR_RM) && a.sign))          /* signed min */
                fp_upset (&a, a.sign, (fmt == FP_D)? UF_MIN_E_D: UF_MIN_E_S, UF_MIN_F);
            else fp_upset (&a, a.sign, 0, 0);           /* else signed 0 */
            }
        else {                                          /* ~FS */
            fp_trap (ctx, FPCR_UNI);                    /* unimplemented */
            return;                                     /* must trap */
            }
        }                                               /* end if unfl */
    else if (inex && fp_trap (ctx, FPCR_INE))           /* inexact and trap? */
        return;
    }                                                   /* end if finite */
if (!(ftpc = fp_unpack (ctx, fmt, fr, &c, 0)))          /* unpack C; trap? */
    return;
if (fnc & 0x08) c.sign = c.sign ^ 1;                    /* subtract? */
if ((ftpr = fp_upadd (ctx, ftpr, &a, ftpc, &c)) &&      /* add ok? */
     fp_rtest (ctx, fmt, ftpr, &a, rndm)) {             /* round ok? */
    if (fnc & 0x10) a.sign = a.sign ^ 1;                /* NMADD/NMSUB? */
    fp_pack (ctx, fmt, fd, &a);                         /* yes, pack it up */
    }
return;
}

/* Floating square root

   - Take care of NaNs, +infinite, zero
   - Compute sqrt */

void fp_sqrt (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
uint32 ftpb, ftpr;
UFP b;

if (!(ftpb = fp_unpack (ctx, fmt, fs, &b, 0)))          /* unpack B */
    return;
if (ftpb == UFT_NAN) {                                  /* B = NaN? quiet B */
    b.frac &= ~UF_SNAN;
    fp_pack (ctx, fmt, fd, &b);
    return;
    }
if ((ftpb == UFT_ZERO) ||                               /* zero? */
    ((ftpb == UFT_INF) && !b.sign)) {                   /* +infinity? */
    fp_pack (ctx, fmt, fd, &b);                         /* pack it up */
    return;
    }
if ((ftpr = fp_upsqrt (ctx, ftpb, &b)) &&               /* sqrt ok? */
     fp_rtest (ctx, fmt, ftpr, &b, CRR))                /* round ok? */
    fp_pack (ctx, fmt, fd, &b);                         /* yes, pack it up */
return;
}

/* Reciprocal square root

   - Take care of NaNs, +infinite, zero
   - Compute sqrt
   - Divide into 1 */

void fp_rsqrt (CORECTX *ctx, uint32 fmt, uint32 fs, uint32 fd)
{
UFP a, b;
uint32 ftpb, ftpr;

if (!(ftpb = fp_unpack (ctx, fmt, fs, &b, 0)))          /* unpack B */
    return;
if (ftpb == UFT_NAN) {                                  /* B = NaN? quiet B */
    b.frac &= ~UF_SNAN;
    fp_pack (ctx, fmt, fd, &b);
    return;
    }
if (ftpb == UFT_ZERO) {                                 /* 1/sqrt (0)? */
    if (fp_trap (ctx, FPCR_DZE)) return;                /* divide by 0 trap? */
    fp_upset (&b, b.sign, UF_INF_E, UF_INF_F);
    fp_pack (ctx, fmt, fd, &b);
    return;
    }
if ((ftpb = fp_upsqrt (ctx, ftpb, &b))) {               /* sqrt ok? */
    a = ufp_one;                                        /* 1/sqrt (b) */
    if ((ftpr = fp_updiv (ctx, UFT_FIN, &a, ftpb, &b)) && /* divide ok? */
         fp_rtest (ctx, fmt, ftpr, &a, CRR))            /* round ok? */
        fp_pack (ctx, fmt, fd, &a);                     /* yes, pack it up */
    }
return;
}

/* Square root routine - code from SoftFloat */

uint32 fp_upsqrt (CORECTX *ctx, uint32 ftpb, UFP *b)
{
t_uint64 asig = b->frac;
int32 exp = b->exp;
t_uint64 zsig, remh, reml, t;
uint32 sticky = 0;

if (!Q_FINITE (ftpb)) return ftpb;                      /* dont change inf etc */
if (b->sign) {                                          /* minus? */
    if (fp_trap (ctx, FPCR_INV)) return UFT_TRAP;       /* signal inv op */
    fp_upset (b, 0, UF_CQN_E, UF_CQN_F);                /* return CQNaN */
    return UFT_NAN;
    }

zsig = estimateSqrt32 (exp, (uint32) (asig >> 32));

/* Calculate the final answer in two steps.  First, do one iteration of
   Newton's approximation.  The divide-by-2 is accomplished by clever
   positioning of the operands.  Then, check the bits just below the
   (double precision) rounding bit to see if they are close to zero
   (that is, the rounding bits are close to midpoint).  If so, make
   sure that the result^2 is <below> the input operand */

asig = asig >> ((exp & 1)? 3: 2);                       /* leave 2b guard */
zsig = estimateDiv128 (asig, 0, zsig << 32) + (zsig << 30 );
if ((zsig & 0x1FF) <= 5) {                              /* close to even? */
    reml = uemul64 (zsig, zsig, &remh);                 /* result^2 */
    remh = asig - remh - (reml? 1:0);                   /* arg - result^2 */
    reml = NEG_D (reml);
    while (D_GETSIGN (remh) != 0) {                     /* if arg < result^2 */
        zsig = zsig - 1;                                /* decr result */
        t = (zsig << 1) | 1;                            /* incr result^2 */
        reml = reml + t;                                /* and retest */
        remh = remh + (zsig >> 63) + ((reml < t)? 1: 0);
        }
    if ((remh | reml) != 0 ) sticky = 1;                /* not exact? */
    }
b->frac = (zsig << 1) | sticky;                         /* left justify result */
b->exp = ((exp - D_BIAS) >> 1) + D_BIAS;                /* result exponent */
return UFT_FIN;
}

/* Estimate 32b SQRT - code from SoftFloat

   Calculate an approximation to the square root of the 32-bit significand given
   by 'a'.  Considered as an integer, 'a' must be at least 2^31.  If bit 0 of
   'exp' (the least significant bit) is 1, the integer returned approximates
   2^31*sqrt('a'/2^31), where 'a' is considered an integer.  If bit 0 of 'exp'
   is 0, the integer returned approximates 2^31*sqrt('a'/2^30).  In either
   case, the approximation returned lies strictly within +/-2 of the exact
   value. */

uint32 estimateSqrt32 (uint32 exp, uint32 a)
{
uint32 index, z;
static const uint32 sqrtOdd[] = {
    0x0004, 0x0022, 0x005D, 0x00B1, 0x011D, 0x019F, 0x0236, 0x02E0,
    0x039C, 0x0468, 0x0545, 0x0631, 0x072B, 0x0832, 0x0946, 0x0A67 };
static const uint32 sqrtEven[] = {
    0x0A2D, 0x08AF, 0x075A, 0x0629, 0x051A, 0x0429, 0x0356, 0x029E,
    0x0200, 0x0179, 0x0109, 0x00AF, 0x0068, 0x0034, 0x0012, 0x0002 };


index = (a >> 27) & 0xF;                                /* bits<30:27> */
if (exp & 1) {                                          /* odd exp? */
    z = 0x4000 + (a >> 17) - sqrtOdd[index];            /* initial guess */
    z = ((a / z) << 14) + (z << 15);                    /* Newton iteration */
    a = a >> 1;
    }
else {
    z = 0x8000 + (a >> 17) - sqrtEven[index];           /* initial guess */
    z = (a / z) + z;                                    /* Newton iteration */
    z = (z >= 0x20000) ? 0xFFFF8000: (z << 15);
    if (z <= a) z = (a >> 1) | 0x80000000;
    }
return (uint32) ((((((t_uint64) a) << 31) / ((t_uint64) z)) + (z >> 1)) & M32);
}

/* Estimate 128b unsigned divide - code from SoftFloat */

t_uint64 estimateDiv128 (t_uint64 a0, t_uint64 a1, t_uint64 b)
{
t_uint64 b0, b1;
t_uint64 rem0, rem1, term0, term1;
t_uint64 z;

if (b <= a0) return SIM_ULL(0xFFFFFFFFFFFFFFFF);
b0 = b >> 32;
z = ((b0 << 32) <= a0)? SIM_ULL(0xFFFFFFFF00000000): ((a0 / b0) << 32);
term1 = uemul64 (b, z, &term0);
rem0 = a0 - term0 - (a1 < term1);
rem1 = a1 - term1;
while (D_GETSIGN (rem0)) {
    z = z - SIM_ULL(0x100000000);
    b1 = b << 32;
    rem1 = b1 + rem1;
    rem0 = b0 + rem0 + (rem1 < b1);
    }
rem0 = (rem0 << 32) | (rem1 >> 32);
z |= (((b0 << 32) <= rem0)? 0xFFFFFFFF: (rem0 / b0));
return z;
}

/* Support routines */

uint32 fp_unpack (CORECTX *ctx, uint32 fmt, uint32 rn, UFP *r, uint32 qdnm)
{
t_uint64 t64;

if (fmt == FP_D) {                                      /* double prec? */
    t64 = fp_getdpr (ctx, rn);                          /* get reg */
    r->sign = D_GETSIGN (t64);                          /* unpack */
    r->exp = D_GETEXP (t64);
    r->frac = D_GETFRAC (t64);
    }
else {                                                  /* single prec */
    if (fmt == FP_PSU)
        t64 = fp_getdpr (ctx, rn) >> 32;
    else if (fmt == FP_PS)
        t64 = fp_getdpr (ctx, rn) & SIM_ULL(0x00000000FFFFFFFF);
    else t64 = fp_getspr (ctx, rn);                     /* get reg */
    r->sign = S_GETSIGN (t64);                          /* unpack */
    r->exp = S_GETEXP (t64);
    r->frac = S_GETFRAC (t64) << (32 - 3);
    }
if (r->exp == 0) {                                      /* exponent = 0? */
    if (r->frac == 0) return UFT_ZERO;                  /* frac = 0? then true 0 */
    if (ctx->fpcr & FPCR_FS) {                          /* squash dnorms? */
        if (fp_trap (ctx, FPCR_INE))                    /* inexact trap? */
            return UFT_TRAP;
        r->frac = 0;                                    /* clear fraction */
        return UFT_ZERO;
        }
    if (qdnm)                                           /* allow denorms? */
        return UFT_DENORM;
    fp_trap (ctx, FPCR_UNI);                            /* unimplemented */
    return UFT_TRAP;
    }
else if (fmt != FP_D) {                                 /* S, PS format? */
    if (r->exp == S_M_EXP)                              /* NaN or inf? */
        r->exp = D_M_EXP;
    else r->exp = r->exp - S_BIAS + D_BIAS;             /* dprec format */
    }
if (r->exp == UF_INF_E) {                               /* exponent = max? */
    if (r->frac == 0)                                   /* frac = 0? then inf */
        return UFT_INF;
    r->frac = (r->frac << FP_GUARD) | UF_NM;            /* ins hidden bit, guard */
    if ((r->frac & UF_SNAN) &&                          /* signaling NAN and */
        fp_trap (ctx, FPCR_INV)) return UFT_TRAP;       /* trap? trap */
    return UFT_NAN;                                     /* no, NaN */
    }
r->frac = (r->frac << FP_GUARD) | UF_NM;                /* ins hidden bit, guard */
return UFT_FIN;                                         /* finite */
}

/* Normalize - input must be zero, finite, or denorm */

void fp_norm (UFP *r)
{
int32 i;
static t_uint64 normmask[5] = {
    SIM_ULL(0xc000000000000000), SIM_ULL(0xf000000000000000),
    SIM_ULL(0xff00000000000000),
    SIM_ULL(0xffff000000000000), SIM_ULL(0xffffffff00000000)
    };
static int32 normtab[6] = { 1, 2, 4, 8, 16, 32 };

r->frac = r->frac & M64;
if (r->frac == 0) {                                     /* if fraction = 0 */
    r->sign = 0;
    r->exp = 0;                                         /* result is plus 0 */
    return;
    }
while ((r->frac & UF_NM) == 0) {                        /* normalized? */
    for (i = 0; i < 5; i++) {                           /* find first 1 */
        if (r->frac & normmask[i]) break;
        }
    r->frac = r->frac << normtab[i];                    /* shift frac */
    r->exp = r->exp - normtab[i];                       /* decr exp */
    }
return;
}

/* Round and test a finite result - much of the IEEE fiddling is buried here */

/* Round - round and truncate an unpacked fp number according to the rounding mode
   Returns TRUE if any bits lost (inexact result) */

INLINE t_bool fp_round (uint32 fmt, UFP *r, uint32 rndm)
{
static const t_uint64 stdrnd[2] = { UF_SRND, UF_DRND };
static const t_uint64 infrnd[2] = { UF_SINF, UF_DINF };
t_uint64 rndbits, rndadd;
uint32 dp = (fmt == FP_D)? 1: 0;

rndbits = r->frac & infrnd[dp];                         /* isolate round bits */
if (rndm == FPCR_RN)                                    /* round to nearest? */
    rndadd = stdrnd[dp];
else if (((rndm == FPCR_RP) && !r->sign) ||             /* round to inf and */
    ((rndm == FPCR_RM) && r->sign))                     /* right sign? */
    rndadd = infrnd[dp];
else rndadd = 0;
r->frac = r->frac + rndadd;                             /* round */
if ((r->frac & UF_NM) == 0) {                           /* carry out? */
    r->frac = (r->frac >> 1) | UF_NM;                   /* renormalize */
    r->exp = r->exp + 1;
    }
r->frac = r->frac & ~infrnd[dp];                        /* clear extra bits */
if ((rndm == FPCR_RN) && (rndbits == stdrnd[dp]))       /* nearest and halfway? */
    r->frac &= ~(stdrnd[dp] << 1);                      /* clear low bit */
return (rndbits != 0);                                  /* TRUE if bits lost */
}

/* Overflow test - test for overflow, trap if enabled, return default value if not */

INLINE uint32 fp_ovftest (CORECTX *ctx, uint32 fmt, UFP *r, uint32 rndm)
{
static const int32 expmax[2] = { D_BIAS - S_BIAS + S_M_EXP - 1, D_M_EXP - 1 };
uint32 dp = (fmt == FP_D)? 1: 0;

if (r->exp > expmax[dp]) {                              /* ovflo? */
    if (fp_trap (ctx, FPCR_OVF|FPCR_INE))               /* set trap; done? */
        return UFT_TRAP;
    if ((rndm == FPCR_RN) ||                            /* RN, RP & +, RM & -? */
        ((rndm == FPCR_RP) && !r->sign) ||
        ((rndm == FPCR_RM) && r->sign)) {
        fp_upset (r, r->sign, UF_INF_E, UF_INF_F);      /* store signed inf */
        return UFT_INF;
        }
    fp_upset (r, r->sign, dp? UF_MAX_E_D: UF_MAX_E_S, UF_MAX_F); /* store signed max */
    }
return UFT_FIN;
}

/* Underflow test - test for underflow, trap if enabled, return default if not

   FS           FN              results and actions

   0            0               unimplemented
   1            0               underflow, inexact, flush to zero
   x            1               underflow, inexact, flush to nearest */

INLINE uint32 fp_unftest (CORECTX *ctx, uint32 fmt, UFP *r, uint32 rndm)
{
uint32 dp = (fmt == FP_D)? 1: 0;

if (r->exp < (dp? UF_MIN_E_D: UF_MIN_E_S)) {            /* underflow? */
    if (fp_trap (ctx,                                   /* trap on */
        ((ctx->fpcr & (FPCR_FS|FPCR_FN))?               /* FS/FN? unfl, inex */
        (FPCR_UNF|FPCR_INE): FPCR_UNI)))                /* otherwise, unimpl */
        return UFT_TRAP;                                /* trap? */
    if (((ctx->fpcr & FPCR_FN) &&                       /* FN override */
         (rndm == FPCR_RN) &&                           /* in RN mode */
         (r->exp == 0)) ||                              /* and halfway? */
        (((rndm == FPCR_RP) && !r->sign) ||             /* RP and + or RM and -? */
         ((rndm == FPCR_RM) && r->sign)))
        fp_upset (r, r-> sign, dp? UF_MIN_E_D: UF_MIN_E_S, UF_MIN_F); /* store signed min */
    else fp_upset (r, r->sign, 0, 0);                   /* else signed 0 */
    }
return UFT_FIN;
}

/* Pack up a floating point result and store in register */

void fp_pack (CORECTX *ctx, uint32 fmt, uint32 rn, UFP *r)
{
t_uint64 t64;
uint32 t32;

if (fmt == FP_D) {                                      /* dp? */
    t64 = (((t_uint64) r->sign) << D_V_SIGN) |          /* form result */
        (((t_uint64) (r->exp & D_M_EXP)) << D_V_EXP) |
        ((r->frac >> FP_GUARD) & D_FRAC);
    fp_putdpr (ctx, rn, t64);
    }
else {
    if (r->exp == D_M_EXP)                              /* NaN/inf? change exp */
        r->exp = S_M_EXP;
    else if (r->exp)
        r->exp = r->exp - D_BIAS + S_BIAS;              /* nz? change bias */
    t32 = (r->sign << S_V_SIGN) |                       /* form result */
        ((r->exp & S_M_EXP) << S_V_EXP) |
        ((uint32) ((r->frac >> (32 - 3 + FP_GUARD)) & S_FRAC));
    if (fmt == FP_PS) {
        t64 = fp_getdpr (ctx, rn);
        t64 = (t64 & SIM_ULL(0xFFFFFFFF00000000)) |
              ((t_uint64) t32);
        fp_putdpr (ctx, rn, t64);
        }
    else if (fmt == FP_PSU) {
        t64 = fp_getdpr (ctx, rn);
        t64 = (t64 & SIM_ULL(0x00000000FFFFFFFF)) |
              (((t_uint64) t32) << 32);
        fp_putdpr (ctx, rn, t64);
        }
    else fp_putspr (ctx, rn, t32);
    }
return;
}

/* Round and test - master routine for most operations */

uint32 fp_rtest (CORECTX *ctx, uint32 fmt, uint32 ftpr, UFP *r, uint32 rndm)
{
t_bool inex;

if (!Q_FINITE (ftpr))                                   /* finites only */
    return ftpr;
if (r->frac == 0)                                       /* result = 0? */
    return UFT_FIN;
inex = fp_round (fmt, r, rndm);                         /* round */
if (!(ftpr = fp_ovftest (ctx, fmt, r, rndm)))           /* ovf test; trap? */
    return UFT_TRAP;
if (!(ftpr = fp_unftest (ctx, fmt, r, rndm)))           /* unf test; trap? */
    return UFT_TRAP;
if (inex && fp_trap (ctx, FPCR_INE))                    /* inexact and trap? */
    return UFT_TRAP;
return ftpr;
}

/* Arithmetic trap - multiple traps can be set simultaneously */

t_bool fp_trap (CORECTX *ctx, uint32 flags)
{
uint32 enbls;

set_fpcr (ctx->fpcr | (flags << FPCR_V_CAUSE));         /* set causes */
enbls = FPCR_GETENB (ctx->fpcr) | FPCR_UNI;             /* get enables, force unimpl */
if (flags & enbls) {                                    /* any enabled traps? */
    ctx->traps |= TRAP_FPE;                             /* fp trap */
    return TRUE;                                        /* abort instruction */
    }
set_fpcr (ctx->fpcr | ((flags & ~FPCR_UNI) << FPCR_V_FLAGS)); /* no trap, set flags */
return FALSE;
}

/* 64b / 64b unsigned fraction divide */

t_uint64 udiv64 (t_uint64 dvd, t_uint64 dvr, uint32 prec, uint32 *sticky)
{
t_uint64 quo;
uint32 i;

quo = 0;                                                /* clear quotient */
for (i = 0; (i < prec) && dvd; i++) {                   /* divide loop */
    quo = quo << 1;                                     /* shift quo */
    if (dvd >= dvr) {                                   /* div step ok? */
        dvd = dvd - dvr;                                /* subtract */
        quo = quo + 1;                                  /* quo bit = 1 */
        }
    dvd = dvd << 1;                                     /* shift divd */
    }
quo = quo << (UF_V_NM - i + 1);                         /* shift quo */
if (sticky) *sticky = (dvd? 1: 0);                      /* set sticky bit */
return quo;                                             /* return quotient */
}

/* Test fp cc - input is cc#'x'F/T */

t_bool fp_testcc (CORECTX *ctx, uint32 ccs)
{
uint32 ccv;

ccv = ctx->fpcr & fp_ccmap[ccs >> 2];                   /* get CC value */
return ((ccv == 0) ^ (ccs & 1));
}

/* Set fp cc - input is cc#'x'x */

void fp_setcc (CORECTX *ctx, uint32 ccs, uint32 val)
{
if (val) set_fpcr (ctx->fpcr | fp_ccmap[ccs >> 2]);
else set_fpcr (ctx->fpcr & ~fp_ccmap[ccs >> 2]);
return;
}

/* Register access routines */

t_uint64 fp_getspr (CORECTX *ctx, uint32 rn)
{
if (get_cp0_sr() & CP0_SR_FR)                            /* 64b? */
    return (fpr(rn) & M32);                          /* return low 32b */
else if (rn & 1)                                        /* 32b and odd? */
    return ((fpr(rn & ~1) >> 32) & M32);             /* high 32b of even reg */
else return (fpr(rn) & M32);                         /* 32b even, ret low 32b */
}

void fp_putspr (CORECTX *ctx, uint32 rn, t_uint64 val)
{
t_uint64 v32 = val & M32;

if (get_cp0_sr() & CP0_SR_FR)                            /* 64b? */
    setfpr(rn, v32);                                    /* store low 32b */
else if (rn & 1)                                        /* 32b and odd? */
    setfpr(rn & ~1, (fpr(rn & ~1) & ((t_uint64) M32)) |
        (v32 << 32));                                   /* mod high 32b of even */
else setfpr(rn, (fpr(rn) & ~((t_uint64) M32)) | v32);   /* 32b even, mod low 32b */
return;
}

t_uint64 fp_getdpr (CORECTX *ctx, uint32 rn)
{
if (get_cp0_sr() & CP0_SR_FR) return fpr(rn);            /* 64b? */
else if (rn & 1) {                                      /* 32b and odd? */
    fprintf (stderr,"%%Error: Odd double precision FP register, FR = 0, rn = %d\n", rn);
    fflush (stderr);
    return 0;
    }
else return fpr(rn);
}

void fp_putdpr (CORECTX *ctx, uint32 rn, t_uint64 val)
{
if (get_cp0_sr() & CP0_SR_FR) setfpr(rn, val);
else if (rn & 1) {                                      /* 32b and odd? */
    fprintf (stderr,"%%Error: Odd double precision FP register, FR = 0, rn = %d\n", rn);
    fflush (stderr);
    return;
    }
else setfpr(rn, val);
return;
}

INLINE void fp_upset (UFP *a, uint32 s, uint32 e, t_uint64 f)
{
a->sign = s;
a->exp = e;
a->frac = f;
return;
}

/* Restore state after paired single trap */

void fp_rest_ps (CORECTX *ctx, uint32 fd, t_uint64 sv_fd, uint32 sv_fpcr)
{
fp_putdpr (ctx, fd, sv_fd);                             /* restore fd */
set_fpcr ((ctx->fpcr & ~(FPCR_CC|FPCR_FLAGS)) |
          (sv_fpcr & (FPCR_CC|FPCR_FLAGS)));
return;
}

/* Read control registers */

t_uint64 fp_getcr (CORECTX *ctx, uint32 rn)
{
switch (rn) {

    case CP1_FIR:
        return FIR_VAL;

    case CP1_FCCR:
        return FPCR_GETCC (ctx->fpcr);

    case CP1_FEXR:
        return (ctx->fpcr & (FPCR_FLAGS|FPCR_CAUSE));

    case CP1_FENR:
        return ((ctx->fpcr & (FPCR_ENBLS|FPCR_RNDM)) |
            ((ctx->fpcr & FPCR_FS)? FENR_FS: 0));

    case CP1_FPCR:
        return (ctx->fpcr & FPCR_RW);
        }

return 0;
}

/* Write control registers */

void fp_putcr (CORECTX *ctx, uint32 rn, t_uint64 v64)
{
uint32 val = (uint32) v64;
uint32 enbls, causes;

switch (rn) {

    case CP1_FCCR:
        set_fpcr (ctx->fpcr & ~(FPCR_CC1|FPCR_CC0));
        if (val & 1) set_fpcr (ctx->fpcr | FPCR_CC0);
        set_fpcr (ctx->fpcr | ((val << (FPCR_V_CC1 - 1)) & FPCR_CC1));
        return;

    case CP1_FEXR:
        set_fpcr (ctx->fpcr & ~(FPCR_FLAGS|FPCR_CAUSE));
        set_fpcr (ctx->fpcr | (val & (FPCR_FLAGS|FPCR_CAUSE)));
        break;

    case CP1_FENR:
        set_fpcr (ctx->fpcr & ~(FPCR_ENBLS|FPCR_RNDM|FPCR_FS));
        set_fpcr (ctx->fpcr | (val & (FPCR_ENBLS|FPCR_RNDM)));
        if (val & FENR_FS) set_fpcr (ctx->fpcr | FPCR_FS);
        break;

    case CP1_FPCR:
        set_fpcr (val & FPCR_RW);
        break;

    default:
        return;
        }

enbls = FPCR_GETENB (ctx->fpcr) | FPCR_UNI;             /* get enables, force unimpl */
causes = FPCR_GETCAU (ctx->fpcr);                       /* get causes */
if (enbls & causes) ctx->traps |= TRAP_FPE;             /* enabled traps? */
return;
}
