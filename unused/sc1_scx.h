#ifndef __SC1_SCX_H__
#define __SC1_SCX_H__

#ifdef SIMX_NATIVE  /* Built into simx */
# include "ice9_magic_spec_sw.h"
#else
# include "sicortex/ice9/ice9_magic_spec_sw.h"
#endif

/* Pseudo device to control the System-C model (scx) */

#define SCX_BASE              ICE9_RBASEA_Simh
#define SCX_SIZE              (ICE9_RBASEA_Simh | ICE9_RBASEAM_Simh)
#define SCX_END               ICE9_RBASEAE_Simh

#define ADDR_IS_SCX(x)        ((((uint32) (x)) >= SCX_BASE) && \
                               (((uint32) (x)) < (SCX_BASE + SCX_SIZE)))
#define PA_IS_SCX(x)          (((x) >= SCX_BASE) && ((x) < (SCX_BASE + SCX_SIZE)))

#endif /* guard */
