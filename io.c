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
    return &client[i];
}

/* Cleanup: unlink_socket() has to be registered with atexit(). */
static char *socket_name;
static void unlink_socket(void) { unlink(socket_name); }

/*
 * Get a listen()ing socket ready to accept() a connection.
 * Returns -1 on error.
 */
int get_socket(char *name)
{
    int s;
    struct sockaddr_un my_addr;
    int err;

    socket_name = name;

    /* Get a socket. */
    s = socket(PF_UNIX, SOCK_STREAM, 0);
    if (s == -1) { syslog(LOG_ERR, "socket: %m\n"); return -1; }

    /* bind() it. */
    my_addr.sun_family = AF_UNIX;
    strcpy(my_addr.sun_path, socket_name);
    err = bind(s, (struct sockaddr *) &my_addr, sizeof my_addr);
    if (err == -1) { syslog(LOG_ERR, "bind: %m\n"); return -1; }

    /* We want to unlink the named socket at program termination. */
    err = atexit(unlink_socket);
    if (err) {
        syslog(LOG_ERR, "atexit failed\n");
        unlink_socket();
        return -1;
    }

    /* Make sure clients can connect to us. */
    err = chmod(socket_name, 0666);
    if (err) {
        syslog(LOG_ERR, "chmod failed\n");
        return -1;
    }

    /* listen() to one client at a time. */
    err = listen(s, 1);
    if (err) { syslog(LOG_ERR, "listen: %m\n"); return -1; }

    return s;
}

/* Queue message in the client output buffer. */
void queue_output(client_t *cl, char *fmt, ...)
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
 * Read a string from the client and return it in a static buffer.
 * Returns NULL on disconnect.
 */
char *process_input(client_t *cl)
{
    static char buffer[1024];
    int ret;
#ifdef ECHO_COMMANDS
    char *eol;
#endif

    /* Read the command line. */
    ret = read(cl->in, buffer, sizeof buffer - 1);
    if (ret == -1) {
        syslog(LOG_ERR, "read: %m\n");
        exit(EXIT_FAILURE);
    }
    buffer[ret] = '\0';

    /* End of file? */
    if (ret == 0) return NULL;

#ifdef ECHO_COMMANDS
    /* Testing: echo to stderr (ugly code here). */
    eol = strrchr(buffer, '\n');
    if (*eol) strcpy(eol, "\\n");
    fprintf(stderr, "[%s]\n", buffer);
    if (*eol) strcpy(eol, "\n");
#endif

    return buffer;
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
