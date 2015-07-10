#ifndef _SIMPLE_SOCKET_H_
#define _SIMPLE_SOCKET_H_

#define USOCK_START_PORT 9000
#define USOCK_PORT_ATTEMPTS 20

#define SIMPSOCK_OK 1
#define SIMPSOCK_AGAIN -1
#define SIMPSOCK_FAIL -2

enum SIMPSOCK_STATE {SIMPSOCK_OFF, SIMPSOCK_START, SIMPSOCK_ATTACH,
                  SIMPSOCK_LISTENING, SIMPSOCK_IO};

enum SIMPSOCK_RW_MODE {SIMPSOCK_INIT, SIMPSOCK_READ, SIMPSOCK_BLOCKING_READ,
                  SIMPSOCK_WRITE, SIMPSOCK_BLOCKING_WRITE};

struct simple_socket_priv {
    /* Persistent state */
    int state;
    char name[40];
    int socket;  /* start port #, 0 is deactivated */
    int socket_wait;
    int startport;
    int portno;
    int portattempts;
    int sockfd, newsockfd;
};

int do_simple_socket(struct simple_socket_priv * p,
                   enum SIMPSOCK_RW_MODE rw_mode, char *ch, int *io_status);

/* UART specific headers */

int uart_socket_read(char * ch_p);
int uart_socket_write(char ch);

#if defined(_WIN32)

/* Stubs for compiling under Windows */
struct simple_socket_priv uart_socket_priv;

int uart_socket_read(char * ch_p) {
    return 0;
}

int uart_socket_write(char ch) {
    return 0;
}
#endif


#endif
