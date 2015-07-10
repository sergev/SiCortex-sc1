/* sc1_gdb.c - SiCortex 1 gdb remote stub

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

   Copyright (c) 2003-2005, Joel Martin

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


/*
 * TODO: if gdb is attached, then the simh prompt should go to the remote gdb
 * session and continues in gdb should trigger a continue at the simh prompt.
 */

#if defined (_WIN32)
#include "sc1_defs.h"

static t_stat gdb_reset(DEVICE *dptr)
{
    printf ("in gdb_reset()\r\n"); 
    return SCPE_OK;
}

#else

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sc1_defs.h"
#include "sc1_gdb.h"
#include "simple_socket.h"


/* 
 * simple_socket state structure
 */

struct simple_socket_priv gdb_socket_priv = {
    state:        SIMPSOCK_OFF,
    name:         "sc1_gdb",
    socket:       0,
    socket_wait:  0,
    startport:    1234,
    portno:       0,
    portattempts: 10,
    sockfd:       0,
    newsockfd:    0};


/*
 * SIMH -> GDB interface section
 */

/* Local function declarations */

static t_stat gdb_reset( DEVICE *dptr );
static t_stat gdb_attach( UNIT *uptr, char *cptr );
static t_stat gdb_detach( UNIT *uptr );
static t_stat gdb_rcv_svc( UNIT *uptr );
static void handle_exception(int cpu);
static void putDebugChar(char ch);     /* write a single character      */
static char getDebugChar();      /* read and return a single char */
static unsigned char * getpacket(void);
static void putpacket(unsigned char * buffer);

/* SIMH data definitions */

UNIT gdb_unit[] = {
        { UDATA(&gdb_rcv_svc, UNIT_ATTABLE, 0) },
};

DEVICE gdb_dev = {
    "GDB",              /* name */
    gdb_unit,           /* units */
    NULL,               /* registers */
    NULL,               /* modifiers */
    1,                  /* #units */
    16,                 /* address radix */
    0,                  /* address width */
    8,                  /* addr increment */
    16,                 /* data radix */
    64,                 /* data width */
    NULL,               /* examine routine */
    NULL,               /* deposit routine */
    &gdb_reset,         /* reset routine */
    NULL,               /* boot routine */
    &gdb_attach,        /* attach routine */
    &gdb_detach,        /* detach routine */
    (void *) NULL,      /* context */
    0,                  /* flags */
};

/* SIMH external declarations */

extern CORECTX *cpu_ctx[NUM_CORES];

extern t_uint64 fp_getcr (CORECTX *ctx, uint32 rn);

/* SIMH function definitions */

/* Taken from ReadB and WriteB but without TLB exceptions */

t_bool gdb_ReadB (CORECTX *ctx, t_uint64 va, t_uint64 *val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (!xlate_va (ctx, va, VA_CON, &pa, &catr)) return FALSE;
return CALL_READPB (ctx, pa, val, catr);
}

t_bool gdb_WriteB (CORECTX *ctx, t_uint64 va, t_uint64 val)
{
t_uint64 pa;
uint32 catr;

if (Q_MD_U32) va = SEXT_W_D (va);
if (!xlate_va (ctx, va, VA_CON, &pa, &catr)) return FALSE;
return CALL_WRITEPB (ctx, pa, val, catr);
}

static t_stat gdb_reset(DEVICE *dptr)
{
    sim_activate(dptr->units, GDB_POLL_INTVL);

    return SCPE_OK;
}

static t_stat gdb_attach( UNIT *uptr, char *cptr )
{
    struct simple_socket_priv *p = &gdb_socket_priv;

    if (p->state != SIMPSOCK_OFF) {
        printf("gdb_attach: already attached\r\n");
        return SCPE_ALATT;
    }

    p->socket = atoi(cptr);
    if (p->socket <= 0) {
        printf("gdb_attach: invalid port number\n");
        return SCPE_INVSW;
    }
    p->state = SIMPSOCK_START;

    printf("gdb_attach: attached\r\n");

    return SCPE_OK;
}

static t_stat gdb_detach( UNIT *uptr )
{
    struct simple_socket_priv *p = &gdb_socket_priv;

    p->state = SIMPSOCK_OFF;
    printf("gdb_detach: detached\r\n");

    return SCPE_OK;
}

static t_stat gdb_rcv_svc( UNIT *uptr )
{
    sim_activate(uptr, GDB_POLL_INTVL);

    return gdb_stub(0);
}

t_stat gdb_stub(int cpu)
{
    struct simple_socket_priv *p = &gdb_socket_priv;
    static int first_time=1;
    static struct pollfd pfd;

    if (p->state != SIMPSOCK_IO) {
        do_simple_socket(p, SIMPSOCK_INIT, NULL, NULL);
    }

    if (p->state == SIMPSOCK_IO) {
        pfd.revents = 0;
        pfd.events = POLLIN | POLLPRI;
        pfd.fd = p->newsockfd;
        if ((first_time) || (poll(&pfd, 1, 0)) ||
            ((cpu_ctx[cpu]->traps & TRAP_GEN) && 
             (cpu_ctx[cpu]->traps & TRAP_BREAK))) {
            first_time=0;
            handle_exception(cpu);
        }
    }

    return SCPE_OK;
}

/*
 * GDB Stub Section
 *
 * The following functions are based on sparc-stub.c
 *
 */

static void putDebugChar(char ch)
{
    int io_status;

    do_simple_socket(&gdb_socket_priv, SIMPSOCK_BLOCKING_WRITE, &ch, &io_status);
}

static char getDebugChar()
{
    char ch;
    int io_status;

    do_simple_socket(&gdb_socket_priv, SIMPSOCK_BLOCKING_READ, &ch, &io_status);

    if (io_status == 0) {
        return '\0';
    }

    return ch;
}


static const char hexchars[]="0123456789abcdef";

#define NUMREGS 73

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames {R0,  R1,  R2,  R3,  R4,  R5,  R6,  R7,
               R8,  R9,  R10, R11, R12, R13, R14, R15,
               R16, R17, R18, R19, R20, R21, R22, R23,
               R24, R25, R26, R27, R28, R29, R30, R31,

               SR,  LO,  HI,  BADVA, CAUSE, PC,

               F0,  F1,  F2,  F3,  F4,  F5,  F6,  F7,
               F8,  F9,  F10, F11, F12, F13, F14, F15,
               F16, F17, F18, F19, F20, F21, F22, F23,
               F24, F25, F26, F27, F28, F29, F30, F31,

               FSR, FIR,  FP };

static int
hex (char ch)
{
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

/* scan for the sequence $<data>#<checksum>     */

static unsigned char *
getpacket (void)
{
    struct simple_socket_priv *p = &gdb_socket_priv;
    unsigned char * buffer = (unsigned char *)remcomInBuffer;
    char checksum;
    char xmitcsum;
    int count;
    char ch;

    while (1) {
        /* wait around for the start character, ignore all other characters */
        do {
            ch = getDebugChar();
            if (p->state != SIMPSOCK_IO) return NULL;
        } while (ch != '$');

retry:
        checksum = 0;
        xmitcsum = -1;
        count = 0;

        /* now, read until a # or end of buffer is found */
        while (count < BUFMAX) {
            ch = getDebugChar ();
            if (ch == '$')
                goto retry;
            if (ch == '#')
                break;
            checksum = checksum + ch;
            buffer[count] = ch;
            count = count + 1;
        }
        buffer[count] = 0;

        if (ch == '#') {
            ch = getDebugChar ();
            xmitcsum = hex (ch) << 4;
            ch = getDebugChar ();
            xmitcsum += hex (ch);

            if (checksum != xmitcsum) {
                putDebugChar ('-');       /* failed checksum */
            } else {
                putDebugChar ('+');       /* successful transfer */

                /* if a sequence char is present, reply the sequence ID */
                if (buffer[2] == ':') {
                    putDebugChar (buffer[0]);
                    putDebugChar (buffer[1]);

                    memmove(buffer, buffer+3, BUFMAX-3);
                }
                //printf("getpacket: %s\r\n", buffer);
                return buffer;
            }
        }
    }
}

static void putpacket(unsigned char * buffer)
{
    struct simple_socket_priv *p = &gdb_socket_priv;
    unsigned char checksum;
    int count;
    unsigned char ch;
    
    //printf("putpacket: %s\r\n", buffer);

    /*  $<packet info>#<checksum>. */
    do {
        putDebugChar('$');
        checksum = 0;
        count = 0;

        while ((ch = buffer[count])) {
            putDebugChar(ch);
            checksum += ch;
            count += 1;
        }

        putDebugChar('#');
        putDebugChar(hexchars[checksum >> 4]);
        putDebugChar(hexchars[checksum & 0xf]);

        ch = getDebugChar();
        if (p->state != SIMPSOCK_IO) return;
    } while (ch != '+');
}

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 * If MAY_FAULT is non-zero, then we will handle memory faults by returning
 * a 0, else treat a fault like any other fault in the stub.
 */

static char *
mem2hex (char *mem, char *buf, int count, int may_fault)
{
  unsigned char ch;

  while (count-- > 0)
    {
      ch = *mem++;
      if (mem_err)
        return 0;
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
    }

  *buf = 0;

  return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char *
hex2mem (char *buf, char *mem, int count, int may_fault)
{
  int i;
  unsigned char ch;

  for (i=0; i<count; i++)
    {
      ch = hex(*buf++) << 4;
      ch |= hex(*buf++);
      *mem++ = ch;
      if (mem_err)
        return 0;
    }

  return (char *) mem;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */

static int
hexToInt(char **ptr, long *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex(**ptr);
      if (hexValue < 0)
        break;

      *intValue = (*intValue << 4) | hexValue;
      numChars ++;

      (*ptr)++;
    }

  return (numChars);
}


/*
 * required commands: g, G, m, M, c, s
 */

static void
handle_exception (int cpu)
{
    struct simple_socket_priv *p = &gdb_socket_priv;
    int sigval;
    long addr, length;
    long tmp, thread=cpu;
    char *ptr;

    t_uint64 val;
    int i, good;

    sigval = SIGTRAP;
    ptr = remcomOutBuffer;

    *ptr++ = 'T';
    *ptr++ = hexchars[sigval >> 4];
    *ptr++ = hexchars[sigval & 0xf];

    strcpy(ptr,"thread");
    ptr += 6;
    *ptr++ = ':';
    tmp = thread + 1;
    ptr = mem2hex((char *) &tmp, ptr, 1, 1);
    *ptr++ = ';';

    *ptr++ = hexchars[PC >> 4];
    *ptr++ = hexchars[PC & 0xf];
    *ptr++ = ':';
    ptr = mem2hex((char *)&cpu_ctx[thread]->last_PC, ptr, 8, 0); /* PC */
    *ptr++ = ';';

    /* We could send other registers here if we wanted to. Do we want to? */

    *ptr++ = 0;

    putpacket((unsigned char *)remcomOutBuffer);

    while (1) {
        if (p->state != SIMPSOCK_IO) return;
        remcomOutBuffer[0] = 0;
        ptr = (char *)getpacket();
        switch (*ptr++)
        {
        case '?':
            remcomOutBuffer[0] = 'S';
            remcomOutBuffer[1] = hexchars[sigval >> 4];
            remcomOutBuffer[2] = hexchars[sigval & 0xf];
            remcomOutBuffer[3] = 0;
            break;

        case 'd':               /* toggle debug flag */
            break;

        case 'g':               /* return the value of the CPU registers */
            {
            ptr = remcomOutBuffer;
            i = 0;
            //for (i=0; i<32; i++) {
            //    printf("R[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->R[i]);
            //}
            ptr = mem2hex((char *)cpu_ctx[thread]->R, ptr, 32 * 8, 0); /* General Purpose */

            //printf("SR 0x%016x\r\n", (unsigned int)cpu_ctx[thread]->cp0_sr);
            ptr = mem2hex((char *)&cpu_ctx[thread]->cp0_sr, ptr, 4, 0); /* SR */
            memset(ptr, '0', 4 * 2); /* Pad since gdb thinks it's 64-bits */
            ptr += 4 * 2;

            //printf("mlo 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mlo);
            ptr = mem2hex((char *)&cpu_ctx[thread]->mlo, ptr, 8, 0); /* LO */

            //printf("mhi 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mhi);
            ptr = mem2hex((char *)&cpu_ctx[thread]->mhi, ptr, 8, 0); /* HI */

            //printf("badva 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->cp0_badva);
            ptr = mem2hex((char *)&cpu_ctx[thread]->cp0_badva, ptr, 8, 0); /* BADVA */

            //printf("cause 0x%08x\r\n", (unsigned int)cpu_ctx[thread]->cp0_cause);
            ptr = mem2hex((char *)&cpu_ctx[thread]->cp0_cause, ptr, 4, 0); /* CAUSE */
            memset(ptr, '0', 4 * 2); /* Pad since gdb thinks it's 64-bits */
            ptr += 4 * 2;

            //printf("PC 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->last_PC);
            ptr = mem2hex((char *)&cpu_ctx[thread]->last_PC, ptr, 8, 0); /* PC */

            //for (i=0; i<32; i++) {
            //    printf("F[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->F[i]);
            //}
            ptr = mem2hex((char *)&cpu_ctx[thread]->F, ptr, 32 * 8, 0); /* Floating Point */

            /* TODO: This might not be quite right */
            val = 0;
            val = cpu_ctx[thread]->fpcr;
            //printf("fsr 0x%08x\r\n", (unsigned int)val);
            ptr = mem2hex((char *)&val, ptr, 8, 0); /* FSR */

            val = 0;
            val = fp_getcr(cpu_ctx[thread], CP1_FIR);
            //printf("fir 0x%08x\r\n", (unsigned int)val);
            ptr = mem2hex((char *)&val, ptr, 8, 0); /* FIR */

            /* TODO: FP is in the official list but gdb doesn't display it and
             * what is it anyways? */

            }
            break;

        case 'G':          /* set the value of the CPU registers - return OK */
            {
            /* TODO: any registers we need to avoid touching? */

            //for (i=0; i<32; i++) {
            //    printf("before: R[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->R[i]);
            //}
            hex2mem(ptr, (char *)cpu_ctx[thread]->R, 32 * 8, 0); /* General Purpose */
            //for (i=0; i<32; i++) {
            //    printf("after: R[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->R[i]);
            //}
            ptr += 32 * 8 * 2;

            //printf("before: SR 0x%016x\r\n", (unsigned int)cpu_ctx[thread]->cp0_sr);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->cp0_sr, 4, 0); /* SR */
            //printf("after: SR 0x%016x\r\n", (unsigned int)cpu_ctx[thread]->cp0_sr);
            ptr += 4 * 2 + 4 * 2; /* Pad since gdb thinks it's 64-bits */

            //printf("before: mlo 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mlo);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->mlo, 8, 0); /* LO */
            //printf("after: mlo 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mlo);
            ptr += 8 * 2;

            //printf("before: mhi 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mhi);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->mhi, 8, 0); /* HI */
            //printf("after: mhi 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->mhi);
            ptr += 8 * 2;

            //printf("before: badva 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->cp0_badva);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->cp0_badva, 8, 0); /* BADVA */
            //printf("after: badva 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->cp0_badva);
            ptr += 8 * 2;

            //printf("before: cause 0x%08x\r\n", (unsigned int)cpu_ctx[thread]->cp0_cause);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->cp0_cause, 4, 0); /* CAUSE */
            //printf("after: cause 0x%08x\r\n", (unsigned int)cpu_ctx[thread]->cp0_cause);
            ptr += 4 * 2 + 4 * 2; /* Pad since gdb thinks it's 64-bits */

            //printf("before: PC 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->last_PC);
            hex2mem(ptr, (char *)&cpu_ctx[thread]->last_PC, 8, 0); /* PC */
            //printf("after: PC 0x%016lx\r\n", (long unsigned int)cpu_ctx[thread]->last_PC);
            ptr += 8 * 2;

            //for (i=0; i<32; i++) {
            //    printf("before: F[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->F[i]);
            //}
            hex2mem(ptr, (char *)&cpu_ctx[thread]->F, 32 * 8, 0); /* Floating Point */
            //for (i=0; i<32; i++) {
            //    printf("after: F[%d]: 0x%016lx\r\n", i, (long unsigned int)cpu_ctx[thread]->F[i]);
            //}
            ptr += 32 * 8 * 2;


            /* TODO: This might not be quite right */
            val = 0;
            val = cpu_ctx[thread]->fpcr;
            //printf("before: fsr 0x%08x\r\n", (unsigned int)val);
            hex2mem(ptr, (char *)&val, 8, 0); /* FSR */
            //printf("after: fsr 0x%08x\r\n", (unsigned int)val);
            ptr += 8 * 2;

            /* FIR is static */
            ptr += 8 * 2;
    
            strcpy(remcomOutBuffer,"OK");
            }
            break;

        case 'm':         /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
            /* Try to read %x,%x.  */
    
            if (hexToInt(&ptr, &addr)
                && *ptr++ == ','
                && hexToInt(&ptr, &length))
            {
                ptr = remcomOutBuffer;
                for (i=0; i<length; i++) {
                    if (gdb_ReadB(cpu_ctx[thread], addr+i, &val) == TRUE) {
                        ptr = mem2hex((char *) &val, ptr, 1, 1);
                    } else {
                        strcpy (remcomOutBuffer, "E03");
                        break;
                    }
                }
            } else {
                strcpy(remcomOutBuffer,"E01");
            }
            break;

        case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
            /* Try to read '%x,%x:'.  */

            if (hexToInt(&ptr, &addr)
                && *ptr++ == ','
                && hexToInt(&ptr, &length)
                && *ptr++ == ':')
            {
                good = 1;
                for (i=0; i<length; i++) {
                    if (hex2mem(ptr+i*2, (char *)&val, 1, 1)) {
                        if (gdb_WriteB(cpu_ctx[thread], addr+i, val) == FALSE) {
                            good = 0;
                            break;
                        }
                    } else {
                        good = 0;
                        break;
                    }
                }
                if (good == 1) {
                    strcpy(remcomOutBuffer, "OK");
                } else {
                    strcpy(remcomOutBuffer, "E03");
                }
            } else {
                strcpy(remcomOutBuffer, "E02");
            }
            break;

        case 'c':    /* cAA..AA    Continue at address AA..AA(optional) */
            /* try to read optional parameter, pc unchanged if no parm */

            if (hexToInt(&ptr, &addr))
            {
                printf("continue at address unimplemented!\n");
                break;
                // TODO: Implement this properly 
                //cpu_ctx[thread]->PC = addr;
                //registers[PC] = addr;
                //registers[NPC] = addr + 4;
            }

            /* 
             * In simh, there is no need to flush the instruction cache. On
             * a real system, we need to flush the instruction cache here, as
             * we may have deposited a breakpoint, and the icache probably has
             * no way of knowing that a data ref to some location may have
             * changed something that is in the instruction cache.
             */

            /* flush_i_cache(); */

            return;  /* Continue execution */

        /* kill the program */
        case 'k' :              /* do nothing */
            close(p->newsockfd);
            p->state = SIMPSOCK_ATTACH;
            break;

        case 'q':    /* q....    General query operation */
            switch (*ptr++) {
            case 'C':
                strcpy(remcomOutBuffer, "QC");
                tmp = thread + 1;
                mem2hex((char *) &tmp, remcomOutBuffer + 2, 1, 1);

                break;
            case 'f':
                if (strncmp("ThreadInfo", ptr, 10) == 0) {
                    /* Hardcoded to cpu list */
                    strcpy(remcomOutBuffer, "m1,2,3,4,5,6l");
                }
            default:
                break;
            }
            break;

        case 'T':    /* T XX    Check if thread XX is running */
            strcpy(remcomOutBuffer, "OK");
            break;

        case 'H':    /* H c t    Set thread to t for operations c */
            switch (*ptr++) {
            case 'g':
                if (hexToInt(&ptr, &tmp)) {
                    /* 0 means 'any' so we number CPUs 1-6 */
                    if (tmp == -1) {
                        strcpy(remcomOutBuffer, "E01");
                        break;
                    } else if (tmp == 0) {
                        thread = 0;
                    } else {
                        thread = tmp - 1;
                    }
                    strcpy(remcomOutBuffer, "OK");
                }
                break;

            case 'c':
                /* Not supported, can't run just one CPU */
                break;

            default:
                break;
            }
            break;
        }                       /* switch */

        /* reply to the request */
        putpacket((unsigned char *)remcomOutBuffer);
    }
}

#endif
