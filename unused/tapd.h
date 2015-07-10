
#include <stdint.h>
#include <linux/if_ether.h>

typedef enum ethmsgtype {
    ETPACKET,
    ETIOCTL,
} ethmsgtype_t;

typedef struct ethpacket {
    uint32_t size;                 /* Size of packet */
    uint8_t  data[ETH_FRAME_LEN];  /* Packet data */
} ethpacket_t;

typedef struct ethioctl {
    uint32_t cmd_ret;      /* Command or return value depending on direction */
    uint8_t  padding[22];  /* to talk to older tapd that spoke differently */
    uint8_t  hwaddr[6];
} ethioctl_t;

typedef struct ethmsg {
    enum ethmsgtype type;
    uint32_t padding;  /* for the same reason as above */
    union {
        struct ethpacket ep;
        struct ethioctl  ei;
    } payload;
} ethmsg_t;
