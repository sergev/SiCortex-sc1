/* sc1_eth.c - SiCortex 1 cut-through ethernet simulator

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


#if defined (_WIN32)
#include "sc1_defs.h"

static t_stat eth_reset(DEVICE *dptr)
{
return SCPE_OK;
}

#else

// TODO cleanup includes
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "sc1_defs.h"
#include "tapd.h"
#include "sc1_eth.h"

#define ETH_POLL_INTVL 100000

#define MAXQUEUELEN 256

/* These definitions must match those in lanlan.c! */

#define ETH_IOREG                    0x0

#define ETH_IOREG_MIN                (ETH_IOREG)
#define ETH_IOREG_SIZE               (4)
#define ETH_IOREG_MAX                (ETH_IOREG_MIN+ETH_IOREG_SIZE)

#define ETH_IOREG_R_UP               0x1
#define ETH_IOREG_R_DOWN             0x2
#define ETH_IOREG_R_INTS_ENABLED     0x4
#define ETH_IOREG_R_INTS_DISABLED    0x8
#define ETH_IOREG_R_PKT_AVAIL        0x10
#define ETH_IOREG_R_NO_PKT_AVAIL     0x20
#define ETH_IOREG_R_PKT_SIZE(s)      ((s & 0xFFFF) << 16)
#define ETH_IOREG_W_UP               0x1
#define ETH_IOREG_W_DOWN             0x2
#define ETH_IOREG_W_ENABLE_INTS      0x4
#define ETH_IOREG_W_DISABLE_INTS     0x8
#define ETH_IOREG_W_PKT_READ         0x10
#define ETH_IOREG_W_PKT_WRITTEN      0x20
#define ETH_IOREG_W_PKT_SIZE(s)      (s >> 16)

#define ETH_IOCTL_MIN                (ETH_IOREG_MAX)
#define ETH_IOCTL_SIZE               (4)
#define ETH_IOCTL_MAX                (ETH_IOCTL_MIN+ETH_IOCTL_SIZE)

#define ETH_TXRXBUF_MIN              (ETH_IOCTL_MAX)
#define ETH_TXRXBUF_SIZE             (4096)
#define ETH_TXRXBUF_MAX              (ETH_TXRXBUF_MIN+ETH_TXRXBUF_SIZE)

#define ETH_HWADDR_MIN               (ETH_TXRXBUF_MAX)
#define ETH_HWADDR_SIZE              (6)
#define ETH_HWADDR_MAX               (ETH_HWADDR_MIN+ETH_HWADDR_SIZE)


extern uint32 global_int;
extern int32 sim_quiet;

struct eth_priv
{
    int             up:1;
    int             enabinterrupts:1;
    int             packetavail:1;
    int             packetsize:16;
    int             ioctlreturn;
    int             fd;
    uint8	    hwaddr[6];
    uint32          txbuf[4096/4];
    uint32          rxbuf[4096/4];

    uint32          *queuedpackets[MAXQUEUELEN];
    uint16          packetsizes[MAXQUEUELEN];
    int             queuelen;
};

static char *tappath = "/dev/tap0";
struct eth_priv priv_instance;

/* Declarations */

static t_bool eth_rd( t_uint64 pa, t_uint64 *val, uint32 unit );
static t_bool eth_wr( t_uint64 pa, t_uint64 val, uint32 unit );
//static t_stat eth_ex( t_value *vptr, t_addr exta, UNIT *uptr, int32 sw );
//static t_stat eth_dep( t_value val, t_addr exta, UNIT *uptr, int32 sw );
static t_stat eth_reset( DEVICE *dptr );
static t_stat eth_attach( UNIT *uptr, char *cptr );
//static t_stat eth_detach( UNIT *uptr );
static t_stat eth_rcv_svc( UNIT *uptr );
static void eth_up(struct eth_priv* priv);
static void eth_down(struct eth_priv* priv);

/* ETH data structures

   eth_dib	ETH dib
   eth_dev	ETH device descriptor
   eth_unit	ETH unit descriptor
   eth_reg	ETH register list
*/

DIB eth_dib = { ETHBASE, ETHBASE+ETHSIZE, &eth_rd, &eth_wr, 0 };

UNIT eth_unit[] = { 
    { UDATA(&eth_rcv_svc, UNIT_ATTABLE, ETHSIZE) },
    //{ UDATA(eth_rcv_svc, 0, 0), 10 /*msec*/ },
};

REG eth_reg[] = { { NULL }  };

DEVICE eth_dev = {
    "ETH",              /* name */
    eth_unit,           /* units */
    eth_reg,            /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &eth_reset,         /* reset routine */
    NULL,               /* boot routine */
    &eth_attach,        /* attach routine */
    NULL,               /* detach routine */
    (void *) &eth_dib,  /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

static void eth_loadpacket(struct eth_priv* priv)
{        
    if (priv->queuelen == 0)
    {               
        if( priv->queuelen < 0)
            printf("[ Aiiieee! SIMH ethernet queue length of %d\r\n", priv->queuelen);
        priv->packetavail = 0;
        return;
    }
    memcpy((void*)priv->rxbuf, (void*)priv->queuedpackets[0], priv->packetsizes[0]);
    priv->packetsize = priv->packetsizes[0];
    priv->packetavail = 1;
}

static void eth_putpacket(struct eth_priv* priv, uint32 *packet, uint16 size)
{
    if (priv->queuelen == (MAXQUEUELEN-1) )
    {
        printf("[ eth: dropping ]\r\n");
        return;
    }
    priv->queuedpackets[priv->queuelen] = (uint32*)malloc(size);
    priv->packetsizes[priv->queuelen] = size;
    memcpy((void*)priv->queuedpackets[priv->queuelen], (void*)packet, size);
    priv->queuelen++;
}

static void eth_nextpacket(struct eth_priv* priv)
{
    int i;

    if (priv->queuelen == 0)
    { 
        priv->packetavail = 0;
        return;
    }

    if( priv->queuedpackets[0] )
        free(priv->queuedpackets[0]);

    for (i=0; i<priv->queuelen-1; i++)
    {
        priv->queuedpackets[i] = priv->queuedpackets[i+1];
        priv->packetsizes[i] = priv->packetsizes[i+1];
    }
    priv->packetsizes[priv->queuelen] = 0;
    priv->queuelen--;

    eth_loadpacket(priv);

    return;
}

static int eth_ioctl(struct eth_priv* priv, uint32 idata)
{
    int up = priv->up;
    ethmsg_t emsg;

    /* We only handle one ioctl: */
    if (idata != SIOCGIFHWADDR) {
	return FALSE;
    }

    if( !up ) {
        eth_up(priv);
    }

    emsg.type = ETIOCTL;
    emsg.payload.ei.cmd_ret = idata;

    // Send out IOCTL
    if (priv->fd >= 0)
        write(priv->fd, &emsg, sizeof(emsg));
    // Read IOCTL result
    if (priv->fd >= 0) {
        do {
            read(priv->fd, &emsg, sizeof(emsg));
            if( emsg.type != ETIOCTL ) {
                printf("[ eth: throwing away non-ioctl packet to keep iotcl atomic. ]\r\n");
            }
        } while( emsg.type != ETIOCTL );
    }

    memcpy(priv->hwaddr, &emsg.payload.ei.hwaddr,
	   sizeof(emsg.payload.ei.hwaddr));
    priv->ioctlreturn = emsg.payload.ei.cmd_ret;

    if( !up ) {
        eth_down(priv);
    }

    return TRUE;
}

static void eth_write(struct eth_priv* priv, unsigned int size)
{
    ethmsg_t emsg;
    emsg.type = ETPACKET;
    emsg.payload.ep.size = size;
    memcpy(emsg.payload.ep.data, priv->txbuf, (ETH_FRAME_LEN < 4096) ? ETH_FRAME_LEN : 4096);
    if (priv->fd >= 0)
        write(priv->fd, &emsg, sizeof(emsg));
}

static void eth_up(struct eth_priv* priv)
{ 
        struct sockaddr_un sockaddr;
        int i;
        
        priv->fd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (priv->fd < 0)
        {
                perror("socket");
                return;
        }
        sockaddr.sun_family = AF_UNIX;
        strcpy(sockaddr.sun_path, tappath);
        i = connect(priv->fd, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
        if (i < 0)
        {
                printf("[ eth: NOT up %s ]\r\n", tappath);
                perror("connect");
                close(priv->fd);
                priv->fd = -1;
                return;
        }

        priv->up = 1;
        if(!sim_quiet) printf("[ eth: up ]\r\n");
}       
        
static void eth_down(struct eth_priv* priv)
{
    if (priv->fd >= 0)
        close(priv->fd);
    priv->fd = -1;
    priv->up = 0;
    if(!sim_quiet) printf("[ eth: down ]\r\n");
}

static t_bool eth_rd( t_uint64 pa, t_uint64 *val, uint32 unit)
{
    t_uint64 odata = 0;
    uint32 relative_addr = ((uint32) ((pa - ETHBASE) ));
    struct eth_priv *priv = &priv_instance;

    if ((relative_addr >= ETH_HWADDR_MIN) && (relative_addr < ETH_HWADDR_MAX)) {
	/* read mac address one byte at a time */
	if (unit != L_BYTE) return FALSE;
	odata = priv->hwaddr[relative_addr - ETH_HWADDR_MIN];
    } else {

	if ( unit != L_WORD ) return FALSE;

	if (relative_addr == ETH_IOREG)
	{
	    odata = 0x0ULL;
	    if (priv->up)
		odata |= ETH_IOREG_R_UP;
	    if (!priv->up)
		odata |= ETH_IOREG_R_DOWN;
	    if (priv->enabinterrupts)
		odata |= ETH_IOREG_R_INTS_ENABLED;
	    if (!priv->enabinterrupts)
		odata |= ETH_IOREG_R_INTS_DISABLED;
	    if (priv->packetavail)
		odata |= ETH_IOREG_R_PKT_AVAIL |
		    ETH_IOREG_R_PKT_SIZE(priv->packetsize);
	    if (!priv->packetavail)
		odata |= ETH_IOREG_R_NO_PKT_AVAIL;
	} else if (relative_addr == ETH_IOCTL_MIN) {
	    odata = priv->ioctlreturn;
	} else if (relative_addr >= ETH_TXRXBUF_MIN && relative_addr < ETH_TXRXBUF_MAX) {
	    odata = priv->rxbuf[(relative_addr - ETH_TXRXBUF_MIN) / 4];
	} else {
	    printf("[ eth: unimplemented read from "
		   "offset 0x%x ]\n", relative_addr);
	    return FALSE;
	}
    }

    *val = odata;

    return TRUE;
}

static t_bool eth_wr(t_uint64 pa, t_uint64 idata, uint32 unit)
{
    uint32 relative_addr = ((uint32) ((pa - ETHBASE) ));
    struct eth_priv *priv = &priv_instance;

    if (relative_addr == ETH_IOREG)
    {
        if ((idata & ETH_IOREG_W_UP) && (idata & ETH_IOREG_W_DOWN)) {
            printf("[ eth: ask Schrodinger ]\r\n");
            return FALSE;
        }
        if ((idata & ETH_IOREG_W_UP) && !priv->up)
            eth_up(priv);
        if ((idata & ETH_IOREG_W_DOWN) && priv->up)
            eth_down(priv);
        if (idata & ETH_IOREG_W_ENABLE_INTS)
            priv->enabinterrupts = 1;
        if (idata & ETH_IOREG_W_DISABLE_INTS)
            priv->enabinterrupts = 0;
        if (idata & ETH_IOREG_W_PKT_READ)
            eth_nextpacket(priv);
        if (idata & ETH_IOREG_W_PKT_WRITTEN)
            eth_write(priv, ETH_IOREG_W_PKT_SIZE(idata));
        global_int &= ~(INT_ETH);
        if (priv->packetavail && priv->enabinterrupts)
            global_int |= INT_ETH;
        eval_intr_all();

    } else if (relative_addr == ETH_IOCTL_MIN) {
        return eth_ioctl(priv, idata);
    } else if (relative_addr >= ETH_TXRXBUF_MIN && relative_addr < ETH_TXRXBUF_MAX) {
        priv->txbuf[(relative_addr - ETH_TXRXBUF_MIN) / 4] = idata;
    } else {
        printf("[ eth: unimplemented write to "
            "offset 0x%x data=0x%02llx ]\n", relative_addr, idata);
        return FALSE;
    }

    return TRUE;
}

static t_stat eth_reset(DEVICE *dptr)
{
    struct eth_priv *priv = &priv_instance;
    //struct eth_priv* priv;

    //priv = malloc(sizeof(priv));

    priv->up = 0;
    priv->packetavail = 0;
    priv->enabinterrupts = 0;
    priv->queuelen = 0;

    sim_activate(dptr->units, ETH_POLL_INTVL);

    return SCPE_OK;
}

static t_stat eth_rcv_svc( UNIT *uptr )
{
    struct eth_priv *priv = &priv_instance;

    if (priv->up && priv->fd)
    {
        ethmsg_t emsg;
        struct pollfd pfd;
        pfd.fd = priv->fd;
        pfd.events = POLLIN | POLLPRI;
        pfd.revents = 0;
        while (poll(&pfd, 1, 0))
        {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                printf("[ eth: unexpected down ]\r\n");
                close(priv->fd);
                priv->fd = -1;
                priv->up = 0;
                return SCPE_OK;
            }
            read(priv->fd, &emsg, sizeof(emsg));
            if (emsg.type == ETPACKET)
            {
                eth_putpacket(priv, (uint32*)emsg.payload.ep.data, emsg.payload.ep.size);
                eth_loadpacket(priv);
            } else if (emsg.type == ETIOCTL) {
                printf("[ eth: ioctl in ]\r\n");
            } else {
                printf("[ eth: unknown emsg type %d ]\r\n", emsg.type);
            }
            pfd.revents = 0;
        }
    }
    if (priv->enabinterrupts && priv->packetavail)
    {
        global_int |= INT_ETH;
        eval_intr_all ();
    }

    sim_activate(uptr, ETH_POLL_INTVL);

    return SCPE_OK;
}

static t_stat eth_attach( UNIT *uptr, char *cptr )
{
    static char str[256] = {0};

    strncpy(str, cptr, 255);
    tappath = str;
    printf("[ eth: attach %s]\r\n", cptr);
 
    return SCPE_OK;
}


#endif
