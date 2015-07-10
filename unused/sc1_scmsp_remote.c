#include "sc1_defs.h"
#include "sc1_scmsp.h"
#include "sc1_cac.h"
#include <unistd.h>
#include "sim_fio.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define SCMSP_POLL_INTVL 2000

/* get defs for SysTapAtnMsp register */
#include "sicortex/ice9/ice9_lbs_spec_sw.h"

/* Declarations */


extern CORECTX *cpu_ctx[NUM_CORES];
extern int32 sim_quiet;

/* SCMSP data structures

   scmsp_dib	SCMSP dib
   scmsp_dev	SCMSP device descriptor
   scmsp_unit	SCMSP unit descriptor
   scmsp_reg	SCMSP register list
   
*/

#include "sicortex/ice9_defs.h"
#include "sicortex/msptypes.h"

extern t_bool scmsp_rd( t_uint64 pa, t_uint64 *val, uint32 lnt);
extern t_bool scmsp_wr(t_uint64 pa, t_uint64 val, uint32 lnt);
extern t_stat scmsp_rcv_svc( UNIT *uptr );
extern t_stat scmsp_reset(DEVICE *dptr);
extern void msp_mspwr(uint32 val);
static t_stat msp_sock_attach( UNIT *uptr, char *cptr );

#define MSP_BUFSIZE 128

typedef struct MSP_Port {
	uint32 from_ice9;	   /* data from cores */
	uint32 to_ice9;	   /* data to cores */
	uint32 from_msp;	   /* data from msp (syschain) */
	uint32 to_msp;	   /* data to msp (syschain) */

	uint32 inten;		/* holds interrupt enable mask */
    uint32 prevdata;
	int sockfd;
} MSP_Port_t;

MSP_Port_t msp_port;
MSP_Port_t *msp = &msp_port;

DIB scmsp_dib = { SCMSPBASE, SCMSPEND, &scmsp_rd, &scmsp_wr, 0 };

UNIT scmsp_unit[] = { 
	{ UDATA(&scmsp_rcv_svc, UNIT_ATTABLE, SCMSPSIZE) },
};

REG scmsp_reg[] =
{
	{ HRDATA(from_ice9, msp_port.from_ice9, 32) },
	{ HRDATA(to_ice9, msp_port.to_ice9, 32) },
	{ HRDATA(from_msp, msp_port.from_msp, 32) },
	{ HRDATA(to_msp, msp_port.to_msp, 32) },
	{ NULL }
};

DEVICE scmsp_dev = {
	"SCMSP",		/* name */
	scmsp_unit,		/* units */
	scmsp_reg,		/* registers */
	NULL,			/* modifiers */
	1,			/* #units */
	16,			/* address radix */
	64,			/* address width */
	8,			/* addr increment */
	16,			/* data radix */
	64,			/* data width */
	NULL,			/* examine routine */
	NULL,			/* deposit routine */
	&scmsp_reset,		/* reset routine */
	NULL,			/* boot routine */
	&msp_sock_attach,	/* attach routine */
	NULL,			/* detach routine */
	(void *)&scmsp_dib,	/* context */
	DEV_DIB,		/* flags */
#if 0
	NULL,			/* debug control */
	NULL,			/* debug flags */
	NULL,			/* mem size routine */
	NULL			/* logical name */
#endif
};

static char *msppath = "127.0.0.1";
static int mspport = 7777;

/* returns 0 if new connection is open
 * otherwise returns -errno
 */

int msp_sock_up()
{ 
	struct sockaddr_in sockaddr;
	int i;
        
        printf("[ msp_sock: connecting to MSP on %s:%d ]\r\n", msppath, mspport);
	msp->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (msp->sockfd < 0)
	{
		perror("socket");
		return -1;
	}
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(mspport);
	if (inet_aton(msppath, &sockaddr.sin_addr) == 0)
	{
		perror("inet_aton");
		return -1;
	}
	i = connect(msp->sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
	if (i < 0)
	{
		printf("[ msp_sock: NOT up: %s ]\r\n", msppath);
		perror("connect");
		close(msp->sockfd);
		msp->sockfd = -1;
		return -1;
	}
	msp->prevdata = 0xdeadbeef;
	if(!sim_quiet) printf("[ msp_sock: up ]\r\n");
	return 0;
}

static t_stat msp_sock_attach( UNIT *uptr, char *cptr )
{
	static char str[256] = {0};
	char *s;

	strncpy(str, cptr, 255);
	msppath = str;
	s = strchr(str, ':');
	*s = '\0';
	if (s && s[1])
		mspport = atoi(s+1);
	
	
	return SCPE_OK;
}

t_bool
scmsp_rd( t_uint64 pa, t_uint64 *val, uint32 lnt)
{
    int len;
    if (lnt == L_WORD) len = 4;
    else return FALSE;

    * (uint32 *) val = msp->to_ice9;

    return TRUE;
}

/* write from the ice9 */
t_bool
scmsp_wr(t_uint64 pa, t_uint64 val, uint32 lnt)
{
    int len;
    uint32 temp;
    if (lnt == L_WORD) len = 4;
    else return FALSE;

    msp->from_ice9 = (uint32) val;

    temp = 0;
#ifdef SCX_PROD_BASE_ICE9A
    temp |= SETFIELD(ICE9_SysTapAtnMsp_RecvAtn,
		     GETFIELD(ICE9_SysTapAtnMsp_RecvAtn, msp->from_msp));
#else
    temp |= SETFIELD(ICE9_SysTapAtnMsp_TxAtnMask,
		     GETFIELD(ICE9_SysTapAtnMsp_TxAtnMask, msp->from_msp));
#endif
    /* RW1CS.  The MSP's recv valid bit is SET by a write from ice9
     * otherwise it is HOLD.  The CLEAR is in the write from MSP
     */
    if (GETFIELD(ICE9_ScbAtnChip_SendVld, msp->from_ice9)) {
	temp |= SETFIELD(ICE9_SysTapAtnMsp_RecvVld, 1);
	temp |= SETFIELD(ICE9_SysTapAtnMsp_RecvData,
			 GETFIELD(ICE9_ScbAtnChip_SendData, msp->from_ice9));
    } else {
	temp |= SETFIELD(ICE9_SysTapAtnMsp_RecvVld,
			 GETFIELD(ICE9_SysTapAtnMsp_RecvVld, msp->to_msp));
	temp |= SETFIELD(ICE9_SysTapAtnMsp_RecvData,
			 GETFIELD(ICE9_SysTapAtnMsp_RecvData, msp->to_msp));
    }
    /* The MSP's Send Valid bit is SET by an MSP write, and CLEARED
     * by a write from the ICE9 (here) otherwise HOLD
     */
    if (GETFIELD(ICE9_ScbAtnChip_RecvTaken, msp->from_ice9)) {
	temp &= ~ICE9_SysTapAtnMsp_SendVld_MASK;
	msp->to_ice9 &= ~ICE9_SysTapAtnMsp_RecvVld_MASK;
    }
    else
	temp |= SETFIELD(ICE9_SysTapAtnMsp_SendVld, 
			 GETFIELD(ICE9_SysTapAtnMsp_SendVld, msp->to_msp));
    msp->to_msp = temp;
    /* now copy the RecvInt bit from from_ice9 to to_ice9 */
    msp->to_ice9 = (msp->to_ice9 & ~ICE9_ScbAtnChip_RecvInt_MASK)
	| (msp->from_ice9 & ICE9_ScbAtnChip_RecvInt_MASK);
    /* the ice9 SendVld bit is the same as the MSP RecvVld bit. It is
     * cleared by a later write to RecvTaken from msp */
    msp->to_ice9 = (msp->to_ice9 & ~ICE9_ScbAtnChip_SendVld_MASK)
	| SETFIELD(ICE9_ScbAtnChip_SendVld,
		   GETFIELD(ICE9_SysTapAtnMsp_RecvVld, msp->to_msp));
    
    
    /* cause an interrupt if one is enabled */
    if (GETFIELD(ICE9_ScbAtnChip_RecvInt, msp->to_ice9) &&
	GETFIELD(ICE9_ScbAtnChip_RecvVld, msp->to_ice9)) {
	/* cause an interrupt */
	/* XXX is this also masked in SCB control registers? */
	cac_set_slow(ICE9_CacxSlIntStat_SCBSlInt_MASK);
    } else
        cac_clr_slow(ICE9_CacxSlIntStat_SCBSlInt_MASK);
/*
    if (GETFIELD(ICE9_SysTapAtnMsp_RecvVld, msp->to_msp))
	printf("scmsp:%d to_msp %x %x %x %x\r\n", 
	       __LINE__,
	       msp->to_msp,
	       GETFIELD(MSP_LINK_TAG, msp->to_msp),
	       GETFIELD(MSP_LINK_FD, msp->to_msp),
	       GETFIELD(MSP_LINK_DATA, msp->to_msp));
*/
    return TRUE;
}

/* write from the msp */
void msp_mspwr(uint32 val)
{
    uint32 temp;
    msp->from_msp = val;
    temp = 0;
    /* RecvInt bit is read write, so copy from from_ice9 */
    temp |= SETFIELD(ICE9_ScbAtnChip_RecvInt,
		     GETFIELD(ICE9_ScbAtnChip_RecvInt, msp->from_ice9));
    /* recv taken always reads as 0 */
    /* RecvVld is SET by this write, otherwise HOLD
     * the CLEAR is by ICE9 write
     */
    if (
#ifdef SCX_PROD_BASE_ICE9A
	GETFIELD(ICE9_SysTapAtnMsp_SendVld, msp->from_msp)
#else
	GETFIELD(ICE9_SysTapAtnMsp_SendReq, msp->from_msp)
#endif
	) {
	/* ice9 data comes from MSP */
	temp |= SETFIELD(ICE9_ScbAtnChip_RecvVld, 1);
	temp |= SETFIELD(ICE9_ScbAtnChip_RecvData,
		    GETFIELD(ICE9_SysTapAtnMsp_SendData, msp->from_msp));
    } else {
	temp |= SETFIELD(ICE9_ScbAtnChip_RecvVld,
			 GETFIELD(ICE9_ScbAtnChip_RecvVld, msp->to_ice9));
	temp |= SETFIELD(ICE9_ScbAtnChip_RecvData,
		    GETFIELD(ICE9_ScbAtnChip_RecvData, msp->to_ice9));
    }
    /* ICE9's sendvld is CLEARED by a write here, and SET by an ICE9 write
     * otherwise HOLD
     * MSP's recvvld is also cleared
     */
    if (GETFIELD(ICE9_SysTapAtnMsp_RecvTaken, msp->from_msp)) {
	temp |= SETFIELD(ICE9_ScbAtnChip_SendVld, 0);
	msp->to_msp &= ~ICE9_SysTapAtnMsp_RecvVld_MASK;
    }
    else
	temp |= SETFIELD(ICE9_ScbAtnChip_SendVld,
			 GETFIELD(ICE9_ScbAtnChip_SendVld, msp->to_ice9));
    msp->to_ice9 = temp;
    /* now copy the RecvAtn bit from from_msp to to_msp */
#ifdef SCX_PROD_BASE_ICE9A
    msp->to_msp = (msp->to_msp & ~ICE9_SysTapAtnMsp_RecvAtn_MASK)
	| (msp->from_msp & ICE9_SysTapAtnMsp_RecvAtn_MASK);
#else
    msp->to_msp = (msp->to_msp & ~ICE9_SysTapAtnMsp_TxAtnMask_MASK)
	| (msp->from_msp & ICE9_SysTapAtnMsp_TxAtnMask_MASK);
#endif
    /* the msp SendVld bit is the same as the ice9 RecvVld bit. It is
     * cleared by a later write to RecvTaken from ice9 */
    msp->to_msp = (msp->to_msp & ~ICE9_SysTapAtnMsp_SendVld_MASK)
	| SETFIELD(ICE9_SysTapAtnMsp_SendVld,
		   GETFIELD(ICE9_ScbAtnChip_RecvVld, msp->to_ice9));

    /* cause an interrupt if one is enabled */
    if (GETFIELD(ICE9_ScbAtnChip_RecvInt, msp->to_ice9) &&
	GETFIELD(ICE9_ScbAtnChip_RecvVld, msp->to_ice9)) {
	/* cause an interrupt */
	/* XXX is this also masked in SCB control registers? */
	cac_set_slow(ICE9_CacxSlIntStat_SCBSlInt_MASK);
    } else
        cac_clr_slow(ICE9_CacxSlIntStat_SCBSlInt_MASK);
/*
    if (GETFIELD(ICE9_ScbAtnChip_RecvVld, msp->to_ice9))
               printf("scmsp:%d  to_ice9 %x %x %x %x\r\n", 
        	__LINE__,
	       msp->to_ice9,
	       GETFIELD(MSP_LINK_TAG, msp->to_ice9),
	       GETFIELD(MSP_LINK_FD, msp->to_ice9),
	       GETFIELD(MSP_LINK_DATA, msp->to_ice9));
*/
}


/*
 *      SCMSP reset
 */

void msp_init()
{
    /* init spinlock  here */
    msp->inten = 0;
    msp->sockfd = -1;
}

t_stat
scmsp_reset(DEVICE *dptr)
{
    msp->to_ice9 = 0;
    msp->from_ice9 = 0;
    msp->prevdata = 0xdeadbeef;
    msp_init();
  sim_activate(dptr->units, SCMSP_POLL_INTVL);
  return SCPE_OK;
}




/*
 *      SCMSP Recieve service
 */

t_stat
scmsp_rcv_svc( UNIT *uptr )
{
    if (msp->sockfd < 0)
	msp_sock_up();
    if (msp->sockfd >= 0)
	{
	    int ret;
	    uint32 foo[2];
	    	
	    while ((ret = recv(msp->sockfd, foo, 8, MSG_DONTWAIT)) != -1)
	    	{
		    if (ret == 0)
		    	{
			    close(msp->sockfd);
			    printf("[ msp_sock: down ] \n");
			    msp->sockfd = -1;
			    break;
		    	} else if (ret < 8) {
			    printf("[ msp_sock: short read ]\n");
			} else {
			    foo[0] = ntohl(foo[0]);
			    foo[1] = ntohl(foo[1]);
			    if (foo[0] == 0xfeed1111) {
				uint32_t outdata = htonl(msp->to_msp);
/*				printf("<-ice9 %0lx\r\n", (unsigned long) msp->to_msp); */
				if (send(msp->sockfd, &outdata, 4, 0) != 4)
				    perror("send");
			    } else if (foo[0] == 0xdeadbeef) {
/*				printf("->ice9 %08lx\r\n", (unsigned long) foo[1]); */
				msp_mspwr(foo[1]);
			    } else {
				printf("msp rcv got %0lx %0lx\r\n", (unsigned long) foo[0], (unsigned long) foo[1]);
			    }
			}
		}
	    if ((errno != EAGAIN) && (ret != 0))
		{
		    perror("recv");
		    printf("\r\n");
		    close(msp->sockfd);
		    msp->sockfd = -1;
		}
	}
    sim_activate(uptr, SCMSP_POLL_INTVL);

    return SCPE_OK;
}



