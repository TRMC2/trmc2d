/*
 * Simple non-network shell for the TRMC2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "constants.h"
#include "parse.h"
#include "interpreter.h"
#include "io.h"

/* Read lines on stdin and send them to parse(). */
int main(void)
{
    client_t *tty;
    char *line;
    int ret;

    tty = get_client_slot();
    tty->in = 0;          /* stdin, actually unused */
    tty->out = 1;         /* stdout */
    tty->autoflush = 1;   /* don't have to call process_output() */

    while (!should_quit && (line = readline("trmc2> ")) != NULL) {
        if (line && *line) add_history(line);
        ret = parse(line, trmc2_syntax, tty);
        free(line);
        if (ret < 0)
            push_error(const_name(ret, parse_errors));
    }
    if (!should_quit) fputs("quit\n", stdout);  /* exit via Control-D */
    return EXIT_SUCCESS;
}
