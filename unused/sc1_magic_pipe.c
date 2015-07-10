
/*
 * sc1_magic_pipe.c:
 *
 * Support for special instructions that allow code under simh to
 * communicate with an external * daemon via a socket.
 */

#if defined(_WIN32)
int64 sim_magic_pipe_instruction(uint64_t reg_val)
{
    return -1;
}
#else /* !_WIN32 */

#include "sc1_defs.h"
#include "sc1_magic_pipe.h"

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

static t_bool initialized = FALSE;
static int socket_fd;

/* Various ways to figure out where to connect to. */
#define PORT_FILE_NAME_A "logs/magic_pipe.dat"
#define PORT_FILE_NAME_B "magic_pipe.dat"
#define PORT_FILE_ENV_VARNAME "MAGIC_PIPE_FILE"
#define PORT_ENV_VARNAME "MAGIC_PIPE_PORT"

#define MAX_HOST_NAME 100

enum magic_pipe_errors {
    magic_pipe_ok =	         0,
    magic_pipe_no_data =        -1,
    magic_pipe_bad_port_info =  -2,
    magic_pipe_no_port_file =   -3,
    magic_pipe_connect_failed = -4,
    magic_pipe_write_failed =   -5,
    magic_pipe_read_failed =    -6,
};

static int connectsock(const char *host, unsigned port);

static int init_pipe(void)
{
    char hostname[MAX_HOST_NAME + 1];
    int port;
    char *env, *file_name;
    FILE *fp;

    if((env = getenv(PORT_ENV_VARNAME)) != NULL)
    {
	if(sscanf(env, "%100[^:]:%d", hostname, &port) != 2)
	{
	    return magic_pipe_bad_port_info;
	}
    }
    else
    {
	/* Three ways to find the name of the file with the port
	   numbers in it: */
	fp = NULL;
	file_name = getenv(PORT_FILE_ENV_VARNAME);
	if(file_name != NULL)
	{
	    fp = fopen(file_name, "r");
	}
	if(fp == NULL)
	{
	    file_name = PORT_FILE_NAME_A;
	    fp = fopen(file_name, "r");
	}
	if(fp == NULL)
	{
	    file_name = PORT_FILE_NAME_B;
	    fp = fopen(file_name, "r");
	}
	if(fp == NULL) return magic_pipe_no_port_file;

	/* Note that the file format matches what was used for the fabric
	   socket, not what the above environment variable uses. */
	if(fscanf(fp, "%*s %100s %d", hostname, &port) != 2)
	{
	    fclose(fp);
	    return magic_pipe_bad_port_info;
	}
	fclose(fp);
    }

    socket_fd = connectsock(hostname, port);
    if(socket_fd < 0) return magic_pipe_connect_failed;

    return 0;
}

int64_t sim_magic_pipe_instruction(uint64_t reg_val)
{
    int result;
    unsigned char c;

    if(!initialized)
    {
	result = init_pipe();
	if(result != magic_pipe_ok) return result;
	initialized = TRUE;
    }

    if((t_int64) reg_val >= 0)
    {
	/* Write the pipe. */
	c = reg_val & 0xff;
	result = write(socket_fd, &c, 1);
	if(result != 1) return magic_pipe_write_failed;
	return magic_pipe_ok;
    }
    else
    {
	/* Read the pipe. */
	if((t_int64) reg_val == -2)
	{
	    /* non-blocking read */

	    fd_set fds;
	    struct timeval zero_timeout;

	    FD_ZERO(&fds);
	    FD_SET(socket_fd, &fds);
	    zero_timeout.tv_sec = 0;
	    zero_timeout.tv_usec = 0;
	
	    result = select(socket_fd + 1, &fds, NULL, NULL, &zero_timeout);
	    if(result == 0) return magic_pipe_no_data;   /* nothing available */
	}

	/* blocking read */
	result = read(socket_fd, &c, 1);

	if(result != 1) return magic_pipe_read_failed;
	return c;
    }
}

#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif	/* INADDR_NONE */

static int connectsock(const char *host, unsigned port)
/*
 * Arguments:
 *      host      - name of host to which connection is desired
 *      service   - service associated with the desired port
 *      transport - name of transport protocol to use ("tcp" or "udp")
 */
{
    struct hostent	*phe;	/* pointer to host information entry	*/
    struct protoent *ppe;	/* pointer to protocol information entry*/
    struct sockaddr_in sin;	/* an Internet endpoint address		*/
    int	s, type;	/* socket descriptor and socket type	*/

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    /* Map service name to port number */
    if ((sin.sin_port=htons((unsigned short)port)) == 0)
    {
	return -1;
    }

    /* Map host name to IP address, allowing for dotted decimal */
    if((phe = gethostbyname(host)) != NULL)
    {
	memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    }
    else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
    {
	return -1;
    }

    /* Map transport protocol name to protocol number */
    if ( (ppe = getprotobyname("tcp")) == 0)
    {
	return -1;
    }

    type = SOCK_STREAM;

    /* Allocate a socket */
    s = socket(PF_INET, type, ppe->p_proto);
    if (s < 0)
    {
	return s;
    }

    /* Connect the socket */
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
	close(s);
	return -1;
    }

    return s;
}

#endif /* !_WIN32 */
