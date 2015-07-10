/* sc1_uart.c - SiCortex 1 UART simulator

   Copyright (c) 2006, SiCortex, Inc.  All rights reserved.

   Based on SIMH; SIMH copyrights attached.

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   uart         16550 compatible UART

   08-May-06    RMS     Added test to prevent duplicate call on FIFO service
   26-Jan-06    RMS     Added disable to UART receive side (only)
   09-Sep-05    RMS     Int ID<7:6> are MBO
                        Transmit interrupt cleared only if recognized
                        Transmit interrupt sets on FIFO empty, not xbuf empty
                        Receive status interrupt cleared by reading LSTA
                        Receive queue status walk not incrementing pointer
                        Preserve most recent IID and fctl write for debugging
                        Call local interrupt eval on transmit service
                        Added receive FIFO timeout interrupt
   29-Aug-05    RMS     Added DLAB bit
*/

#include "sc1_defs.h"
#include "sc1_cac.h"
#include "sc1_uart.h"
#include "simple_socket.h"

#define UNIT_V_8B       (UNIT_V_UF + 0)                 /* 8B */
#define UNIT_8B         (1 << UNIT_V_8B)

uint32 uart_rcv_ip = 0;
uint32 uart_rcv_rp = 0;
uint32 uart_rcv_cnt = 0;
uint32 uart_xmt_ip = 0;
uint32 uart_xmt_rp = 0;
uint32 uart_xmt_cnt = 0;
uint32 uart_xbuf = 0;
uint32 uart_xbuf_full = 0;
uint32 uart_lsta = 0;
uint32 uart_mctl = 0;
uint32 uart_msta = 0;
uint32 uart_lctl = 0;
uint32 uart_fctl = 0;
uint32 uart_ie = 0;
uint32 uart_ista = 0;
uint32 uart_rcv_alarm = 1;
uint32 uart_dl_lo = 0;
uint32 uart_dl_hi = 0;
uint32 uart_iid = 0;
uint32 uart_rcv_tmo = 0;
uint16 uart_rcv_fifo[UART_FIFO_SIZE];
uint16 uart_xmt_fifo[UART_FIFO_SIZE];

static uint8 odd_par[128] = {
 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80,        /* 000-017 */
 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0,        /* 020-037 */
 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0,        /* 040-067 */
 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80,        /* 060-077 */
 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0,        /* 100-117 */
 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80,        /* 120-137 */
 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0x80,        /* 140-157 */
 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0         /* 160-177 */
 };

static uint32 rcv_alarm[4] = { 1, 4, 8, 14 };

t_bool uart_rd (t_uint64 pa, t_uint64 *val, uint32 lnt);
t_bool uart_wr (t_uint64 pa, t_uint64 val, uint32 lnt);
t_stat uart_reset (DEVICE *dptr);
t_stat uart_rcv_svc (UNIT *uptr);
t_stat uart_xmt_svc (UNIT *uptr);
t_stat uart_fifo_svc (UNIT *uptr);
void uart_eval_torq (void);
t_stat uart_set_opt (UNIT *uptr, int32 val, char *cptr, void *desc);
void uart_eval_intr (void);
uint32 uart_calc_par (uint32 c, uint32 lc);

/* UART data structures

   uart_dev     UART device descriptor
   uart_unit    UART unit descriptor
   uart_reg     UART register list
   uart_mod     UART modifiers list
*/

#define UART_RCV        0
#define UART_XMT        1
#define UART_FIFO       2

DIB uart_dib = { UART_BASE, UART_END, &uart_rd, &uart_wr, 0 };

UNIT uart_unit[] = {
    { UDATA (&uart_rcv_svc, UNIT_DISABLE, 0), KBD_POLL_WAIT },
    { UDATA (&uart_xmt_svc, 0, 0), SERIAL_OUT_WAIT },
    { UDATA (&uart_fifo_svc, UNIT_DIS, 0), UART_FIFO_WAIT }
    };

REG uart_reg[] = {
    { HRDATA (LCTL, uart_lctl, 8) },
    { HRDATA (LSTA, uart_lsta, 8) },
    { HRDATA (MCTL, uart_mctl, 8) },
    { HRDATA (FCTL, uart_fctl, 8) },
    { HRDATA (MSTA, uart_msta, 8) },
    { HRDATA (IE, uart_ie, 8) },
    { HRDATA (ISTA, uart_ista, 8) },
    { HRDATA (IID, uart_iid, 8), REG_RO },
    { HRDATA (DLHI, uart_dl_hi, 8) },
    { HRDATA (DLLO, uart_dl_lo, 8) },
    { DRDATA (RTMO, uart_rcv_tmo, 3) },
    { BRDATA (RFIFO, uart_rcv_fifo, 16, 16, UART_FIFO_SIZE) },
    { BRDATA (XFIFO, uart_xmt_fifo, 16, 16, UART_FIFO_SIZE) },
    { HRDATA (RIP, uart_rcv_ip, 4) },
    { HRDATA (RRP, uart_rcv_rp, 4) },
    { HRDATA (RCNT, uart_rcv_cnt, 5) },
    { HRDATA (XIP, uart_xmt_ip, 4) },
    { HRDATA (XRP, uart_xmt_rp, 4) },
    { HRDATA (XCNT, uart_xmt_cnt, 5) },
    { HRDATA (XBUF, uart_xbuf, 8) },
    { HRDATA (RALARM, uart_rcv_alarm, 4) },
    { DRDATA (RPOS, uart_unit[UART_RCV].pos, T_ADDR_W), PV_LEFT },
    { DRDATA (RTIME, uart_unit[UART_RCV].wait, 24), REG_NZ + PV_LEFT },
    { DRDATA (XPOS, uart_unit[UART_XMT].pos, T_ADDR_W), PV_LEFT },
    { DRDATA (XTIME, uart_unit[UART_XMT].wait, 24), REG_NZ + PV_LEFT },
    { DRDATA (FTIME, uart_unit[UART_FIFO].wait, 24), REG_NZ + PV_LEFT },
    { NULL }
    };


/*
 * sim> set uart socket PORTNUM
 *      Attaches the uart to socket PORTNUM
 *
 * sim> set uart socket_wait 1
 *      Wait for a remote connection before continuing
 */

extern struct simple_socket_priv uart_socket_priv;
t_stat uart_socket_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &uart_socket_priv;
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

t_stat uart_socket_wait_set (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    struct simple_socket_priv * p = &uart_socket_priv;
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

MTAB uart_mod[] = {
    { UNIT_8B, 0      , "7b", "7B", &uart_set_opt },
    { UNIT_8B, UNIT_8B, "8b", "8B", &uart_set_opt },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0, "SOCKET",    "SOCKET",
        &uart_socket_set, NULL},
    { MTAB_XTD|MTAB_VDV|MTAB_NMO, 0,    "SOCKET_WAIT", "SOCKET_WAIT",
        &uart_socket_wait_set, NULL},
    { 0 }
    };

DEVICE uart_dev = {
    "UART", uart_unit, uart_reg, uart_mod,
    3, 10, 31, 1, 8, 8,
    NULL, NULL, &uart_reset,
    NULL, NULL, NULL,
    &uart_dib, DEV_DIB
    };

/* UART register read */

t_bool uart_rd (t_uint64 pa, t_uint64 *dat, uint32 lnt)
{
uint32 off = (uint32) SC1_REG_OFF (pa);
uint32 i, qp, enbi;

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: UART: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
switch (off) {

    case UART_RBUF:                                     /* read rcv fifo */
        if (uart_lctl & UART_LCTL_DLAB) {               /* divisor latch? */
            *dat = uart_dl_lo;
            break;
            }
        uart_rcv_tmo = 0;                               /* cancel timeout */
        uart_ista &= ~UART_ISTA_TMO;                    /* clear tmo intr */
        uart_rcv_fifo[uart_rcv_rp] &= 0xFF;             /* clear err */
        *dat = uart_rcv_fifo[uart_rcv_rp];              /* return top */
        if (uart_rcv_cnt > 0) {                         /* any entries? */
            uart_rcv_cnt--;                             /* pop fifo */
            uart_rcv_rp++;
            if (uart_rcv_rp >= UART_FIFO_SIZE)
                uart_rcv_rp = 0;
            uart_eval_torq ();                          /* eval new top */
            }
        if (uart_rcv_cnt < uart_rcv_alarm)              /* below alarm? */
            uart_ista &= ~UART_ISTA_RCV;                /* clear intr */
        break;

    case UART_IE:                                       /* int enables */
        if (uart_lctl & UART_LCTL_DLAB)                 /* divisor latch? */
            *dat = uart_dl_hi;
        else *dat = uart_ie & UART_IE_RW;               /* no, int enab */
        break;

    case UART_IID:                                      /* int ID */
        enbi = uart_ie & uart_ista;                     /* pending intr */
        if (enbi & UART_ISTA_RSTA)                      /* rcv status? */
            uart_iid = UART_IID_RSTA;
        else if (enbi & UART_ISTA_RCV)                  /* rcv fifo alarm? */
            uart_iid = UART_IID_RCV;
        else if (enbi & UART_ISTA_TMO)                  /* rcv timeout? */
            uart_iid = UART_IID_TMO;
        else if (enbi & UART_ISTA_XMT) {                /* xmt? */
            uart_iid = UART_IID_XMT;
            //uart_ista &= ~UART_ISTA_XMT;                /* clear xmt */
            }
        else if (enbi & UART_ISTA_MODEM)                /* modem? */
            uart_iid = UART_IID_MODEM;
        else uart_iid = UART_IID_NOINTR;                /* no intr */
        uart_iid |= UART_IID_MBO;                       /* set MBO */
        *dat = uart_iid;
        break;

    case UART_LCTL:                                     /* line control */
        *dat = uart_lctl;
        break;

    case UART_LSTA:                                     /* line status */
        uart_ista &= ~UART_ISTA_RSTA;                   /* clear rcv sta int */
        uart_lsta &= ~UART_LSTA_DYN;                    /* clear dynamic */
        if (uart_rcv_cnt) uart_lsta |= UART_LSTA_DRDY;  /* any rcv data? */
        if (uart_xmt_cnt == 0) {                        /* xmt fifo empty? */
            uart_lsta |= UART_LSTA_FEMP;
            if (uart_xbuf_full == 0)                    /* and xmt buf too? */
                uart_lsta |= UART_LSTA_XEMP;
            }
        for (i = 0, qp = uart_rcv_rp; i < uart_rcv_cnt; i++) {
            if (uart_rcv_fifo[qp++] & (UART_RCV_BRK|UART_RCV_PE))
                uart_lsta |= UART_LSTA_ERR;             /* error in fifo? */
            if (qp >= UART_FIFO_SIZE) qp = 0;
            }
        *dat = uart_lsta;                               /* clear err flags */
        uart_lsta &= ~(UART_LSTA_OE|UART_LSTA_PE|UART_LSTA_FE|UART_LSTA_BRK);
        break;

    case UART_MCTL:                                     /* modem control */
        *dat = uart_mctl & 0xFF;			            // Linux requires this
        break;

    case UART_MSTA:                                     /* modem status */
        *dat = UART_MSTA_VAL;
        uart_ista &= ~UART_ISTA_MODEM;                  /* clear modem intr */
        break;

    default:
        return FALSE;
        }

uart_eval_intr ();
return TRUE;
}

/* UART register write */

t_bool uart_wr (t_uint64 pa, t_uint64 dat, uint32 lnt)
{
uint32 off = (uint32) SC1_REG_OFF (pa);
uint32 d = (uint32) dat;

if ((pa & 7) != 0) {                                    /* must be aligned */
    fprintf (stderr, "%%Error: UART: unaligned address %X\n", (uint32) pa);
    return FALSE;
    }
switch (off) {

    case UART_XBUF:                                     /* write xmt buffer */
        if (uart_lctl & UART_LCTL_DLAB) {               /* divisor latch? */
            uart_dl_lo = d & 0xFF;
            break;
            }
        if (!(uart_unit[UART_XMT].flags & UNIT_8B))     /* 7b? */
            uart_xbuf = (uint32) dat & 0x7F;
        else {
            uart_xbuf = d & 0xFF;
            if (uart_lctl & UART_LCTL_PE)
                uart_xbuf = (uart_xbuf & ~0x80) |
                    uart_calc_par (uart_xbuf, uart_lctl);
            }
        uart_xbuf_full = 1;
        sim_activate (&uart_unit[UART_FIFO], uart_unit[UART_FIFO].wait);
        uart_ista &= ~UART_ISTA_XMT;
        break;

    case UART_IE:                                       /* int enables */
        if (uart_lctl & UART_LCTL_DLAB)                 /* divisor latch? */
            uart_dl_hi = d & 0xFF;
        else {                                          /* no, int enable */
            uart_ie = d & UART_IE_RW;
            if (d & UART_ISTA_RCV)                      /* RCV controls TMO */
                 uart_ie |= UART_ISTA_TMO;
            }
        break;

    case UART_FCTL:                                     /* FIFO control */
        uart_fctl = d & 0xFF;
        if (d & UART_FCTL_RRST)                         /* receive reset? */
            uart_rcv_ip = uart_rcv_rp = uart_rcv_cnt = 0;
        if (d & UART_FCTL_XRST)                         /* xmit reset? */
            uart_xmt_ip = uart_xmt_rp = uart_xmt_cnt = 0;
        uart_rcv_alarm = rcv_alarm[UART_FCTL_ALARM (d)];
        uart_eval_torq ();
        break;

    case UART_LCTL:                                     /* line control */
        uart_lctl = d & 0xFF;
        break;

    case UART_MCTL:                                     /* modem control */
        uart_mctl = d & 0xFF;
        break;

    default:
        return FALSE;
        }

uart_eval_intr ();
return TRUE;
}

/* UART receive service */
#undef KGDB_SERIAL
#ifdef KGDB_SERIAL
int32 kgdb_serial = 1;
extern CORECTX *cpu_ctx[NUM_CORES];
#endif

t_stat uart_rcv_svc (UNIT *uptr)
{
int32 c=0, par;
char ch;


if (uptr->flags & UNIT_DIS) return SCPE_OK;             /* disabled? stop poll */
sim_activate (uptr, uptr->wait);                        /* continue poll */
if (uart_socket_read(&ch) == 1) {
    c = (int32) ch | SCPE_KFLAG;
#ifdef KGDB_SERIAL
    if (kgdb_serial && ch == 0x3) {
	int cc;
	for (cc = 0; cc < NUM_CORES; cc++) cpu_ctx[cc]->events |= EVT_DINT;
    }
#endif
} else {
    c = sim_poll_kbd ();
}
if (c < SCPE_KFLAG) {        /* no char or error? */
    if (uart_rcv_tmo && (--uart_rcv_tmo == 0)) {        /* countdown to 0? */
        uart_ista |= UART_ISTA_TMO;                     /* set timeout int */
        uart_eval_intr ();                              /* force intr eval */
        }
    return c;
    }
if (c & SCPE_BREAK) c = UART_RCV_BRK;                   /* break? */
else if (!(uptr->flags & UNIT_8B)) c = c & 0x7F;        /* 7b? */
else if (!(uart_lctl & UART_LCTL_PE)) c = c & 0xFF;     /* no parity */
else {                                                  /* char is 8b */
    c = c & 0xFF;
    par = uart_calc_par (c, uart_lctl);
    if ((c ^ par) & 0x80) c = c | UART_RCV_PE;
    }
if (uart_rcv_cnt >= UART_FIFO_SIZE) {                   /* rcv FIFO full? */
    uart_lsta |= UART_LSTA_OE;                          /* set overrun, intr */
    uart_ista |= UART_ISTA_RSTA;
    }
else {
    uart_rcv_cnt++;                                     /* incr rcv count */
    uart_rcv_fifo[uart_rcv_ip++] = c;                   /* store chars, err */
    if (uart_rcv_ip >= UART_FIFO_SIZE)
        uart_rcv_ip = 0;
    if (uart_rcv_cnt >= uart_rcv_alarm) {               /* enough chars? intr */
        uart_ista |= UART_ISTA_RCV;
        uart_rcv_tmo = 0;
        }
    else uart_rcv_tmo = UART_RCV_TMO_CNT;               /* no, start timeout */
    }
uart_eval_torq ();                                      /* error at rcv q top? */
uart_eval_intr ();
return SCPE_OK;
}

/* UART transmit service */

t_stat uart_xmt_svc (UNIT *uptr)
{
t_stat r;

if (uart_xmt_cnt) {
#ifdef KGDB_SERIAL
    if (kgdb_serial)
	r = SCPE_OK;
    else
#endif
    	r = sim_putchar_s (uart_xmt_fifo[uart_xmt_rp], 0);
    /* Mirror to the UART socket (if active) */
    uart_socket_write((char) uart_xmt_fifo[uart_xmt_rp]);
    if (r != SCPE_OK) {
        sim_activate (uptr, uptr->wait);                /* try again */
        return ((r == SCPE_STALL)? SCPE_OK: r);         /* if !stall, report */
        }
    uart_xmt_cnt--;
    uart_xmt_rp++;
    if (uart_xmt_rp >= UART_FIFO_SIZE)
        uart_xmt_rp = 0;
    }
if (uart_xmt_cnt) sim_activate (uptr, uptr->wait);      /* more? */
if (uart_xbuf_full) uart_fifo_svc (uptr);               /* FIFO service pending? */
else if (uart_xmt_cnt == 0) uart_ista |= UART_ISTA_XMT;
uart_eval_intr ();
return SCPE_OK;
}

/* UART FIFO service */

t_stat uart_fifo_svc (UNIT *uptr)
{
if (uart_xbuf_full && (uart_xmt_cnt < UART_FIFO_SIZE)) {/* room to insert? */
    uart_xmt_cnt++;
    uart_xmt_fifo[uart_xmt_ip++] = uart_xbuf & 0xFF;
    if (uart_xmt_ip >= UART_FIFO_SIZE)
        uart_xmt_ip = 0;
    uart_xbuf_full = 0;
    sim_activate (&uart_unit[UART_XMT], uart_unit[UART_XMT].wait);
    }
return SCPE_OK;
}    

/* Calculate parity routine */

uint32 uart_calc_par (uint32 c, uint32 lc)
{
switch UART_LCTL_PAR (lc) {

    case 0:
        return odd_par[c & 0x7F];
    case 1:
        return odd_par[c & 0x7F] ^ 0x80;
    case 2:
        return 0x80;
    }

return 0;
}

/* Evaluate interrupt routine */

void uart_eval_intr (void)
{
uint32 enbi;

enbi = uart_ista & uart_ie;
if (enbi) cac_set_slow(SLOW_UART);
else cac_clr_slow(SLOW_UART);
return;
}

/* Evaluate top of receive queue for errors */

void uart_eval_torq (void)
{
if (uart_rcv_fifo[uart_rcv_rp] & UART_RCV_BRK) {
    uart_lsta |= UART_LSTA_BRK;
    uart_ista |= UART_ISTA_RSTA;
    }
if (uart_rcv_fifo[uart_rcv_rp] & UART_RCV_PE) {
    uart_lsta |= UART_LSTA_PE;
    uart_ista |= UART_ISTA_RSTA;
    }
return;
}

/* UART reset */

t_stat uart_reset (DEVICE *dptr)
{
uint32 i;

uart_rcv_ip = uart_rcv_rp = uart_rcv_cnt = 0;
uart_xmt_ip = uart_xmt_rp = uart_xmt_cnt = 0;
uart_xbuf = uart_xbuf_full = 0;
uart_lctl = UART_LCTL_DEFAULT;
uart_lsta = UART_LSTA_FEMP | UART_LSTA_XEMP ;
uart_msta = 0;
uart_ista = 0;
uart_ie = 0;
uart_iid = 0;
uart_fctl = 0;
uart_rcv_alarm = 1;
uart_rcv_tmo = 0;
for (i = 0; i < UART_FIFO_SIZE; i++)
    uart_rcv_fifo[i] = uart_xmt_fifo[i] = 0;
if (uart_unit[UART_RCV].flags & UNIT_DIS)
    sim_cancel (&uart_unit[UART_RCV]);
else sim_activate (&uart_unit[UART_RCV], uart_unit[UART_RCV].wait);
sim_cancel (&uart_unit[UART_XMT]);
sim_cancel (&uart_unit[UART_FIFO]);
return SCPE_OK;
}

t_stat uart_set_opt (UNIT *uptr, int32 val, char *cptr, void *desc)
{
uart_unit[UART_RCV].flags = (uart_unit[UART_RCV].flags & ~UNIT_8B) | val;
uart_unit[UART_XMT].flags = (uart_unit[UART_XMT].flags & ~UNIT_8B) | val;
return SCPE_OK;
}
