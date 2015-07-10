/* Null SC DMA engine, just emulate DMEM and IMEM spaces for testing purposes. */

#include "sicortex/ice9/ice9_dma_spec_sw.h"

/* Just the DMA dmem and imem spaces: */
#define SCDMABASE         (ICE9_RBASEA_Dma)
#define SCDMAEND         (ICE9_RBASEAE_Dma-1)
#define SCDMASIZE	((SCDMAEND - SCDMABASE) + 1)
#define ADDR_IS_SCDMA(x)  ((((uint32) (x)) >= SCDMABASE) && \
                           (((uint32) (x)) <= (SCDMAEND)))
#define PA_IS_SCDMA(x)     (((x) >= SCDMABASE) && ((x) <= (SCDMAEND)))
