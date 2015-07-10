#ifndef __SC1_SCB_H__
#define __SC1_SCB_H__

#ifdef SIMX_NATIVE  /* Built into simx */
# include "ice9_addr_spec_sw.h"
# include "ice9_scb_spec_sw.h"
#else
# include "sicortex/ice9/ice9_addr_spec_sw.h"
# include "sicortex/ice9/ice9_scb_spec_sw.h"
#endif

/* Serial Configuration Bus (SCB) */

#define SCB_BASE              ICE9_RBASEA_Scb
#define SCB_SIZE             (ICE9_RBASEA_Scb | ICE9_RBASEAM_Scb)
#define SCB_END               ICE9_RBASEAE_Scb

#define ADDR_IS_SCB(x)        ((((uint32) (x)) >= SCB_BASE) && \
                               (((uint32) (x)) < (SCB_BASE + SCB_SIZE)))
#define PA_IS_SCB(x)          (((x) >= SCB_BASE) && ((x) < (SCB_BASE + SCB_SIZE)))

/* Default feature, product, and mask revision */
#define SCB_CHIPREV	(                     0 << ICE9_ScbChipRev_Features_SHIFT |  \
			  Ice9_AddrProduct_ICE9 << ICE9_ScbChipRev_Product_SHIFT  |  \
			                      1 << ICE9_ScbChipRev_Rev_SHIFT )

#endif /* __SC1_SCB_H__ */
