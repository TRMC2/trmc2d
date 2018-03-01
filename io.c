/*
 * Network and IO functions for tempd. Mostly stolen from fieldd.
 *
 * To avoid blocking on input, we do a single read() when select() tells
 * us it will not block. To avoid blocking on output, we write
 * everything on an internal buffer. When there is data in the buffer,
 * we do a single write() when selects() tells un it will not block.
 *
 * Debugging macro: if compiled with -DECHO_COMMANDS, all commands
 * received will be echoed on stderr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/ip.h>
#include "io.h"

/* Array of clients. */
client_t client[MAX_CLIENTS];

/* Get a free slot from the above array. */
client_t *get_client_slot(void)
{
    int i;

    for (i=0; i<MAX_CLIENTS; i++) if (!client[i].active) break;
    if (i == MAX_CLIENTS) return NULL;
    client[i].output_pending = 0;
    client[i].autoflush = 0;
    client[i].verbose = 0;
    return &client[i];
}

/* Cleanup: unlink_socket() has to be registered with atexit(). */
static const char *socket_name;
static void unlink_socket(void) { unlink(socket_name); }

/*
 * Get a listening socket ready to accept() a connection.
 * 'domain' should be either:
 *  - AF_UNIX for a Unix domain socket:
 *      - 'port' is ignored
 *      - 'name' is the socket file name
 *  - AF_INET for a TCP port:
 *      - 'port' is the port number
 *      - 'name' is ignored.
 * Returns -1 on error, the listening socket on success.
 */
int get_socket(int domain, int port, const char *name)
{
    int s;
    union {
        struct sockaddr    any;
        struct sockaddr_un unix_;  // 'unix' is a numeric constant
        struct sockaddr_in inet;
    } my_addr;
    size_t address_size;
    int err;

    /* Sanity check. */
    if (domain != AF_UNIX && domain != AF_INET) {
        syslog(LOG_ERR, "Communication domain not supported\n");
        return -1;
    }

    /* Get a socket. */
    s = socket(domain, SOCK_STREAM, 0);
    if (s == -1) { syslog(LOG_ERR, "socket: %m\n"); return -1; }

    /* Avoid the TIME_WAIT delay when restarting the daemon. */
    const int one = 1;
    err = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    if (err == -1) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR): %m\n"); return -1;
        return -1;
    }

    /* bind() it. */
    switch (domain) {
        case AF_UNIX:
            my_addr.unix_.sun_family = AF_UNIX;
            strcpy(my_addr.unix_.sun_path, name);
            address_size = sizeof my_addr.unix_;
            break;
        case AF_INET:
            my_addr.inet.sin_family = AF_INET;
            my_addr.inet.sin_port = htons(port);
            my_addr.inet.sin_addr.s_addr = INADDR_ANY;
            address_size = sizeof my_addr.inet;
            break;
    }
    err = bind(s, &my_addr.any, address_size);
    if (err == -1) { syslog(LOG_ERR, "bind: %m\n"); return -1; }

    if (domain == AF_UNIX) {

        /* We want to unlink the named socket at program termination. */
        socket_name = name;
        err = atexit(unlink_socket);
        if (err) {
            syslog(LOG_ERR, "atexit failed\n");
            unlink_socket();
            return -1;
        }

        /* Make sure clients can connect to us. */
        err = chmod(name, 0666);
        if (err) {
            syslog(LOG_ERR, "chmod failed\n");
            return -1;
        }
    }

    /* listen() to one client at a time. */
    err = listen(s, 1);
    if (err) { syslog(LOG_ERR, "listen: %m\n"); return -1; }

    return s;
}

/* Get a command from the input buffer. */
char *get_command(client_t *cl, char *command)
{
    char *eol = strpbrk(cl->input_buffer, "\r\n");
    if (!eol) return NULL;
    char *next_cmd = eol + 1;
    if (eol[0] == '\r' && eol[1] == '\n') next_cmd++;
    *eol = '\0';
    strcpy(command, cl->input_buffer);
    size_t shift = next_cmd - cl->input_buffer;
    memmove(cl->input_buffer, next_cmd, cl->input_pending - shift);
    cl->input_pending -= shift;
    cl->input_buffer[cl->input_pending] = '\0';
    return command;
}

/* Queue message in the client output buffer. */
void queue_output(client_t *cl, const char *fmt, ...)
{
    va_list ap;
    int available;
    int n;

    available = sizeof cl->output_buffer - cl->output_pending;
    va_start(ap, fmt);
    n = vsnprintf(cl->output_buffer + cl->output_pending, available,
            fmt, ap);
    va_end(ap);

    /* -1 in glibc < 2.1. Space needed in C99 and glibc >= 2.1 */
    if (n == -1 || n >= available) {
        syslog(LOG_WARNING, "Output buffer overflow\n");
        n = available;
    }
    cl->output_pending += n;

    if (cl->autoflush) while (cl->output_pending)
        process_output(cl);
}

/*
 * Read bytes from the client.
 * Returns the number of bytes read, 0 on disconnect.
 */
int process_input(client_t *cl)
{
    char *p = cl->input_buffer + cl->input_pending;
    size_t sz = sizeof cl->input_buffer - cl->input_pending;
    int ret;
#ifdef ECHO_COMMANDS
    char *eol;
#endif

    /* Read the client input. */
    ret = read(cl->in, p, sz - 1);
    if (ret == -1) {
        syslog(LOG_ERR, "read: %m\n");
        exit(EXIT_FAILURE);
    }
    cl->input_pending += ret;
    p[ret] = '\0';

#ifdef ECHO_COMMANDS
    /* Testing: echo to stderr (ugly code here). */
    if (!ret) return 0;
    eol = strrchr(cl->input_buffer, '\n');
    if (*eol) strcpy(eol, "\\n");
    fprintf(stderr, "[%s]\n", cl->input_buffer);
    if (*eol) strcpy(eol, "\n");
#endif

    return ret;
}

/* Send pending output to the client. */
void process_output(client_t *cl)
{
    int ret;

    if (!cl->output_pending) return;    /* be defensive */
    ret = write(cl->out, cl->output_buffer, cl->output_pending);
    if (ret < 0) { syslog(LOG_WARNING, "write: %m\n"); return; }
    if (ret > 0 && (unsigned) ret < cl->output_pending) {
#ifdef ECHO_COMMANDS
        fprintf(stderr, "(partial write)\n");
#endif
        memmove(cl->output_buffer, cl->output_buffer + ret,
                cl->output_pending - ret);
    }
    cl->output_pending -= ret;
}
