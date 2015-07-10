/* Non-volatile RAM - 16MB long */

/* 
 * TODO The real SC1000 has 64MB of flash starting at 0x10000000
 * (8-bits wide of address) We're carving out of I2C space right now.
 */

#define NVRAWIDTH       20                              /* NVR addr width */
#define NVRSIZE         (1u << NVRAWIDTH)               /* NVR length */
#define NVRAMASK        (NVRSIZE - 1)                   /* NVR addr mask */
#define NVRBASE         0x01E000000                     /* NVR base */
#define ADDR_IS_NVR(x)  ((((uint32) (x)) >= NVRBASE) && \
                         (((uint32) (x)) < (NVRBASE + NVRSIZE)))
#define PA_IS_NVR(x)     (((x) >= NVRBASE) && ((x) < (NVRBASE + NVRSIZE)))
