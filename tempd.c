/*
 * tempd: the temperature daemon, based on libtrmc2, with code stolen
 * from fieldd.
 *
 * This program controls the TRMC2 temperature controller. At
 * initialization it opens a Unix socket in /tmp/tempd-socket (#define
 * SOCKET_NAME for overriding) and listens for ASCII-formatted commands
 * sent over it.
 *
 * This program has to be installed suid root in order to gain access to
 * the I/O space of the serial port and to get real-time scheduling
 * priority.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include "parse.h"
#include "interpreter.h"
#include "io.h"

#ifndef SOCKET_NAME
# define SOCKET_NAME "/tmp/tempd-socket"
#endif


static char *cmdline_help =
"Usage: tempd [-h] [-s] [-d]\n"
"Options:\n"
"    -h       print this message\n"
"    -s       shell mode (talk to stdin/stdout)\n"
"    -p port  bind to TCP port\n"
"    -d       go to the background\n"
"Default is to bind to Unix socket " SOCKET_NAME "\n";

static char *optstring = "hsp:d";

#define FD_SET_M(fd, set) do { FD_SET(fd, set); \
        max_fd = fd>max_fd ? fd : max_fd; } while (0)

int main(int argc, char *argv[])
{
    int opt;
    int port = 0;
    int i;
    client_t *cl;
    int ls;                         /* listening socket */
    union {                         /* client socket:   */
        struct sockaddr    any;     /*  - generic       */
        struct sockaddr_un un;      /*  - Unix domain   */
        struct sockaddr_in in;      /*  - TCP           */
    } peer_addr;
    int domain = AF_UNIX;
    socklen_t peer_lg = sizeof(struct sockaddr_un);
    fd_set rfds, wfds;
    int max_fd;
    int ret;

    /* Process options. */
    while ((opt=getopt(argc, argv, optstring)) != -1) switch(opt) {
        case 'h':
            fputs(cmdline_help, stdout);
            return EXIT_SUCCESS;
        case 's':
            cl = get_client_slot();
            cl->active = 1;
            cl->in = 0;
            cl->out = 1;
            break;
        case 'p':
            domain = AF_INET;
            peer_lg = sizeof(struct sockaddr_in);
            port = atoi(optarg);
            break;
        case 'd':
            if (fork()) _exit(EXIT_SUCCESS);
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            setsid();
            break;
        case '?':
            fputs(cmdline_help, stderr);
            return EXIT_FAILURE;
    }

    /* Log messages via syslog. */
    openlog("tempd", 0, LOG_DAEMON);

    /* Drop root privileges. */
    if (setuid(getuid()) == -1) {
        syslog(LOG_ERR, "setuid: %m\n");
        return EXIT_FAILURE;
    }

    /* Get a listening socket. */
    ls = get_socket(domain, port, SOCKET_NAME);
    if (ls == -1) return EXIT_FAILURE;

    do {

        /* select() loop. */
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        max_fd = 0;
        for (i=0; i<MAX_CLIENTS; i++) {
            cl = &client[i];
            if (cl->active) {
                FD_SET_M(cl->in, &rfds);
                if (cl->output_pending)
                    FD_SET_M(cl->out, &wfds);
            }
        }
        cl = get_client_slot();
        if (cl) FD_SET_M(ls, &rfds);
        ret = select(max_fd + 1, &rfds, &wfds, NULL, NULL);
        if (ret == -1) {
            syslog(LOG_ERR, "select: %m\n");
            return EXIT_FAILURE;
        }

        /* accept() connection. */
        if (FD_ISSET(ls, &rfds)) {
            cl->in = cl->out =
                accept(ls, &peer_addr.any, &peer_lg);
            if (cl->in == -1) {
                syslog(LOG_ERR, "accept: %m\n");
                return EXIT_FAILURE;
            }
            cl->active++;
        }

        /* Do I/O. */
        for (i=0; i<MAX_CLIENTS; i++) {
            cl = &client[i];
            if (!cl->active) continue;
            if (cl->in > max_fd) continue;   /* this is the new client */
            if (FD_ISSET(cl->out, &wfds)) process_output(cl);
            if (FD_ISSET(cl->in, &rfds)) {
                char *command = process_input(cl);

                if(command)
                    parse(command, trmc2_syntax, cl);
                else {         /* client disconnected */
                    close(cl->in);
                    cl->active = 0;
                }
            }
        }

    } while (!should_quit);

    return EXIT_SUCCESS;
}
