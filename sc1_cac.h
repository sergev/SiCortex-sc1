/* sc1_cac.h - SC1 cache controller definitions

   Copyright (c) 2006, SiCortex, Inc.  All rights reserved.

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
*/

#ifndef _SC1_CAC_H_
#define _SC1_CAC_H_     0

/* L2 cache controller and interrupt definitions */

#define PA_CAC_BASE     SIM_ULL(0xE9E000000)            /* base */
#define PA_CAC_LNT      0x2000

#define CAC_ICR_OFF     (0x0000 >> 3)                   /* interrupt cause */
#define CAC_ICR_NUM     (2 * INT_N_HLVLS)
#define CAC_ICR_ACT     0x200                           /* interrupt active */
#define CAC_ICR_OVF     0x100                           /* overflow */
#define CAC_ICR_CAUSE   0x0FF                           /* cause */
#define CAC_ICR_RD      0x3FF
#define CAC_ICR_W1C     0x200

#define CAC_SLSEL_OFF   (0x00C8 >> 3)                   /* slow interrupt select */
#define CAC_SLSEL_RW    0x1FFF

#define CAC_SLSTA_OFF   (0x00D0 >> 3)                   /* slow interrupt status */

#define CAC_ECCMODE_OFF (0x0100 >> 3)                   /* L2/CSW ECC mode */
#define CAC_ECCMODE_RW  0x3F
#define CAC_ECCMODE_L2TE    0x20                        /* L2 tag ECC enable */
#define CAC_ECCMODE_CSWE    0x08                        /* CSW ECC enable */
#define CAC_ECCMODE_L2DE    0x02                        /* L2 data ECC enable */

#define CAC_ECCSTAT_OFF (0x0110 >> 3)                   /* L2/CSW ECC status */
#define CAC_ECCSTAT_R   0x1FF
#define CAC_ECCSTAT_W1C 0x1FF
#define CAC_ECCSTAT_V_L2T   6
#define CAC_ECCSTAT_V_CSW   3
#define CAC_ECCSTAT_V_L2D   0
#define CAC_ECCSTAT_M_STA   0x3
#define CAC_ECCSTAT_L2TC 0x180
#define CAC_ECCSTAT_L2TU 0x040
#define CAC_ECCSTAT_CSWC 0x030
#define CAC_ECCSTAT_CSWU 0x008
#define CAC_ECCSTAT_L2DC 0x006
#define CAC_ECCSTAT_L2DU 0x001

#define UPDATE_ECC_STAT(c, fld, d) \
                        c->cac_EccStat = \
                        ((c->cac_EccStat & ~(CAC_ECCSTAT_M_STA << CAC_ECCSTAT_V_##fld)) | \
                        (((d) & CAC_ECCSTAT_M_STA) << CAC_ECCSTAT_V_##fld))

#define CAC_L2ECCADDR_OFF   (0x0118 >> 3)               /* L2 ECC error addr */
#define CAC_CSWECCADDR_OFF  (0x0120 >> 3)               /* CSW ECC error addr */
#define CAC_TAGECCADDR_OFF  (0x0128 >> 3)               /* TAG ECC error addr */
#define CAC_ECCADDR_R   SIM_ULL(0xFFFFFFFF8)            /* read only mask */

#define CAC_ECCSYND_OFF (0x0130 >> 3)                   /* ECC syndrome (CSW only) */
#define CAC_ECCSYND_R   0xFFFF

#define CAC_IDR_OFF     (0x1000 >> 3)                   /* interrupt delivery */
#define CAC_IDR_V_BUS   12                              /* bus stop */
#define CAC_IDR_M_BUS   0xF
#define CAC_IDR_V_IDX   8                               /* ICR index */
#define CAC_IDR_M_IDX   0xF
#define CAC_IDR_CAUSE   0xFF
#define CAC_IDR_W       0xFFFF
#define CAC_IDR_GETBUS(x) (((x) >> CAC_IDR_V_BUS) & CAC_IDR_M_BUS)
#define CAC_IDR_GETIDX(x) (((x) >> CAC_IDR_V_IDX) & CAC_IDR_M_IDX)

/* Bus stops */

#define CAC_M_BUSSTOP	0xF

/* Slow interrupts */

enum slow_interrupts {
    SLOW_V_COHO,    SLOW_V_COHE,    SLOW_V_UART,    SLOW_V_FSW,
    SLOW_V_DMA,     SLOW_V_FLNK,    SLOW_V_SCB,     SLOW_V_PMI,
    SLOW_V_OCLA,    SLOW_V_L2C,     SLOW_V_L2U,     SLOW_V_CSWC,
    SLOW_V_CSWU
    };

#define SLOW_COHO       (1u << SLOW_V_COHO)
#define SLOW_COHE       (1u << SLOW_V_COHE)
#define SLOW_UART       (1u << SLOW_V_UART)
#define SLOW_FSW        (1u << SLOW_V_FSW)
#define SLOW_DMA        (1u << SLOW_V_DMA)
#define SLOW_FLNK       (1u << SLOW_V_FLNK)
#define SLOW_SCB        (1u << SLOW_V_SCB)
#define SLOW_PMI        (1u << SLOW_V_PMI)
#define SLOW_OCLA       (1u << SLOW_V_OCLA)
#define SLOW_L2C        (1u << SLOW_V_L2C)
#define SLOW_L2U        (1u << SLOW_V_L2U)
#define SLOW_CSWC       (1u << SLOW_V_CSWC)
#define SLOW_CSWU       (1u << SLOW_V_CSWU)
#define SLOW_GLOBAL     (SLOW_COHO|SLOW_COHE|SLOW_UART|SLOW_FSW|\
                         SLOW_DMA|SLOW_FLNK|SLOW_SCB|SLOW_PMI|SLOW_OCLA)
#define SLOW_LOCAL      (SLOW_L2C|SLOW_L2U|SLOW_CSWC|SLOW_CSWU)
#define SLOW_ILVL       5                               /* slow intr are IRQ 5 */

/* COHO and COHE coherency engine definitions */

#define PA_COHO_BASE    SIM_ULL(0xE00000000)            /* base */
#define PA_COHO_LNT     0x2000
#define PA_COHE_BASE    SIM_ULL(0xE09000000)            /* base */
#define PA_COHE_LNT     0x2000

#define COH_ECCMODE_OFF (0x0000 >> 3)                   /* ECC mode */
#define COH_ECCMODE_DBL 0x4                             /* report dbl bit */
#define COH_ECCMODE_SGL 0x2                             /* report sgl bit */
#define COH_ECCMODE_COR 0x1                             /* correct */
#define COH_ECCMODE_RW  0x7

#define COH_ECCSTAT_OFF (0x0020 >> 3)                   /* ECC status */
#define COH_ECCSTAT_MUL 0x4
#define COH_ECCSTAT_COR 0x2
#define COH_ECCSTAT_UNC 0x1
#define COH_ECCSTAT_RW1C 0x7

#define COH_ECCADDR_OFF (0x0028 >> 3)                   /* ECC error addr */
#define COH_ECCADDR_R   0x7FFFFFFF

#define COH_ECCSYND_OFF (0x0040 >> 3)
#define COH_ECCSYND_R   0xFF

/* ECC event parameters word */

#define ECCEVT_V_EVT    0                               /* event number */
#define ECCEVT_M_EVT    0xFF
#define ECCEVT_V_SECT   8                               /* section */
#define ECCEVT_M_SECT   0xF
#define ECCEVT_V_TIME   12                              /* cycles in future */
#define ECCEVT_M_TIME   0xFFFFF
#define ECCEVT_V_DATA   32                              /* data */
#define ECCEVT_M_DATA   0xFFFFFFFF

enum ecc_sections {
    ECCS_C0,    ECCS_C1,    ECCS_C2,    ECCS_C3,
    ECCS_C4,    ECCS_C5,    ECCS_COHO,  ECCS_COHE,
    ECCS_PCI,   ECCS_DMA,   ECCS_FSW
    };

/* L1 and L2 event definitions */

#define ECC_CE_L1       0x10                            /* data = error flags */
#define ECC_CE_L2T      0x20                            /* data = address */
#define ECC_CE_CSW      0x40                            /* data = address */
#define ECC_CE_L2D      0x80                            /* data = address */

/* COHO and COHE event definitions */

#define ECC_COH_TAG     0x10                            /* data = address */

/* Function prototypes */

void cac_set_slow (uint32 slow);
void cac_clr_slow (uint32 slow);
t_stat cac_csw_int (uint32 bus, uint32 idx, uint32 val);
t_stat cac_ecc_svc (UNIT *uptr);

#endif
