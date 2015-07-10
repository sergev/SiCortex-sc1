#include "sc1_defs.h"
#include "sc1_scmsp.h"
#include "sc1_cac.h"
#include <unistd.h>
#include "sim_fio.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include "tapd.h"
#include "sc1_eth.h"
#include "simple_socket.h"

#define SCMSP_POLL_INTVL 20000

int something_received = 0;
int nop_received = 0;

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

void scmsp_progress();
extern t_bool scmsp_rd( t_uint64 pa, t_uint64 *val, uint32 lnt);
extern t_bool scmsp_wr(t_uint64 pa, t_uint64 val, uint32 lnt);
extern t_stat scmsp_rcv_svc( UNIT *uptr );
extern t_stat scmsp_reset(DEVICE *dptr);
extern void msp_mspwr(uint32 val);
static t_stat msp_tap_attach( UNIT *uptr, char *cptr );


#define MSP_MAX_FD 31
#define MSP_BUFSIZE 128

typedef struct MSP_Message {
    int signal;
    int command;
    int fd;
    int length;
    int code;
    int pos;
    char *buf;
    struct MSP_Message *next;
} MSP_Message_t;

typedef struct MSP_Queue {
    MSP_Message_t *head;
    MSP_Message_t *tail;
} MSP_Queue_t;

MSP_Message_t *msp_dequeue(MSP_Queue_t *q)
{
    MSP_Message_t *ob = q->head;
    if (!ob)
        return NULL;	/* I got nothin' */
    q->head = ob->next;
    if (q->tail == ob) q->tail = NULL;
    ob->next = NULL;
    return(ob);
}

void msp_enqueue(MSP_Queue_t *q, MSP_Message_t *ob)
{
    ob->next = NULL;
    if (q->tail) q->tail->next = ob;
    q->tail = ob;
    if (q->head == NULL) q->head = ob;
}

/* There is one ring, which is used for console traffic */
typedef struct MSP_Ring {
    uint32 tx_buf[MSP_BUFSIZE];
    int tx_write;
    int tx_read;
    uint32 rx_buf[MSP_BUFSIZE];
    int rx_write;
    int rx_read;
} MSP_Ring_t;

/* each fd is used for a file or other stateful command */
typedef struct MSP_Fd {
    FILE *extern_fd;	   /* host environment fd */
} MSP_Fd_t;


typedef struct MSP_Port {
    uint32 from_ice9;	   /* data from cores */
    uint32 to_ice9;	   /* data to cores */
    uint32 from_msp;	   /* data from msp (syschain) */
    uint32 to_msp;	   /* data to msp (syschain) */

    uint32 inten;        /* holds interrupt enable mask */
    
    int tapfd;

    MSP_Queue_t outqueue;

    MSP_Ring_t ring;
    MSP_Fd_t fd[MSP_MAX_FD];
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





/*
 * Console Socket
 */

#define MSPSOCK_START_PORT 9020
#define MSPSOCK_PORT_ATTEMPTS 20
struct simple_socket_priv msp_socket_priv = {
    state:        SIMPSOCK_START,
    name:         "msp",
    socket:       0,
    socket_wait:  0,
    startport:    MSPSOCK_START_PORT,
    portno:       0,
    portattempts: MSPSOCK_PORT_ATTEMPTS,
    sockfd:       0,
    newsockfd:    0};
#define MSPSOCK_KGDB_START_PORT 9040
#define MSPSOCK_KGDB_PORT_ATTEMPTS 20
struct simple_socket_priv mspkgdb_socket_priv = {
    state:        SIMPSOCK_START,
    name:         "mspkgdb",
    socket:       0,
    socket_wait:  0,
    startport:    MSPSOCK_KGDB_START_PORT,
    portno:       0,
    portattempts: MSPSOCK_KGDB_PORT_ATTEMPTS,
    sockfd:       0,
    newsockfd:    0};

t_stat msp_socket_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &msp_socket_priv;
    char *end;
    if (cptr == NULL) {
        p->socket = 1; 
    } else {
        p->socket = (int32) strtol (cptr, &end, 10);
        if ((end == cptr) || (p->socket < 0) || (p->socket > 65535)) {
            p->socket = 0;
            return SCPE_ARG;
        }
    }
    printf("set socket to %d\n", p->socket);
    return SCPE_OK;
}

t_stat msp_socket_wait_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &msp_socket_priv;
    char *end;
    if (cptr == NULL) {
        p->socket_wait = 1; 
    } else {
        p->socket_wait = (int32) strtol (cptr, &end, 10);
        if ((end == cptr) || (p->socket_wait < -1)) {
            p->socket_wait = 0;
            return SCPE_ARG;
        }
    }
    printf("set socket_wait to %d\n", p->socket_wait);
    return SCPE_OK;
}

t_stat mspkgdb_socket_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &mspkgdb_socket_priv;
    char *end;
    if (cptr == NULL) {
        p->socket = 1; 
    } else {
        p->socket = (int32) strtol (cptr, &end, 10);
        if ((end == cptr) || (p->socket < 0) || (p->socket > 65535)) {
            p->socket = 0;
            return SCPE_ARG;
        }
    }
    printf("set kgdb_socket to %d\n", p->socket);
    return SCPE_OK;
}

t_stat mspkgdb_socket_wait_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &mspkgdb_socket_priv;
    char *end;
    if (cptr == NULL) {
        p->socket_wait = 1; 
    } else {
        p->socket_wait = (int32) strtol (cptr, &end, 10);
        if ((end == cptr) || (p->socket_wait < -1)) {
            p->socket_wait = 0;
            return SCPE_ARG;
        }
    }
    printf("set kgdb_socket_wait to %d\n", p->socket_wait);
    return SCPE_OK;
}


MTAB msp_mod[] = {
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "SOCKET",    "SOCKET",
        &msp_socket_set, NULL},
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0,    "SOCKET_WAIT", "SOCKET_WAIT",
        &msp_socket_wait_set, NULL},
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "KGDB_SOCKET",    "KGDB_SOCKET",
        &mspkgdb_socket_set, NULL},
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0,    "KGDB_SOCKET_WAIT", "KGDB_SOCKET_WAIT",
        &mspkgdb_socket_wait_set, NULL},
    { 0 }
    };


DEVICE scmsp_dev = {
    "SCMSP",            /* name */
    scmsp_unit,         /* units */
    scmsp_reg,          /* registers */
    msp_mod,            /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    64,                 /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &scmsp_reset,       /* reset routine */
    NULL,               /* boot routine */
    &msp_tap_attach,    /* attach routine */
    NULL,               /* detach routine */
    (void *)&scmsp_dib, /* context */
    DEV_DIB,            /* flags */
#if 0
    NULL,               /* debug control */
    NULL,               /* debug flags */
    NULL,               /* mem size routine */
    NULL                /* logical name */
#endif
};

#ifdef USE_TAP
static char *tappath = "/dev/tap0";

static int msp_tap_up()
{ 
        struct sockaddr_un sockaddr;
        int i;
        
        msp->tapfd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (msp->tapfd < 0)
        {
                perror("socket");
                return -1;
        }
        sockaddr.sun_family = AF_UNIX;
        strcpy(sockaddr.sun_path, tappath);
        i = connect(msp->tapfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
        if (i < 0)
        {
                printf("[ msp_tap: NOT up %s ]\r\n", tappath);
                perror("connect");
                close(msp->tapfd);
                msp->tapfd = -1;
                return -1;
        }

        if(!sim_quiet) printf("[ msp_tap: up ]\r\n");
        return 0;
}
#endif

static t_stat msp_tap_attach( UNIT *uptr, char *cptr )
{
#if USE_TAP
    static char str[256] = {0};

    strncpy(str, cptr, 255);
    tappath = str;
    printf("[ mspeth: attach %s]\r\n", cptr);

#endif
    return SCPE_OK;
}

void msp_putchar(int32 c, int32 cpu)
{
   int io_status=0;
   char ochar = c;
   sim_os_putchar(c, cpu);
   do_simple_socket(&msp_socket_priv, SIMPSOCK_WRITE, &ochar, &io_status); 
}

/* msp_do_cmd is called with complete commands */
void msp_do_signal(MSP_Message_t *cmd)
{
    switch (cmd->command) {
    case MSP_CMD_DIAG:
	if (cmd->fd == MSP_DIAG_ECHO) {
	    MSP_Message_t *reply = (MSP_Message_t*)malloc(sizeof(MSP_Message_t));
	    reply->command = MSP_CMD_DIAG;
	    reply->fd = MSP_DIAG_ECHO_REPLY;
	    reply->signal = 1;
	    reply->code = cmd->code;
	    msp_enqueue(&msp->outqueue, reply);

	    /* xxx send echo reply */
	}
        printf("scmsp:do_signal:%d  DIAG:cmd %x type %x data %x\r\n", __LINE__,
               cmd->command, cmd->fd, cmd->code);
	break;
#ifdef USE_TAP
    case MSP_CMD_NET:
	if (cmd->fd == MSP_NET_SIOCGIFHWADDR) {
                ethmsg_t emsg;
                if (msp->tapfd < 0)
                    if (msp_tap_up())
                        break;
                emsg.type = ETIOCTL;
                emsg.payload.ei.cmd_ret = SIOCGIFHWADDR;
                write(msp->tapfd, &emsg, sizeof(emsg));
                break;
	} else {
                printf("scmsp:msp_net  :%d unsupported ioctl %08x\r\n", __LINE__, cmd->fd);
                break;
	}
	break;
#endif
    case MSP_CMD_CONSOLE:
        switch (cmd->fd) {
        case MSP_CONSOLE_STDOUT:
        case MSP_CONSOLE_STDERR:
            if ((cmd->code & 0xFF) == '\n')
                msp_putchar('\r', (cmd->code >> 8) & 0xFF);
            msp_putchar(cmd->code & 0xFF, (cmd->code >> 8) & 0xFF);
            break;
        case MSP_CONSOLE_KGDB: {
	   int io_status=0;
	   char ochar = cmd->code & 0xFF;
	   do_simple_socket(&mspkgdb_socket_priv, SIMPSOCK_WRITE, 
			    &ochar, &io_status);
            break;
	}
        default:
            printf("scmsp:do_signal:%d cmd %x type %x data %x\r\n", __LINE__,
        	   cmd->command, cmd->fd, cmd->code);
            break;
        }
        break;
    default:
        printf("scmsp:do_signal:%d cmd %x type %x data %x\r\n", __LINE__,
               cmd->command, cmd->fd, cmd->code);
        break;
    }
}

/* msp_do_cmd is called with complete commands */
void msp_do_cmd(MSP_Message_t *cmd)
{
    MSP_Fd_t *f;
    MSP_Message_t *reply = (MSP_Message_t*)malloc(sizeof(MSP_Message_t));
    f = &msp->fd[cmd->fd]; 
    reply->fd = cmd->fd;
    reply->signal = 0;
    reply->pos = 0;	/* Use this to inform ourselves later whether we actually generated a reply */
    switch (cmd->command) {

#ifdef USE_TAP
    case MSP_CMD_NET: {
	if (cmd->fd == MSP_NET_PKT) {
            ethmsg_t emsg;
            
            if (msp->tapfd < 0)
                if (msp_tap_up())
                    goto bailout;
            emsg.type = ETPACKET;
            emsg.payload.ep.size = cmd->length;
            memcpy(emsg.payload.ep.data, cmd->buf, (ETH_FRAME_LEN < cmd->length) ? ETH_FRAME_LEN : cmd->length);
/*	    printf(" mt %d\r\n", cmd->length); */
            write(msp->tapfd, &emsg, sizeof(emsg));
        } else {
            printf("scmsp:msp_net  :%d unknown fd %d\r\n", __LINE__, cmd->fd);
        }
        break;
    }
#endif
    default:
        printf("scmsp:do_cmd   :%d cmd %x type %x data %x\r\n", __LINE__,
	       cmd->command, cmd->fd, cmd->code);
    }
bailout:
    if (reply->pos == 0)
        free(reply);
}

void msp_free_msg(MSP_Message_t *cmd)
{
    if (cmd->buf) {
	free(cmd->buf);
	cmd->buf = NULL;
    }
    free(cmd);
}

void msp_process_input(uint32 odata)
{
    int tag = GETFIELD(MSP_LINK_TAG, odata);
    static MSP_Message_t *cmd;
    MSP_Message_t *quickcmd;

    /* MSP_TAG_CMD can be handled immediately */
    if (tag == MSP_TAG_CMD) {
	int fd = GETFIELD(MSP_LINK_FD, odata);
	nop_received =  (fd == MSP_CMD_NOP);
	if (nop_received) return;
        quickcmd = (MSP_Message_t*)malloc(sizeof(MSP_Message_t));
        quickcmd->signal = 1;
	quickcmd->command = GETFIELD(MSP_LINK_CMD, odata);
	quickcmd->fd = fd;
	quickcmd->code = GETFIELD(MSP_LINK_LENGTH, odata);
	quickcmd->length = 0;
	quickcmd->buf = NULL;
	msp_do_signal(quickcmd);
	msp_free_msg(quickcmd);
	quickcmd = NULL;
    } else if (tag == MSP_TAG_DATA) {
	if (!cmd) {
	    printf("scmsp:%d data %x received outside block\r\n", __LINE__, odata);
	    return;
	}
	if (cmd->pos < cmd->length) {
	    cmd->buf[cmd->pos] = odata;
	    cmd->pos += 1;
	}
	if (cmd->pos < cmd->length) {
	    cmd->buf[cmd->pos] = odata >> 8;
	    cmd->pos += 1;
	}
	if (cmd->pos < cmd->length) {
	    cmd->buf[cmd->pos] = odata >> 16;
	    cmd->pos += 1;
	}
	if (cmd->pos >= cmd->length) {
	    msp_do_cmd(cmd);
	    msp_free_msg(cmd);
	    cmd = NULL;
	}
    } else if (tag == MSP_TAG_CMD_MULTI) {
	if (cmd) {
	    printf("scmsp:%d cmd_multi %x received inside block\r\n", __LINE__, odata);
	    msp_free_msg(cmd);
	    cmd = NULL;
	}
        cmd = (MSP_Message_t*)malloc(sizeof(MSP_Message_t));
        cmd->signal = 0;
	cmd->command = GETFIELD(MSP_LINK_CMD, odata);
	cmd->fd = GETFIELD(MSP_LINK_FD, odata);
	cmd->length = GETFIELD(MSP_LINK_LENGTH, odata);
	if (cmd->length > 0)
	    cmd->buf = (char *) malloc(cmd->length);
	cmd->pos = 0;
	if (cmd->length ==0)
	{
	    msp_do_cmd(cmd);
	    msp_free_msg(cmd);
	    cmd = NULL;
        }
    }
}

uint32 msp_process_output()
{
    /* see if there is anything to send to the msp */
    static MSP_Message_t *r = NULL;
    uint32 res = 0;

    if (something_received && !nop_received) res = MSP_LINK_CMD_WORD(MSP_CMD_NOP, 0, 0);
    if (!r)
        r = msp_dequeue(&msp->outqueue);
    if (!r) /* still no? */
        return 0;
    
    if (r->signal) {
	res = SETFIELD(ICE9_SysTapAtnMsp_SendVld, 1)  // ICE9A
	    | SETFIELD(ICE9_SysTapAtnMsp_SendReq, 1)  // ICE9B+
	    | SETFIELD(ICE9_SysTapAtnMsp_SendData,
		       MSP_LINK_CMD_WORD(r->command, r->fd, r->code));
        msp_free_msg(r);
        r = NULL;
	return (res);
    }

    if ((r->pos == -2) || (r->pos == -3)) {
        fflush(stdout);
        /* send the command word */
	res = SETFIELD(ICE9_SysTapAtnMsp_SendVld, 1) // ICE9A
	    | SETFIELD(ICE9_SysTapAtnMsp_SendReq, 1) // ICE9B+
	     | SETFIELD(ICE9_SysTapAtnMsp_SendData,
	                MSP_LINK_CMD_MULTI(r->command, r->fd, r->length));
        fflush(stdout);
        if (r->pos == -2)
	    r->pos = -1;
        else
            r->pos = 0;
    } else if (r->pos == -1) {
        fflush(stdout);
        
        res = SETFIELD(ICE9_SysTapAtnMsp_SendVld, 1) // ICE9A
	    | SETFIELD(ICE9_SysTapAtnMsp_SendReq, 1) // ICE9B+
            | SETFIELD(ICE9_SysTapAtnMsp_SendData,
                        MSP_LINK_DATA_WORD(r->code));
	    
        r->pos = 0;
        if (r->length == 0) {
            msp_free_msg(r);
            r = NULL;
        }
    } else {
        fflush(stdout);
        res = 0xDEADFF;
        
        if (r->pos < r->length) res  = (r->buf[r->pos++] & 0xFF) << 0;
        if (r->pos < r->length) res |= (r->buf[r->pos++] & 0xFF) << 8;
        if (r->pos < r->length) res |= (r->buf[r->pos++] & 0xFF) << 16;
        
        res = SETFIELD(ICE9_SysTapAtnMsp_SendVld, 1) // ICE9A
	    | SETFIELD(ICE9_SysTapAtnMsp_SendReq, 1) // ICE9B+
            | SETFIELD(ICE9_SysTapAtnMsp_SendData,
                        MSP_LINK_DATA_WORD(res));
        fflush(stdout);
        
        if (r->pos == r->length) {
            fflush(stdout);
            msp_free_msg(r);
            r = NULL;
        }
    }
    return(res);
}


void scmsp_progress()
{
    MSP_Port_t *m = msp;
    uint32 odata = m->to_msp;
    int tag, cmd, signal;
    uint32 ndata = m->inten;
    
    if (GETFIELD(ICE9_SysTapAtnMsp_RecvVld, odata)) {
	something_received = 1;
	tag = GETFIELD(MSP_LINK_TAG, odata);
	cmd = GETFIELD(MSP_LINK_CMD, odata);
	signal = GETFIELD(MSP_LINK_FD, odata);
	msp_process_input(odata);
	ndata |= ICE9_SysTapAtnMsp_RecvTaken_MASK;
    } else something_received = 0;
    if (GETFIELD(ICE9_SysTapAtnMsp_SendVld, odata) == 0) {
	if (m->ring.tx_write != m->ring.tx_read) {
	    ndata |= SETFIELD(ICE9_SysTapAtnMsp_SendVld, 1) // ICE9A
		|    SETFIELD(ICE9_SysTapAtnMsp_SendReq, 1) // ICE9B
		| m->ring.tx_buf[m->ring.tx_read];
	    m->ring.tx_read = (m->ring.tx_read + 1) % MSP_BUFSIZE;
	} else {
	    ndata |= msp_process_output();
	}
    }
    if (ndata != m->inten) {
	msp_mspwr(ndata);
    }
}

int scmsp_ring_tx_not_full(MSP_Ring_t *r)
{
    int updated_write = (r->tx_write + 1) % MSP_BUFSIZE;
    return (r->tx_read != updated_write);
}

int scmsp_ring_tx_not_empty(MSP_Ring_t *r)
{
    return (r->tx_read != r->tx_write);
}

void scmsp_ring_tx_push(MSP_Ring_t *r, uint32 v)
{
    if (scmsp_ring_tx_not_full(r)) {
	r->tx_buf[r->tx_write] = v;
	r->tx_write = (r->tx_write + 1) % MSP_BUFSIZE;
    }
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
    
    scmsp_progress();
    
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
    int i;
    MSP_Fd_t *f;
    /* init spinlock  here */
    msp->outqueue.head = NULL;
    msp->outqueue.tail = NULL;
    msp->ring.tx_write = 0;
    msp->ring.tx_read = 0;
    msp->ring.rx_write = 0;
    msp->ring.rx_read = 0;
    msp->inten = 0;
    msp->tapfd = -1;
    for (i = 0; i < MSP_MAX_FD; i += 1) {
	f = &msp->fd[i];
	if (f->extern_fd != NULL) fclose(f->extern_fd);
	f->extern_fd = NULL;
    }
}

t_stat
scmsp_reset(DEVICE *dptr)
{
    msp->to_ice9 = 0;
    msp->from_ice9 = 0;
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
#ifdef USE_TAP
    if (msp->tapfd >= 0)
    {
        ethmsg_t emsg;
        struct pollfd pfd;
        pfd.fd = msp->tapfd;
        pfd.events = POLLIN | POLLPRI;
        pfd.revents = 0;
        while (poll(&pfd, 1, 0))
        {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                printf("[ msp_tap: unexpected down ]\r\n");
                close(msp->tapfd);
                msp->tapfd = -1;
                break;
            }
            read(msp->tapfd, &emsg, sizeof(emsg));
            if (emsg.type == ETPACKET)
            {
                MSP_Message_t *msg;
                
                msg = (MSP_Message_t *) malloc(sizeof(MSP_Message_t));
                msg->signal = 0;
                msg->command = MSP_CMD_NET;
                msg->fd = MSP_NET_PKT;
                msg->length = emsg.payload.ep.size;
                msg->pos = -3;
                msg->buf = (char*)malloc(emsg.payload.ep.size);
                memcpy(msg->buf, emsg.payload.ep.data, emsg.payload.ep.size);
                msp_enqueue(&msp->outqueue, msg);
            } else if (emsg.type == ETIOCTL) {
/*                printf("[ msp_tap: ioctl in ]\r\n"); */
		/* XXX really should check for fullness here */

		scmsp_ring_tx_push(&msp->ring, MSP_LINK_CMD_WORD(
				       MSP_CMD_NET,
				       MSP_NET_MAC_01,
				       (emsg.payload.ei.hwaddr[0] & 0xff)
				       | ((emsg.payload.ei.hwaddr[1] & 0xff) << 8)
				       ));
		scmsp_ring_tx_push(&msp->ring, MSP_LINK_CMD_WORD(
				       MSP_CMD_NET,
				       MSP_NET_MAC_23,
				       (emsg.payload.ei.hwaddr[2] & 0xff)
				       | ((emsg.payload.ei.hwaddr[3] & 0xff) << 8)
				       ));
		scmsp_ring_tx_push(&msp->ring, MSP_LINK_CMD_WORD(
				       MSP_CMD_NET,
				       MSP_NET_MAC_45,
				       (emsg.payload.ei.hwaddr[4] & 0xff)
				       | ((emsg.payload.ei.hwaddr[5] & 0xff) << 8)
				       ));
            } else {
                printf("[ msp_tap: unknown emsg type %d ]\r\n", emsg.type);
            }
            pfd.revents = 0;
        }
    }
#endif
    {
	int io_status = 0;
	char c;
	if (scmsp_ring_tx_not_full(&msp->ring)) {
	    do_simple_socket(&msp_socket_priv, SIMPSOCK_READ, &c, &io_status);
	    if (io_status == 1) {
		scmsp_ring_tx_push(&msp->ring, MSP_LINK_CMD_WORD(
				       MSP_CMD_CONSOLE,
				       MSP_CONSOLE_STDIN,
				       c));
	    }
	}
	if (scmsp_ring_tx_not_full(&msp->ring)) {
	    do_simple_socket(&mspkgdb_socket_priv, SIMPSOCK_READ, &c, &io_status);
	    if (io_status == 1) {
		if ((c & 0x7f) == 0x3) {
		    int cc;
		    for (cc = 0; cc < NUM_CORES; cc++) 
			cpu_ctx[cc]->events |= EVT_DINT;
		}
		scmsp_ring_tx_push(&msp->ring, MSP_LINK_CMD_WORD(
				       MSP_CMD_CONSOLE,
				       MSP_CONSOLE_KGDB,
				       c));
	    }
	}
    }
    scmsp_progress();
    sim_activate(uptr, SCMSP_POLL_INTVL);

    return SCPE_OK;
}



