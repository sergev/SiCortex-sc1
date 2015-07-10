/* sc1_sys.h: SiCortex 1 CPU instruction format definitions

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

   12-Oct-07    RMS     Added support for Mips64 R2 instructions
*/

#ifndef _SC1_SYS_H_
#define _SC1_SYS_H_    0

#include "sim_defs.h"

/* Instruction parse data */

/* Fields */

#define F_Z             0x0                             /* always 0 */
#define F_RS            0x1                             /* rs, also fr */
#define F_FR            (F_RS)
#define F_RT            0x2                             /* rt, also ft, cc */
#define F_FT            (F_RT)
#define F_RD            0x3                             /* rd, also fs */
#define F_FS            (F_RD)
#define F_SA            0x4                             /* sa, also fd, cc */
#define F_FD            (F_SA)
#define F_FNC           0x5                             /* fnc, also sel */
#define F_SEL           (F_FNC)
#define F_JT            0x6                             /* jump target */
#define F_DISP          0x7                             /* disp, also imm */
#define F_CODE          0x8                             /* code */

/* Interpretations

   Note that CC, SEL, CODE, and GC are conditional, on both input and output.
   On input, if the user specifies fewer fields, the conditional field is defaulted.
   On output, if the value is the default value, the field is not printed.
*/

#define FMT_NOPARSE     0x0                             /* no print/parse */
#define FMT_R           0x1                             /* int register */
#define FMT_FR          0x2                             /* flt register */
#define FMT_SA          0x3                             /* 5b lit */
#define FMT_CC          0x4                             /* 3b lit << 2 */
#define FMT_SEL         0x5                             /* 3b lit */
#define FMT_IMM         0x6                             /* 16b lit */
#define FMT_JA          0x7                             /* jump addr */
#define FMT_BA          0x8                             /* branch addr */
#define FMT_MA          0x9                             /* mem addr */
#define FMT_CODE        0xA                             /* 20b lit */
#define FMT_XA          0xB                             /* reg(reg) */  
#define FMT_RC          0xC                             /* cond ret reg */      

#define SYM(x,y)        (((x) << 4) | (y))
#define SYM_GETFLD(x,k) (((x) >> (((k) * 8) + 4)) & 0xF)
#define SYM_GETFMT(x,k) (((x) >> (((k) * 8) + 0)) & 0xF)
#define SYM4(a,b,c,d)   ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

/* Masks and decodes.

   Despite the simplicity of the Mips instruction set, there are many, many
   different decode formats.  This is due to the varying placement of
   register selects in the instruction.

   Every instruction has a maximum of four fields.  Note that the placement and
   size of the field and the interpretation of the field are orthogonal.
*/

#define M_NOP           0xFFFFFFFF
#define F_NOP           0
#define M_MCG           0xFC01003F
#define F_MCG           SYM4 (SYM (F_RD, FMT_R), SYM (F_RS, FMT_R), SYM (F_RT, FMT_CC), 0)
#define M_MCF           0xFFE1003F
#define F_MCF           SYM4 (SYM (F_FD, FMT_FR), SYM (F_FS, FMT_FR), SYM (F_RT, FMT_CC), 0)
#define M_MXC           0xFFE00000
#define F_MXC           SYM4 (SYM (F_RT, FMT_R), SYM (F_RD, FMT_R), SYM (F_SEL, FMT_SEL), 0)
#define M_LFX           0xFC00003F
#define F_LFX           SYM4 (SYM (F_FD, FMT_FR), SYM (F_RS, FMT_XA), 0, 0)
#define M_SFX           0xFC00003F
#define F_SFX           SYM4 (SYM (F_FS, FMT_FR), SYM (F_RS, FMT_XA), 0, 0)
#define M_BRU           0xFFFF0000
#define F_BRU           SYM4 (SYM (F_DISP, FMT_BA), 0, 0, 0)
#define M_BR0           0xFFE00000
#define F_BR0           SYM4 (SYM (F_DISP, FMT_BA), 0, 0, 0)
#define M_BR1           0xFC1F0000
#define F_BR1           SYM4 (SYM (F_RS, FMT_R), SYM (F_DISP, FMT_BA), 0, 0)
#define M_RGI           0xFC1F0000
#define F_RGI           SYM4 (SYM (F_RS, FMT_R), SYM (F_DISP, FMT_IMM), 0, 0)
#define M_SYI           0xFC1F0000
#define F_SYI           SYM4 (SYM (F_DISP, FMT_MA), 0, 0, 0)
#define M_JMP           0xFC000000
#define F_JMP           SYM4 (SYM (F_JT, FMT_JA), 0, 0, 0)
#define M_BR2           0xFC000000
#define F_BR2           SYM4 (SYM (F_RS, FMT_R), SYM (F_RT, FMT_R), SYM (F_DISP, FMT_BA), 0)
#define M_LUI           0xFC000000
#define F_LUI           SYM4 (SYM (F_RT, FMT_R), SYM (F_DISP, FMT_IMM), 0, 0)
#define M_IMM           0xFC000000
#define F_IMM           SYM4 (SYM (F_RT, FMT_R), SYM (F_RS, FMT_R), SYM (F_DISP, FMT_IMM), 0)
#define M_LI            0xFFE00000
#define F_LI            SYM4 (SYM (F_RT, FMT_R), SYM (F_DISP, FMT_IMM), 0, 0)
#define M_MRF           0xFC000000
#define F_MRF           SYM4 (SYM (F_RT, FMT_R), SYM (F_DISP, FMT_MA), 0, 0)
#define M_OR1           0xFC00003F
#define F_OR1           SYM4 (SYM (F_RS, FMT_R), 0, 0, 0)
#define M_OW1           0xFC00003F
#define F_OW1           SYM4 (SYM (F_RD, FMT_R), 0, 0, 0)
#define M_MFM           0xFFC0FFFF
#define F_MFM           SYM4 (SYM (F_RT, FMT_R), 0, 0, 0)
#define M_OR2           0xFC00003F
#define F_OR2           SYM4 (SYM (F_RS, FMT_R), SYM (F_RT, FMT_R), 0, 0)
#define M_OP2           0xFFE0003F
#define F_OP2           SYM4 (SYM (F_RD, FMT_R), SYM (F_RT, FMT_R), 0, 0) 
#define M_OP3           0xFC00003F
#define F_OP3           SYM4 (SYM (F_RD, FMT_R), SYM (F_RS, FMT_R), SYM (F_RT, FMT_R), 0)
#define M_SH3           0xFC20003F
#define F_SH3           SYM4 (SYM (F_RD, FMT_R), SYM (F_RT, FMT_R), SYM (F_SA, FMT_SA), 0)
#define M_SV3           0xFC00007F
#define F_SV3           SYM4 (SYM (F_RD, FMT_R), SYM (F_RT, FMT_R), SYM (F_RS, FMT_R), 0)
#define M_MAC           0xFC00003F
#define F_MAC           SYM4 (SYM (F_FD, FMT_FR), SYM (F_FR, FMT_FR), SYM (F_FS, FMT_FR), SYM (F_FT, FMT_FR))
#define M_SP2           0xFC00003F
#define F_SP2           SYM4 (SYM (F_RD, FMT_R), SYM (F_RT, FMT_NOPARSE), SYM (F_RS, FMT_R), 0)
#define M_COD           0xFC00003F
#define F_COD           SYM4 (SYM (F_CODE, FMT_CODE), 0, 0, 0)
#define M_FP2           0xFFE0003F
#define F_FP2           SYM4 (SYM (F_FD, FMT_FR), SYM (F_FS, FMT_FR), 0, 0)
#define M_FP3           0xFFE0003F
#define F_FP3           SYM4 (SYM (F_FD, FMT_FR), SYM (F_FS, FMT_FR), SYM (F_FT, FMT_FR), 0)
#define M_FCP           0xFFE0003F
#define F_FCP           SYM4 (SYM (F_SA, FMT_CC), SYM (F_FS, FMT_FR), SYM (F_FT, FMT_FR), 0)
#define M_MFI           0xFFE0003F
#define F_MFI           SYM4 (SYM (F_FD, FMT_FR), SYM (F_FS, FMT_FR), SYM (F_RT, FMT_R), 0)
#define M_BCO           0xFFE30000
#define F_BCO           SYM4 (SYM (F_RT, FMT_CC), SYM (F_DISP, FMT_BA), 0, 0)
#define M_MVF           0xFFE00000
#define F_MVF           SYM4 (SYM (F_RT, FMT_R), SYM (F_FS, FMT_FR), 0, 0)
#define M_CJ2           0xFC00003F
#define F_CJ2           SYM4 (SYM (F_RD, FMT_RC), SYM (F_RS, FMT_R), 0, 0)
#define M_FLD           0xFC00003F
#define F_FLD           SYM4 (SYM (F_RT, FMT_R), SYM (F_RS, FMT_R), SYM (F_SA, FMT_SA), SYM (F_RD, FMT_SA))
#define M_ALNV          0xFC00003F
#define F_ALNV          SYM4 (SYM (F_FD, FMT_FR), SYM (F_FS, FMT_FR), SYM (F_FT, FMT_FR), SYM (F_RS, FMT_R))
#define M_POP           0xFC00003F
#define F_POP           SYM4 (SYM (F_RD, FMT_R), SYM (F_RS, FMT_R), 0, 0)

#endif
