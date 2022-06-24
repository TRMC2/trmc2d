// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * trmc2d: TRMC2 temperature daemon.
 *
 * This program controls a TRMC2 temperature controller. At
 * initialization, it binds to a TCP port (default: 5025) or,
 * optionally, a Unix domain socket. It then accepts SCPI-like
 * commands from connected clients.
 *
 * This program has to be installed suid root in order to gain access to
 * the I/O space of the serial port and to get real-time scheduling
 * priority.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include "constants.h"
#include "parse.h"
#include "io.h"
#include "interpreter.h"
#include "shell.h"

static const char cmdline_help[] =
"Usage: trmc2d [-h] [-s] [-p port] [-u name] [-d]\n"
"Options:\n"
"    -h       print this message\n"
"    -s       shell mode (talk to stdin/stdout)\n"
"    -c       use a color prompt in shell mode\n"
"    -p port  bind to the specified TCP port\n"
"    -u name  bind to a Unix domain socket with the given name\n"
"    -n count accept that many simultaneous clients (default: 1)\n"
"    -d       go to the background\n"
"Default is to bind to TCP port 5025 (aka scpi-raw).\n";

static const char optstring[] = "hscp:u:n:d";

#define FD_SET_M(fd, set, max_fd) do { FD_SET(fd, set); \
        max_fd = fd>max_fd ? fd : max_fd; } while (0)

int main(int argc, char *argv[])
{
    int opt;
    int port = 5025;
    int shell_mode = 0;
    const char *socket_name = NULL;
    int max_client_count = 1;
    int client_count = 0;
    int i;
    client_t *cl;
    int ls;                         /* listening socket */
    union {                         /* client socket:   */
        struct sockaddr    any;     /*  - generic       */
        struct sockaddr_un un;      /*  - Unix domain   */
        struct sockaddr_in in;      /*  - TCP           */
    } peer_addr;
    int domain = AF_INET;
    socklen_t peer_lg = sizeof(struct sockaddr_in);
    fd_set rfds, wfds;

    /* Process options. */
    while ((opt=getopt(argc, argv, optstring)) != -1) switch(opt) {
        case 'h':
            fputs(cmdline_help, stdout);
            return EXIT_SUCCESS;
        case 's':
            shell_mode = 1;
            break;
        case 'c':
            force_color_prompt = 1;
            break;
        case 'p':
            if (socket_name) {
                fputs(cmdline_help, stderr);
                return EXIT_FAILURE;
            }
            port = atoi(optarg);
            break;
        case 'u':
            domain = AF_UNIX;
            peer_lg = sizeof(struct sockaddr_un);
            socket_name = optarg;
            break;
        case 'n':
            max_client_count = atoi(optarg);
            if (max_client_count > MAX_CLIENTS) {
                fprintf(stderr,
                        "Cannot accept more than %d simultaneous clients\n",
                        MAX_CLIENTS);
                max_client_count = MAX_CLIENTS;
            }
            if (max_client_count < 1) {
                fprintf(stderr,
                        "Cannot accept less than one client\n");
                max_client_count = 1;
            }
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
    if (shell_mode)
        return shell();

    /* Log messages via syslog. */
    openlog("trmc2d", 0, LOG_DAEMON);

    /* Get a listening socket. */
    ls = get_socket(domain, port, socket_name);
    if (ls == -1) return EXIT_FAILURE;

    do {

        /* select() loop. */
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        int max_fd = 0;
        for (i=0; i<MAX_CLIENTS; i++) {
            cl = &client[i];
            if (cl->active) {
                FD_SET_M(cl->in, &rfds, max_fd);
                if (cl->output_pending)
                    FD_SET_M(cl->out, &wfds, max_fd);
            }
        }
        cl = get_client_slot();
        if (cl) FD_SET_M(ls, &rfds, max_fd);
        int ret = select(max_fd + 1, &rfds, &wfds, NULL, NULL);
        if (ret == -1) {
            if (errno == EINTR)    /* Interrupted system call */
                continue;
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
            if (client_count >= max_client_count) {
                close(cl->in);
            } else {
                cl->active++;
                client_count++;
            }
        }

        /* Do I/O. */
        for (i=0; i<MAX_CLIENTS; i++) {
            cl = &client[i];
            if (!cl->active) continue;
            if (cl->in > max_fd) continue;   /* this is the new client */
            if (FD_ISSET(cl->out, &wfds)) process_output(cl);
            if (FD_ISSET(cl->in, &rfds)) {
                if (process_input(cl)) {
                    char command[COMMAND_LENGTH];
                    while (get_command(cl, command)) {
                        ret = parse(command, trmc2_syntax, cl);
                        if (ret < 0)
                            report_error(cl, const_name(ret, parse_errors));
                    }
                } else {         /* client disconnected */
                    client->quitting = 1;
                }
                if (client->quitting) {
                    close(cl->in);
                    cl->active = 0;
                    client_count--;
                }
            }
        }

    } while (!should_quit);

    return EXIT_SUCCESS;
}
