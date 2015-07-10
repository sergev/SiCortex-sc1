#if !defined(_WIN32)

#ifdef SIMX_NATIVE  /* Built into simx */
#include "ScxUtil.h"
#include "ScxVars.h"
#include "ScxLog.h"
#else  /* Normal build of simh */
#define UERROR(...) printf(__VA_ARGS__)
#define UINFO(num, ...) printf(__VA_ARGS__)
#endif

/* Not on Windows, so define simple socket code */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <fcntl.h>
#include "simple_socket.h"


/* 
 * do_simple_socket:
 *   Read and write from socket, if socket is not initialized,
 *   then the first call will initialize it.
 */

int do_simple_socket(struct simple_socket_priv * p,
                   enum SIMPSOCK_RW_MODE rw_mode, char *ch, int *io_status)
{
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    struct pollfd pfd;
    char buf[2];
    static char pbuf[200];
    int n=0, flags;
  
    /* io_status defaults to 0 (i.e. no I/O) */
    if (io_status != NULL) *io_status = 0;

simple_socket_again:
    switch(p->state) {
    case SIMPSOCK_OFF:
        /* We couldn't get a port or we aren't actived, so do nothing */
        return SIMPSOCK_FAIL;
        break;

    case SIMPSOCK_START:
        /* First call, initialize state. */
        if (p->socket <= 0) {
            p->state = SIMPSOCK_OFF;
            return SIMPSOCK_FAIL;
        } else if (p->socket == 1) {
            p->socket = p->startport;
        }
        if (p->socket_wait == 1) {
            p->socket_wait = -1; /* Wait forever until connect */
        }
        p->portno = p->socket;
        p->state = SIMPSOCK_ATTACH;

        /* Fall through */

    case SIMPSOCK_ATTACH:
        /* Search for available port and bind a socket to it */
        p->sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (p->sockfd < 0) {
            sprintf(pbuf, "do_simple_socket (%s): fatal error creating socket\r\n", p->name);
            UERROR(pbuf);
            p->state = SIMPSOCK_OFF;
            return SIMPSOCK_FAIL;
        }
        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        for(;p->portno < p->socket+p->portattempts; p->portno++) {
            serv_addr.sin_port = htons(p->portno);
            if (bind(p->sockfd, (struct sockaddr *) &serv_addr,
                    sizeof(serv_addr)) < 0) {
                sprintf(pbuf, "do_simple_socket (%s): port %d busy\r\n", p->name, p->portno);
                UINFO(1,pbuf);
            } else {
                sprintf(pbuf, "do_simple_socket (%s): bound to port %d\r\n", p->name, p->portno);
#ifdef SIMX_NATIVE   /* Built into simx */
                ScxLog a_portFile;
                sprintf(pbuf, "logs/sock_%s.dat", p->name);
                a_portFile.open(pbuf);
                a_portFile << p->portno << endl;
                a_portFile.flush();
                a_portFile.close();
#endif
                UINFO(1, pbuf);
                p->state = SIMPSOCK_LISTENING;
                listen(p->sockfd,1);
                sprintf(pbuf, "do_simple_socket (%s): port %d listen\r\n", p->name, p->portno);
                UINFO(1,pbuf);
                break; /* Fall through */
            }
        }
        if (p->portno > p->socket+p->portattempts) {
            sprintf(pbuf, "do_simple_socket (%s): ports exhausted\r\n", p->name);
            UERROR(pbuf);
            p->state = SIMPSOCK_OFF;
            return SIMPSOCK_FAIL;
        }

        /* Fall through */

    case SIMPSOCK_LISTENING:
        /* Listen for incoming connections */
        pfd.revents = 0;
        pfd.events = POLLIN | POLLPRI;
        pfd.fd = p->sockfd;
        if (p->socket_wait != 0) {
            sprintf(pbuf, "do_simple_socket (%s): waiting for connection on port %d\r\n", p->name, p->portno);
            UINFO(0, pbuf);
        }
        if (poll(&pfd, 1, p->socket_wait)) {
            clilen = sizeof(cli_addr);
            p->newsockfd = accept(p->sockfd,
                        (struct sockaddr *) &cli_addr,
                        &clilen);
            close(p->sockfd); /* Only one at a time */
            /* 
             * Set new socket to non-blocking. Reads/writes will return -1 and
             * set errno to EAGAIN instead of blocking.
             */ 
            if (p->newsockfd < 0) {
                sprintf(pbuf, "do_simple_socket (%s): error on accept\r\n", p->name);
                UERROR(pbuf);
                p->state = SIMPSOCK_ATTACH;
                goto simple_socket_again;
            }
            if (-1 == (flags = fcntl(p->newsockfd, F_GETFL, 0))) flags = 0;
            if (fcntl(p->newsockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
                sprintf(pbuf, "do_simple_socket (%s): failed to make socket non-blocking\r\n", p->name);
                UERROR(pbuf);
                close(p->newsockfd);
                p->state = SIMPSOCK_ATTACH;
                goto simple_socket_again;
            }
            sprintf(pbuf, "do_simple_socket (%s): accepted connection from %d.%d.%d.%d\r\n",
                    p->name,
                    cli_addr.sin_addr.s_addr & 0xFF,
                    cli_addr.sin_addr.s_addr>>8 & 0xFF,
                    cli_addr.sin_addr.s_addr>>16 & 0xFF,
                    cli_addr.sin_addr.s_addr>>24 & 0xFF);
            UINFO(1, pbuf);
            p->state = SIMPSOCK_IO; /* Fall through */
        } else {
            if (p->socket_wait != 0) {
                sprintf(pbuf, "do_simple_socket (%s): wait for connection expired, socket connections disabled\r\n", p->name);
                UINFO(0, pbuf);
                close(p->sockfd);
                p->state = SIMPSOCK_FAIL;
                return SIMPSOCK_FAIL;
            } else {
                return SIMPSOCK_AGAIN;
            }
        }

        /* Fall through */

    case SIMPSOCK_IO:
        if (rw_mode == SIMPSOCK_INIT) return SIMPSOCK_OK;
        if ((io_status==NULL) || (ch == NULL)) return SIMPSOCK_FAIL;

        *io_status = 0;
        buf[0] = *ch;
        switch (rw_mode) {
        case SIMPSOCK_READ:
            n = read(p->newsockfd,buf,1);
            break;
        case SIMPSOCK_BLOCKING_READ:
            do {
                n = read(p->newsockfd,buf,1);
            } while ((n < 0) && (errno == EAGAIN));
            break;
        case SIMPSOCK_WRITE:
            n = write(p->newsockfd,buf,1);
            break;
        case SIMPSOCK_BLOCKING_WRITE:
            do {
                n = write(p->newsockfd,buf,1);
            } while ((n <0) && (errno == EAGAIN));
            break;
        default:
            return SIMPSOCK_FAIL;
        }
        *ch = buf[0];

        if (n > 0) {
            /* Successful I/O */
            *io_status = 1;
        } else if ((n < 0) && (errno == EAGAIN)) {
            /* No I/O (would have blocked) */
            *io_status = 0;
        } else if ((n < 0) && (errno != EAGAIN)) {
            *io_status = 0;
            close(p->newsockfd);
            p->state = SIMPSOCK_ATTACH;
            sprintf(pbuf, "do_simple_socket: error on %s\r\n",
                rw_mode == 3 ? "write" : "read");
            UERROR(pbuf);
            return SIMPSOCK_AGAIN;
        } else if (n == 0) {
            *io_status = 0;
            close(p->newsockfd);
            p->state = SIMPSOCK_ATTACH;
            sprintf(pbuf, "do_simple_socket: disconnect (during %s)\r\n",
                rw_mode == 3 ? "write" : "read");
            UINFO(1, pbuf);
            return SIMPSOCK_AGAIN;
        }
        break;
    }

    return SIMPSOCK_OK;
}

/*
 * Uart Socket
 */

struct simple_socket_priv uart_socket_priv = {
    state:        SIMPSOCK_START,
    name:         "uart",
    socket:       0,
    socket_wait:  0,
    startport:    USOCK_START_PORT,
    portno:       0,
    portattempts: USOCK_PORT_ATTEMPTS,
    sockfd:       0,
    newsockfd:    0};

/* Read a character from the socket
 *   i.e. for input to the UART from the socket */
int uart_socket_read(char * ch_p) {
    int io_status=0;
#ifdef SIMX_NATIVE  /* Built into simx */
    if (uart_socket_priv.state == SIMPSOCK_START) {
        uart_socket_priv.socket      = ScxVars::varULong("uartSocket",0);
        uart_socket_priv.socket_wait = ScxVars::varULong("uartSocketWait",0);
    }
#endif
    do_simple_socket(&uart_socket_priv, SIMPSOCK_READ, ch_p, &io_status);
    return io_status;
}

/* Write a character to the socket
 *   i.e. for output from the UART to the socket */
int uart_socket_write(char ch) {
    int io_status=0;
#ifdef SIMX_NATIVE  /* Built into simx */
    if (uart_socket_priv.state == SIMPSOCK_START) {
        uart_socket_priv.socket      = ScxVars::varULong("uartSocket",0);
        uart_socket_priv.socket_wait = ScxVars::varULong("uartSocketWait",0);
    }
#endif
    do_simple_socket(&uart_socket_priv, SIMPSOCK_WRITE, &ch, &io_status);
    return io_status;
}

#endif /* !defined(_WIN32) */
