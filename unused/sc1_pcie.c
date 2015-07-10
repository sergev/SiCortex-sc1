/* sc1_pcie.c

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

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

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

*/

#include "sc1_defs.h"
#include "sc1_pcie.h"
#include "sc1_cac.h"
#include "sc1_pcie_rc.h"
#include "bcm5721_config.h"
#include <assert.h>

#undef BRCM		/* not brcm means denali dummy ep */

extern t_uint64 *M;
extern UNIT mem_unit;

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

#include "sicortex/ice9/ice9_addr_spec_sw.h"
#include "sicortex/ice9/ice9_pci_spec_sw.h"
#include "anthrax_pcie_ep_if.h"

#define PCIE_POLL_INTVL 100000

#define NL "\r\n"

#define DBG(FS, ...) \
    printf(">>>>>>> %s in %s:%d " FS NL, \
           __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__);

extern uint32_t global_int;
extern int32 sim_quiet;

/* Declarations */

// simh entry points
static t_bool pcie_rd( uint64_t pa, uint64_t *val, uint32_t lnt );
static t_bool pcie_wr( uint64_t pa, uint64_t val, uint32_t lnt );
static t_stat pcie_reset( DEVICE *dptr );
static t_stat pcie_attach( UNIT *uptr, char *cptr );

static t_bool pmi_rd64( uint64_t pa, uint64_t *val, uint32_t lnt );
static t_bool pmi_wr64( uint64_t pa, uint64_t val, uint32_t lnt );
static t_stat pmi_reset( DEVICE *dptr );
static t_stat pmi_attach( UNIT *uptr, char *cptr );

// types
typedef struct pcie_range {
    uint64_t begin;
    uint64_t end;
    typeof(pcie_rd) *rd;
    typeof(pcie_wr) *wr;
    int next;
} pcie_range_t;

typedef union {
    uint64_t u64;
    uint64_t u32;
    uint64_t u16;
    uint64_t u8;
} byte_set_t;

// Internal infrastructure
static typeof(pcie_rd) pcie_mem_rd;
static typeof(pcie_wr) pcie_mem_wr;
static typeof(pcie_rd) pcie_io_rd;
static typeof(pcie_wr) pcie_io_wr;
static typeof(pcie_rd) pcie_cfg_rd;
static typeof(pcie_wr) pcie_cfg_wr;

void pmi_link_down(uint8_t arg);

static t_stat scheduled_ice9ToEpCplD(UNIT *cb_unit);
static t_stat pcie_scheduled_run(UNIT *cb_unit);

static void simh_to_pcie( uint64_t *addr, byte_set_t *data, uint8_t *be, uint32_t lnt);
//static void pcie_to_simh( uint64_t *addr, byte_set_t *data, uint32_t *lnt, uint8_t be);

#ifndef BRCM
static void byteswap32(uint32_t count, uint32_t *data);
#endif

#define PCIE_LISTENERS 1000
pcie_range_t pcie_listener[PCIE_LISTENERS];

int pcie_listener_first = 0;
int pcie_listener_free = 0;

//--

uint64_t *pcie_memory;

//--

#define PCIE_CFG_BLK_SZ         (1024*4)
#define PCIE_RC_BASE            (PCIE_CFG_BASE)
#define PCIE_RC_END             (PCIE_RC_BASE + PCIE_CFG_BLK_SZ - 1)

#define IS_PCIE_RC(a) \
    ( (a>=PCIE_RC_BASE) && (a<=PCIE_RC_END) )

//--

typedef struct cfg_dword {
    char *desc;
    void (*read)(int off, int len, uint64_t *val);
    void (*write)(int off, int len, uint64_t val);
} cfg_dword_t;

#define PCIE_DEVFN_CFG_SIZE 4096
static cfg_dword_t pcie_rc_cfg1[PCIE_DEVFN_CFG_SIZE/4];

#define CFG_DWORD(o, d, r, w) \
    assert((o)<PCIE_DEVFN_CFG_SIZE); \
    pcie_rc_cfg1[o/4].desc = d; \
    pcie_rc_cfg1[o/4].read = r; \
    pcie_rc_cfg1[o/4].write = w;

//--

#define PCIE_INVL_ADDR (~0x0ULL)

#define PCIE_MEM_ADDR(addr) \
    (((addr) >= ICE9_RA_PciMem ) \
     ? (((addr) < ICE9_RAE_PciMem ) \
         ? ((addr) - ICE9_RA_PciMem ) /* return this */ \
         : PCIE_INVL_ADDR ) \
     : PCIE_INVL_ADDR )

#define PCIE_IO_ADDR(addr) \
    (((addr) >= ICE9_RA_PciIo ) \
     ? (((addr) < ICE9_RAE_PciIo ) \
         ? ((addr) - ICE9_RA_PciIo ) /* return this */ \
         : PCIE_INVL_ADDR ) \
     : PCIE_INVL_ADDR )

#define PCIE_CFG_ADDR(addr) \
    (((addr) >= ICE9_RA_PciConfig) \
     ? (((addr) < ICE9_RAE_PciConfig) \
         ? ((addr) - ICE9_RA_PciConfig) /* return this */ \
         : PCIE_INVL_ADDR ) \
     : PCIE_INVL_ADDR )

/* Broadcom registers */
#ifdef BRCM
uint32_t brcm_nvram_swarb = 0;
#endif

/* PCIE data structures

   pcie_dib	PCIE dib
   pcie_dev	PCIE device descriptor
   pcie_unit	PCIE unit descriptor
   pcie_reg	PCIE register list
*/

DIB pcie_dib = { PCIE_BASE, PCIE_END, &pcie_rd, &pcie_wr, 0 };

UNIT pcie_unit[] = { {0}
//    { UDATA(&pcie_rcv_svc, UNIT_ATTABLE, PCIE_SIZE) },
};

REG pcie_reg[] = { { NULL }  };

DEVICE pcie_dev = {
    "PCIE",              /* name */
    pcie_unit,           /* units */
    pcie_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &pcie_reset,         /* reset routine */
    NULL,               /* boot routine */
    &pcie_attach,        /* attach routine */
    NULL,               /* detach routine */
    (void *) &pcie_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

// TODO We currently overlap with the CAC but the CAC is checked first.
DIB pmi_dib = { PMI_BASE, PMI_END, &pmi_rd64, &pmi_wr64, 0 };

UNIT pmi_unit[] = { {0}
//    { UDATA(&pmi_rcv_svc, UNIT_ATTABLE, PCIE_SIZE) },
};

REG pmi_reg[] = { { NULL }  };

DEVICE pmi_dev = {
    "PMI",              /* name */
    pmi_unit,           /* units */
    pmi_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &pmi_reset,         /* reset routine */
    NULL,               /* boot routine */
    &pmi_attach,        /* attach routine */
    NULL,               /* detach routine */
    (void *) &pmi_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

#define PMI_REGISTER_SIZE 0x4000
static uint64_t pmi_register[ PMI_REGISTER_SIZE >> 3 ];

#ifndef BRCM
static void byteswap32(uint32_t count, uint32_t *data)
{
    uint32_t i, tmp=0;
    for( i=0; i<count; i++ ) {
        tmp |= (data[i] & (0xFF<< 0)) << 24;
        tmp |= (data[i] & (0xFF<< 8)) <<  8;
        tmp |= (data[i] & (0xFF<<16)) >>  8;
        tmp |= (data[i] & (0xFF<<24)) >> 24;
        data[i] = tmp;
        tmp = 0;
    }
}
#endif
static t_bool pmi_rd64( uint64_t pa, uint64_t *val, uint32_t lnt )
{
    uint64_t ofs = pa - ICE9_RBASEA_Pcie;

    //printf("pmi_rd64 %016llx\r\n", pa);

    if( pa & 0x7 ) {
        printf("PMI read not 64 bit aligned.\r\n");
        return false;
    }

    switch(pa >> 3) {
    case (ICE9_RA_PmiCoreCtrl >> 3):
        //printf("PmiCoreCtrl read received\r\n");
        *val = pmi_register[ofs >> 3];
        break;
    case (ICE9_RA_PmiMsiAddr >> 3):
        //printf("PmiMsiAddr read received\r\n");
        *val = pmi_register[ofs >> 3];
        break;
    case (ICE9_RA_PmiIntrEn >> 3):
        //printf("PmiMsiAddr read received\r\n");
        *val = pmi_register[ofs >> 3];
        break;
    case (ICE9_RA_PmiLinkStat >> 3):
        //printf("ICE9_RA_PmiLinkStat read received\r\n");
        *val = pmi_register[ofs >> 3];
        break;
    default:
        *val = pmi_register[ofs >> 3];
        printf("SIMH: PMI unimplemented read: %016llx\r\n",pa);
        break;
    }

    return true;
}

static t_bool pmi_wr64( uint64_t pa, uint64_t val, uint32_t lnt )
{
    uint64_t ofs = pa - ICE9_RBASEA_Pcie;

    printf("pmi_wr64: %016llx <- %016llx\r\n", pa, val);

    if( pa & 0x7 ) {
        //printf("PMI read not 64 bit aligned.\r\n");
        return false;
    }

    switch(pa >> 3) {
    case (ICE9_RA_PmiCoreCtrl >> 3):
        if(val & 0x1ULL) {
            pcie_schedule(30000, pmi_link_down, 0);
        } else {
            pmi_register[ofs >> 3] = val;
        }
        //printf("PmiCoreCtrl write received: %016llx\r\n", val);
        break;
    case (ICE9_RA_PmiMsiAddr >> 3):
        //printf("PmiMsiAddr write received: %016llx\r\n", val);
        pmi_register[ofs >> 3] = val;
        break;
    case (ICE9_RA_PmiIntrEn >> 3):
        printf("PmiIntrAddr write received: %016llx\r\n", val);
        pmi_register[ofs >> 3] = val;
        break;
    default:
        pmi_register[ofs >> 3] = val;
        printf("SIMH: PMI unimplemented write: %016llx\r\n",pa);
        break;
    }
    return true;
}

void pmi_link_down(uint8_t arg)
{
    uint64_t ofs = ICE9_RA_PmiLinkStat - ICE9_RBASEA_Pcie;
    pmi_register[ofs >> 3] |= 0x7ULL;
}

static t_stat pmi_reset( DEVICE *dptr )
{
    return SCPE_OK;
}

static t_stat pmi_attach( UNIT *uptr, char *cptr )
{
    return SCPE_OK;
}


static void pcie_rc_init(void) {
    return;
    //pcie_rc_cfg1[0] = ((typeof(pcie_rc_cfg1)) { "VENDOR ID / DEV ID", NULL, NULL });
    CFG_DWORD(0x00, "VENDOR ID / DEV ID", NULL, NULL);
    CFG_DWORD(0x04, "STATUS / COMMAND", NULL, NULL);
    CFG_DWORD(0x08, "CLASS / REV", NULL, NULL);
    CFG_DWORD(0x0C, "BIST/HT/LT/CLS", NULL, NULL);
    CFG_DWORD(0x10, "BAR0", NULL, NULL);
    CFG_DWORD(0x14, "BAR1", NULL, NULL);
    CFG_DWORD(0x18, "PRI/2ND/SUB BUS# / 2ND LAT", NULL, NULL);
    CFG_DWORD(0x1C, "IO BASE/LIMIT / 2ND STATUS", NULL, NULL);
    CFG_DWORD(0x20, "MEM BASE/LIMIT", NULL, NULL);
    CFG_DWORD(0x24, "PREF MEM BASE/LIMIT", NULL, NULL);
    CFG_DWORD(0x28, "PREF MEM BASE - U23", NULL, NULL);
    CFG_DWORD(0x2C, "PREF MEM LIMIT - U23", NULL, NULL);
    CFG_DWORD(0x30, "IO BASE/LIMIT - U16", NULL, NULL);
    CFG_DWORD(0x34, "CAP. PTR", NULL, NULL);
    CFG_DWORD(0x38, "ROM BASE", NULL, NULL);
    CFG_DWORD(0x3C, "INT LINE/PIN / BRCTL", NULL, NULL);
}

static uint64_t pcie_be_read(uint32_t low, uint32_t high, uint32_t be)
{
    int i;
    uint64_t ival, rval = 0x0ULL;

    for( i=0; i < 8; i++) {
        ival = (i<4) ? low : high;
        if( be & (1<<i) ) {
            rval |= (ival & (0xFFU << (8*(i%4)))) << (32 * (i/4));
        }
    }
    return rval;
}

typedef enum {
    PcieTagStateUnused = 0,
    PcieTagStateAllocated,
    PcieTagStateCompleted,
} pcie_tag_state_t;

typedef struct pcie_tag {
    pcie_tag_state_t state;
    uint64_t val;
    uint8_t *data;
} pcie_tag_t;

static pcie_tag_t pcie_tags[256];

#ifndef BRCM
static t_bool pcie_ep_cfg_rd(uint64_t ofs, uint64_t *val, uint32_t be)
{
    uint8_t tag = 30; // TODO: Alloc a tag

    ice9ToEpCfgRd(ofs, tag, (uint8_t)be);

    if(pcie_tags[tag].state == PcieTagStateCompleted) {
        *val = pcie_tags[tag].val;
        bzero(&pcie_tags[tag], sizeof(pcie_tags[tag]));
    } else  {
        printf("**ERROR** sequence not completed!\r\n");
    }

    byteswap32(2, (uint32_t*)val);

    return true;
}

static t_bool pcie_ep_cfg_wr(uint64_t ofs, uint64_t val, uint32_t be)
{
    uint8_t tag = 255;
    uint32_t data = val;

    byteswap32(1, (uint32_t*)&data);

    ice9ToEpCfgWr(ofs, data, tag, (uint8_t)be);

    if(pcie_tags[tag].state == PcieTagStateCompleted) {
        bzero(&pcie_tags[tag], sizeof(pcie_tags[tag]));
    } else  {
        printf("**ERROR** sequence not completed!\r\n");
    }

    return true;
}
#endif

static t_bool pcie_rc_static_cfg_rd( uint64_t ofs, uint64_t *val, uint32_t be)
{
    uint64_t rc_size = sizeof(sc1000_pcie_rc);

    if( ofs < 4096 ) {
        if( ofs < rc_size ) {
            *val = pcie_be_read(sc1000_pcie_rc[ofs/4], sc1000_pcie_rc[(ofs/4)+1], be);
        } else {
            *val= 0LL;
        }
    }

    return true;
}

static t_bool pcie_rc_static_cfg_wr( uint64_t ofs, uint64_t val, uint32_t be)
{
    return true;
}

#ifdef BRCM
static t_bool pcie_brcm_cfg_rd( uint64_t ofs, uint64_t *val, uint32_t be)
{
    uint64_t cfg_size = sizeof(bcm5721_config);

    if( ofs < 4096 ) {
        if( ofs < cfg_size ) {
            *val = pcie_be_read(bcm5721_config[ofs/4], bcm5721_config[(ofs/4)+1], be);
        } else {
            *val= 0LL;
        }
    }

    if (0) printf("sc1_pcie : brcm cfg rd: ofs 0x%llx val 0x%x" NL, ofs, bcm5721_config[ofs/4]);
    return true;
}

static t_bool pcie_brcm_cfg_wr(uint64_t ofs, uint64_t val, uint32_t be)
{
    int i;
    uint64_t cfg_size = sizeof(bcm5721_config);
    uint64_t wval, mask;

    if (0) printf("sc1_pcie : brcm cfg wr : ofs 0x%llx val 0x%llx be %x" NL, ofs, val, be);

    if( ofs < 4096 ) {
        if( ofs < cfg_size ) {
	    /* BAR1 mask for size discovery; 64k region, 64 bit, no-prefetch */
	    if (ofs == 0x10) 	
	        val = (val & 0x00000000FFFF0000) | ((uint64_t)bcm5721_config[ofs/4] & 0xFULL);
            wval = (bcm5721_config[ofs/4]) | ((uint64_t)bcm5721_config[ofs/4+1] << 32);
	    for (i = 0 ;  i < 8 ; i++) {
	        if (be & (1<<i)) {
		    mask = 0xFFULL << (8 * i);
		    wval = (wval & ~mask) | (val & mask);
		}
	    }
	    bcm5721_config[ofs/4]   =  wval & 0xffffffff;
	    bcm5721_config[ofs/4+1] = (wval >> 32) & 0xffffffff;
        } else {
        }
    }

    return true;
}

#endif

static t_bool pcie_mem_rd(uint64_t ofs, uint64_t *val, uint32_t lnt)
{
#ifdef BRCM
    //printf("pcie mem wr : ofs %llx bar[4] 0x%x bar[5] 0x%x\n\r", 
    //    ofs, bcm5721_config[4], bcm5721_config[5]);
    ofs -= (bcm5721_config[0x10/4] & ~0xF);
    switch (ofs) {
    case NVRAM_SWARB:
        *val = (uint64_t) brcm_nvram_swarb;
        break;
    case NVRAM_CMD:
        *val = NVRAM_CMD_DONE;
        break;
    default:
        break;
    }
#endif
    return true;
}

static t_bool pcie_mem_wr(uint64_t ofs, uint64_t val, uint32_t lnt)
{
#ifdef BRCM
    //printf("pcie mem rd : ofs %llx bar[4] 0x%x bar[5] 0x%x\n\r", 
    //    ofs, bcm5721_config[4], bcm5721_config[5]);
    ofs -= (bcm5721_config[0x10/4] & ~0xF);
    switch (ofs) {
    case NVRAM_SWARB:
        if (val & SWARB_REQ_SET1) 
            brcm_nvram_swarb |=  SWARB_GNT1;
        if (val & SWARB_REQ_CLR1)
            brcm_nvram_swarb &= ~SWARB_GNT1;
        break;
    default:
        break;
    }
#endif

    return true;
}

static t_bool pcie_io_rd(uint64_t ofs, uint64_t *val, uint32_t lnt)
{
    return true;
}

static t_bool pcie_io_wr(uint64_t ofs, uint64_t val, uint32_t lnt)
{
    return true;
}

/* TODO: I'm still not sure this is right. */
#define PCIE_BDF(B,D,F) \
    (((B)*32ULL*8ULL*4096ULL) \
     +((D)*8ULL*4096ULL) \
     +((F)*4096ULL))

static t_bool pcie_cfg_rd(uint64_t ofs, uint64_t *val, uint32_t be)
{
    uint64_t ofs_cfg_base = ofs & (~(4096ULL-1ULL));

    if(ofs_cfg_base == PCIE_BDF(0,0,0)){
        return pcie_rc_static_cfg_rd(ofs, val, be);
    } else if(ofs_cfg_base == PCIE_BDF(1,0,0)) {
        ofs -= PCIE_BDF(1,0,0);
#ifdef BRCM
        return pcie_brcm_cfg_rd(ofs, val, be);
#else
        return pcie_ep_cfg_rd(ofs, val, be);
#endif
    } else {
#ifdef DEBUG
        printf("sc1_pcie : cfg read Unhandled\r\n");
#endif
        *val = 0x0ULL;
        return true;
    }
}

static t_bool pcie_cfg_wr(uint64_t ofs, uint64_t val, uint32_t lnt)
{
    uint64_t ofs_cfg_base = ofs - (ofs % PCIE_DEVFN_CFG_SIZE);
    byte_set_t data = {u64: val};

    uint8_t be;

    simh_to_pcie(&ofs, &data, &be, lnt);

    if(ofs_cfg_base == PCIE_BDF(0,0,0)) {
        return pcie_rc_static_cfg_wr(ofs, data.u32, be);
    } else if(ofs_cfg_base == PCIE_BDF(1,0,0)) {
        uint64_t rel_ofs = ofs % PCIE_DEVFN_CFG_SIZE;
#ifdef BRCM
        return pcie_brcm_cfg_wr(rel_ofs, data.u32, be);
#else
        return pcie_ep_cfg_wr(rel_ofs, data.u32, be);
#endif
    } else {
        printf("sc1_pcie : write unknown at %016llx: %016llx\r\n", ofs, val);
        return true;
    }
}

static void simh_to_pcie( uint64_t *addr, byte_set_t *data, uint8_t *be, uint32_t lnt)
{
    uint64_t wordaddr = *addr & (~0x3ULL);

    // Create mask from length
    *be = ((1<<(1<<lnt))-1);
    // Position mask according to offset
    *be <<= (*addr & 0x3);

    // move data
    data->u64 <<= (*addr & 0x3);

    *addr = wordaddr;
}

static t_bool pcie_rd( uint64_t pa, uint64_t *val, uint32_t lnt)
{
    t_bool rval;
    uint64_t wpa = pa & (~0x3ULL);
    uint32_t be = ((1<<(1<<lnt))-1);

    *val=0;
    be <<= (pa & 0x3ULL)%4;

    if( PCIE_MEM_ADDR(wpa) != PCIE_INVL_ADDR ) {
        rval = pcie_mem_rd(PCIE_MEM_ADDR(wpa), val, be);  /* MEM */
    } else if( PCIE_IO_ADDR(wpa) != PCIE_INVL_ADDR ) {
        rval = pcie_io_rd(PCIE_IO_ADDR(wpa), val, be);    /* IO  */
    } else if( PCIE_CFG_ADDR(wpa) != PCIE_INVL_ADDR ) {
        rval = pcie_cfg_rd(PCIE_CFG_ADDR(wpa), val, be);  /* CFG */
    } else {
        printf("[ !?! Read of non-PCIe address: %#016llx -- How did we get here? ]\r\n", wpa);
        rval = false;
    }

    *val = (*val) >> (8 * (pa & 3));

    return rval;
}

#if 0
    if( !handled ) {
        for(i = pcie_listener_first; i < PCIE_LISTENERS; i = pcie_listener[i].next ) {
            if( (pa >= pcie_listener[i].begin) &&
                (pa <= pcie_listener[i].end) ) {
                rval = pcie_listener[i].rd(pa, val, lnt|(i<<16));
            }
        }
    } else {
        *val = ~(0x0ULL);
    }
#endif

static t_bool pcie_wr(uint64_t pa, uint64_t idata, uint32_t lnt)
{
    //printf("pcie_wr: %016llx -> %016llx\r\n",pa , idata);
    if( PCIE_MEM_ADDR(pa) != PCIE_INVL_ADDR ) {
        return pcie_mem_wr(PCIE_MEM_ADDR(pa), idata, lnt);  /* MEM */
    } else if( PCIE_IO_ADDR(pa) != PCIE_INVL_ADDR ) {
        return pcie_io_wr(PCIE_IO_ADDR(pa), idata, lnt);    /* IO */
    } else if( PCIE_CFG_ADDR(pa) != PCIE_INVL_ADDR ) {
        return pcie_cfg_wr(PCIE_CFG_ADDR(pa), idata, lnt);  /* CFG */
    } else {
        printf("[ !?! Read of non-PCIe address: %#016llx -- How did we get here? ]\r\n", pa);
        return false;
    }
}

void pcie_listener_init()
{
    int i;

    for( i=0; i < PCIE_LISTENERS; i++ )
        pcie_listener[i].next = i+1;

    pcie_listener_free = 0;
    pcie_listener_first = PCIE_LISTENERS;
}

//-------------------------------------------------------------------

void pcie_schedule(uint64_t nanoseconds, void (*fn)(uint8_t), uint8_t arg)
{
    if( nanoseconds <= 1 ) {
        fn(arg);
    } else {
        UNIT *cb_unit = (typeof(cb_unit))calloc(1, sizeof(*cb_unit));
        cb_unit->action = &pcie_scheduled_run;
        cb_unit->filename = (typeof(cb_unit->filename)) fn;
        cb_unit->buf = (typeof(cb_unit->buf)) arg;
        cb_unit->flags = 0; // IMPORTANT to fool simh
        sim_activate(cb_unit, nanoseconds);
    }
}

// Assistance function
static t_stat pcie_scheduled_run(UNIT *cb_unit)
{
    void (*fn)(uint8_t) = (typeof(fn)) cb_unit->filename;
    uint8_t arg = (typeof(arg)) cb_unit->buf;
    bzero(cb_unit, sizeof(cb_unit));
    free(cb_unit);
    fn(arg);
    return SCPE_OK;
}

//--

// DMA read
void epToIce9MemRd(uint64_t addr, uint32_t sizeInDW, uint8_t tag, uint8_t be)
{
    // TODO We get bitten by the LE with words being swapped on the ends
    uint8_t *data = (typeof(data)) calloc(sizeof(uint32_t), sizeInDW+1);
    uint32_t i, *MW = (uint32_t *)M, *dw = (uint32_t *)data;
    uint64_t paddr = (addr & PA_MASK)>>2;
    UNIT *cb_unit = (typeof(cb_unit))calloc(1, sizeof(*cb_unit));

    i = 0;
    if( sizeInDW > 1 ) {
        switch(be & 0xF0) {
        case 0xF0: dw[i] = MW[paddr+i]; break;
        case 0x70: dw[i] &= 0xFF000000UL; dw[i] |= MW[paddr+i] & 0xFF000000UL; break;
        case 0x30: dw[i] &= 0xFFFF0000UL; dw[i] |= MW[paddr+i] & 0xFFFF0000UL; break;
        case 0x10: dw[i] &= 0xFFFFFF00UL; dw[i] |= MW[paddr+i] & 0xFFFFFF00UL; break;
        default: printf("Error: shouldn't get here!\r\n");
        }
        i = 1;
    }

    for( /**/ ; (i+1)<sizeInDW; i++) {
        dw[i] = MW[paddr+i];
    }

    switch(be & 0x0F) {
    case 0x0F: dw[i] = MW[paddr+i]; break;
    case 0x07: dw[i] &= 0xFF000000UL; dw[i] |= MW[paddr+i] & 0xFF000000UL; break;
    case 0x03: dw[i] &= 0xFFFF0000UL; dw[i] |= MW[paddr+i] & 0xFFFF0000UL; break;
    case 0x01: dw[i] &= 0xFFFFFF00UL; dw[i] |= MW[paddr+i] & 0xFFFFFF00UL; break;
    default: printf("Error: shouldn't get here!\r\n");
    }

    cb_unit->action = &scheduled_ice9ToEpCplD;
    cb_unit->filename = (typeof(cb_unit->filename)) data;
    cb_unit->capac = (typeof(cb_unit->capac)) sizeInDW;
    cb_unit->buf = (typeof(cb_unit->buf)) tag;
    cb_unit->flags = 0; // IMPORTANT to fool simh
    sim_activate(cb_unit, 200);
}

// DMA write
void epToIce9MemWr(uint64_t addr, uint8_t *data, uint32_t sizeInDW, uint8_t tag, uint8_t be)
{
    // TODO We get bitten by the LE with words being swapped on the ends
    uint32_t i, *MW = (uint32_t *)M, *dw = (uint32_t *)data;
    uint64_t paddr = (addr & PA_MASK)>>2;
    uint64_t msiaddr = pmi_register[(ICE9_RA_PmiMsiAddr-ICE9_RBASEA_Pcie) >> 3];

    // Handle MSIs
    // TODO: Is this entirely accurate or should we be setting msi base?
    if( addr == msiaddr ) {
        cac_csw_int( (uint32_t)(paddr & 0x3), (dw[0] >> 16) & 15, dw[0] & 0xff);
        return;
    }
    // Throw away invalid writes
    if( paddr >= MEMSIZE ) {
        DBG("Invalid MemWr %016llx", paddr);
        return;
    }

    i = 0;
    if( sizeInDW > 1 ) {
        switch(be & 0xF0) {
        // This is little endian but the be is big endian
        case 0xF0: MW[paddr+i] = dw[i]; break;
        case 0x70: MW[paddr+i] &= 0x000000FFUL; MW[paddr+i] |= dw[i] & 0xFFFFFF00UL; break;
        case 0x30: MW[paddr+i] &= 0x0000FFFFUL; MW[paddr+i] |= dw[i] & 0xFFFF0000UL; break;
        case 0x10: MW[paddr+i] &= 0x00FFFFFFUL; MW[paddr+i] |= dw[i] & 0xFF000000UL; break;
        default: printf("Error: shouldn't get here!\r\n");
        }
        //DBG("%016llx:%08x = %08x", 4*(paddr+i), dw[i], MW[paddr+i]);
        i = 1;
    }

    for( /**/ ; (i+1)<sizeInDW; i++) {
        MW[paddr+i] = dw[i];
        //DBG("%016llx:%08x = %08x", 4*(paddr+i), dw[i], MW[paddr+i]);
    }

    switch(be & 0x0F) {
    // This is little endian but the be is big endian
    case 0x0F: MW[paddr+i] = dw[i]; break;
    case 0x0E: MW[paddr+i] &= 0xFF000000UL; MW[paddr+i] |= dw[i] & 0x00FFFFFFUL; break;
    case 0x0A: MW[paddr+i] &= 0xFFFF0000UL; MW[paddr+i] |= dw[i] & 0x0000FFFFUL; break;
    case 0x08: MW[paddr+i] &= 0xFFFFFF00UL; MW[paddr+i] |= dw[i] & 0x000000FFUL; break;
    case 0x07: MW[paddr+i] &= 0x000000FFUL; MW[paddr+i] |= dw[i] & 0xFFFFFF00UL; break;
    case 0x03: MW[paddr+i] &= 0x0000FFFFUL; MW[paddr+i] |= dw[i] & 0xFFFF0000UL; break;
    case 0x01: MW[paddr+i] &= 0x00FFFFFFUL; MW[paddr+i] |= dw[i] & 0xFF000000UL; break;
    default: printf("Error: shouldn't get here!\r\n");
    }

    //DBG("%016llx:%08x = %08x", 4*(paddr+i), dw[i], MW[paddr+i]);
}

static t_stat scheduled_ice9ToEpCplD(UNIT *cb_unit)
{
    uint32_t sizeInDW = (typeof(sizeInDW)) cb_unit->capac;
    uint8_t *data = (typeof(data)) cb_unit->filename;
    uint8_t tag = (typeof(tag)) (0xFF & cb_unit->buf);

    ice9ToEpCplD(tag, data, sizeInDW);

    free(data);
    bzero(cb_unit, sizeof(cb_unit));
    free(cb_unit);
    return SCPE_OK;
}

void epToIce9Cpl(uint8_t tag)
{
    pcie_tags[tag].state = PcieTagStateCompleted;
    return;
}

void epToIce9CplD(uint8_t *data, uint32_t sizeInDW, uint8_t tag)
{
    pcie_tags[tag].state = PcieTagStateCompleted;
    // TODO proper data completion
    pcie_tags[tag].val = *(uint64_t*)data;
    return;
}

// Functions for sending hotplug / INTx messages
void epToIce9Msg(pcieTlpMsgT msg, uint8_t tag)
{
    DBG();
}

void epToIce9MsgD(pcieTlpMsgT msg, uint8_t tag, uint8_t *data, uint32_t sizeInDW)
{
    DBG();
}

//-------------------------------------------------------------------

static t_stat pcie_reset(DEVICE *dptr)
{
    // PCIe EP API call (REQUIRED)
    pcie_ep_init();

    pcie_listener_init();
    pcie_rc_init();

    return SCPE_OK;
}

static t_stat pcie_attach( UNIT *uptr, char *cptr )
{
    return SCPE_OK;
}
