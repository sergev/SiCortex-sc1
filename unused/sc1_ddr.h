#ifndef __SC1_DDR_H__
#define __SC1_DDR_H__

#ifdef SIMX_NATIVE  /* Built into simx */
# include "ice9_addr_spec_sw.h"
# include "ice9_ddr_spec_sw.h"
#else
# include "sicortex/ice9/ice9_addr_spec_sw.h"
# include "sicortex/ice9/ice9_ddr_spec_sw.h"
#endif

/* DDR Memory Controller (Ddr) */

#define DDR_BASE              ICE9_RBASEA_Ddr0
#define DDR_SIZE             (ICE9_RBASEA_Ddr1 | ICE9_RBASEAM_Ddr1)
#define DDR_END               ICE9_RBASEAE_Ddr1

#define ADDR_IS_DDR(x)        ((((uint32) (x)) >= DDR_BASE) && \
                               (((uint32) (x)) < (DDR_BASE + DDR_SIZE)))
#define PA_IS_DDR(x)          (((x) >= DDR_BASE) && ((x) < (DDR_BASE + DDR_SIZE)))

#endif /* __SC1_DDR_H__ */
