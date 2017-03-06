/*
 * Simple non-network shell for the TRMC2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include "parse.h"
#include "interpreter.h"
#include "io.h"
#include "shell.h"

#ifdef USE_READLINE

    #include <readline/readline.h>
    #include <readline/history.h>

#else  /* if !defined(USE_READLINE) */

    /*
     * Naive replacement for libreadline, with no fancy line editing.
     */

    #include <string.h>
    #define LINE_LENGTH 1024

    char *readline(const char *prompt)
    {
        fputs(prompt, stdout);
        char * line = malloc(LINE_LENGTH);
        char * ret = fgets(line, LINE_LENGTH, stdin);

        /* Return NULL if EOF on empty line. */
        if (!ret) {
            free(line);
            return NULL;
        }

        /* Remove trailing '\n'. */
        char * eol = line + strlen(line) - 1;
        if (*eol == '\n') *eol = '\0';

        return line;
    }

    /* History not supported. */
    typedef struct { char *line; } HIST_ENTRY;
    typedef struct { HIST_ENTRY **entries; int length; } HISTORY_STATE;
    HISTORY_STATE *history_get_history_state(void) { return NULL; }
    void add_history(const char *string) { (void) string; }

#endif  /* if !defined(USE_READLINE) */

/* Read lines on stdin and send them to parse(). */
int shell(void)
{
    client_t *tty;
    char *line;
    int ret;

    tty = get_client_slot();
    tty->in = 0;          /* stdin, actually unused */
    tty->out = 1;         /* stdout */
    tty->autoflush = 1;   /* don't have to call process_output() */

    while (!should_quit && (line = readline("tempd> ")) != NULL) {
        HISTORY_STATE *history = history_get_history_state();
        HIST_ENTRY *last_entry = history && history->entries ?
                history->entries[history->length-1] : NULL;
        char *last_line = last_entry ? last_entry->line : NULL;
        if (line && *line &&
                (!last_line ||
                (last_line && strcmp(line, last_line) != 0)))
            add_history(line);
        ret = parse(line, trmc2_syntax, tty);
        free(line);
        if (ret < 0)
            push_error(const_name(ret, parse_errors));
    }
    if (!should_quit) fputs("quit\n", stdout);  /* exit via Control-D */
    return EXIT_SUCCESS;
}
