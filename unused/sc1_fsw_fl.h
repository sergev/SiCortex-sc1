#ifndef __SC1_FSWFL_H__
#define __SC1_FSWFL_H__

#ifdef SIMX_NATIVE  /* Built into simx */
# include "ice9_addr_spec_sw.h"
# include "ice9_fsw_spec_sw.h"
# include "ice9_fl_spec_sw.h"
# include "ice9_dma_spec_sw.h"
# include "ice9_scb_spec_sw.h"
#else
# include "sicortex/ice9/ice9_addr_spec_sw.h"
# include "sicortex/ice9/ice9_fsw_spec_sw.h"
# include "sicortex/ice9/ice9_fl_spec_sw.h"
# include "sicortex/ice9/ice9_dma_spec_sw.h"
# include "sicortex/ice9/ice9_scb_spec_sw.h"
#endif

/* Fabric Switch and Fabric Link FSW/FL */
#define FSW_BASE              ICE9_RBASEA_Fsw
#define FSW_SIZE             (ICE9_RBASEA_Fsw | ICE9_RBASEAM_Fsw)
#define FSW_END               ICE9_RBASEAE_Fsw

#define FLR0_BASE              ICE9_RBASEA_Flr0
#define FLR0_SIZE             (ICE9_RBASEA_Flr0 | ICE9_RBASEAM_Flr0)
#define FLR0_END               ICE9_RBASEAE_Flr0
#define FLR1_BASE              ICE9_RBASEA_Flr1
#define FLR1_SIZE             (ICE9_RBASEA_Flr1 | ICE9_RBASEAM_Flr1)
#define FLR1_END               ICE9_RBASEAE_Flr1
#define FLR2_BASE              ICE9_RBASEA_Flr2
#define FLR2_SIZE             (ICE9_RBASEA_Flr2 | ICE9_RBASEAM_Flr2)
#define FLR2_END               ICE9_RBASEAE_Flr2

#define FLT0_BASE              ICE9_RBASEA_Flt0
#define FLT0_SIZE             (ICE9_RBASEA_Flt0 | ICE9_RBASEAM_Flt0)
#define FLT0_END               ICE9_RBASEAE_Flt0
#define FLT1_BASE              ICE9_RBASEA_Flt1
#define FLT1_SIZE             (ICE9_RBASEA_Flt1 | ICE9_RBASEAM_Flt1)
#define FLT1_END               ICE9_RBASEAE_Flt1
#define FLT2_BASE              ICE9_RBASEA_Flt2
#define FLT2_SIZE             (ICE9_RBASEA_Flt2 | ICE9_RBASEAM_Flt2)
#define FLT2_END               ICE9_RBASEAE_Flt2

#define QSC_BASE               ICE9_RBASEA_Qsc
#define QSC_SIZE              (ICE9_RBASEA_Qsc | ICE9_RBASEAM_Qsc)
#define QSC_END                ICE9_RBASEAE_Qsc

#define SDMA_BASE              ICE9_RBASEA_SDma
#define SDMA_SIZE             (ICE9_RBASEA_SDma | ICE9_RBASEAM_SDma)
#define SDMA_END               ICE9_RBASEAE_SDma
#define PA_IS_SDMA(x)         (((x) >= SDMA_BASE) && ((x) < (SDMA_BASE + SDMA_SIZE)))


#define SCB_BASE               ICE9_RBASEA_Scb
#define SCB_SIZE              (ICE9_RBASEA_Scb | ICE9_RBASEAM_Scb)
#define SCB_END                ICE9_RBASEAE_Scb

#endif /* __SC1_FSWFL_H__ */
