/* LAN_LAN cut-through ethernet */

#if !defined(USE_TAP) && !defined(__linux)
#define PA_IS_ETH(x)    0

#else
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>

//#define ETHBASE         0xef0000000                     /* ETH base */
#define ETHBASE         SIM_ULL(0xED0000000)            /* ETH base */
#define ETHSIZE         (4+4+4096+6) /* ETH length */
#define ADDR_IS_ETH(x)  ((((uint32) (x)) >= ETHBASE) && \
                         (((uint32) (x)) < (ETHBASE + ETHSIZE)))
#define PA_IS_ETH(x)     (((x) >= ETHBASE) && ((x) < (ETHBASE + ETHSIZE)))
#endif
