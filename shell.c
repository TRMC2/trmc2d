/*
 * Simple non-network shell for the TRMC2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "constants.h"
#include "parse.h"
#include "io.h"
#include "interpreter.h"
#include "shell.h"

#ifdef USE_READLINE

    #include <readline/readline.h>
    #include <readline/history.h>

    /*
     * libreadline requires sequences of non printing characters to be
     * enclosed between \1 and \2.
     */
    #define COLOR_TEMPD   "\1\x1b[33m\2"
    #define COLOR_ARROW   "\1\x1b[1;34m\2"
    #define COLOR_DEFAULT "\1\x1b[0m\2"

#else  /* if !defined(USE_READLINE) */

    /*
     * Naive replacement for libreadline, with no fancy line editing.
     */

    #include <string.h>
    #define LINE_LENGTH 1024

    #define COLOR_TEMPD   "\x1b[33m"
    #define COLOR_ARROW   "\x1b[1;34m"
    #define COLOR_DEFAULT "\x1b[0m"

    char *readline(const char *prompt)
    {
        fputs(prompt, stdout);
        char * line = malloc(LINE_LENGTH);
        char * ret;

        /* Retry reading as long as we get interrupted. */
        do ret = fgets(line, LINE_LENGTH, stdin);
        while (!ret && errno == EINTR);

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
    const int history_length = 0;
    HIST_ENTRY *history_get(int index) { (void) index; return NULL; }
    void add_history(const char *string) { (void) string; }

#endif  /* if !defined(USE_READLINE) */

int force_color_prompt;
static client_t *tty;

static void handle_line(char *line)
{
    /* Exit on EOF (Ctrl-D). */
    if (!line) {
        fputs("quit\n", stdout);
        should_quit = 1;
        return;
    }

    HIST_ENTRY *last_entry = history_get(history_length - 1);
    const char *last_line = last_entry ? last_entry->line : NULL;
    if (line && *line &&
            (!last_line || strcmp(line, last_line) != 0))
        add_history(line);
    int ret = parse(line, trmc2_syntax, tty);
    free(line);
    if (ret < 0)
        report_error(tty, const_name(ret, parse_errors));
    if (tty->quitting)
        should_quit = 1;  /* terminate if the client is leaving */
}

/* Read lines on stdin and send them to parse(). */
int shell(void)
{
    tty = get_client_slot();
    tty->in = 0;          /* stdin, actually unused */
    tty->out = 1;         /* stdout */
    tty->autoflush = 1;   /* don't have to call process_output() */
    tty->verbose = 1;     /* start in verbose mode */

    const char *term = getenv("TERM");
    const char *prompt;
    if (force_color_prompt || (term && strstr(term, "color")))
        prompt = COLOR_TEMPD"tempd"COLOR_ARROW">"COLOR_DEFAULT" ";
    else
        prompt = "tempd> ";

    while (!should_quit) {
        char *line = readline(prompt);
        handle_line(line);
    }

    return EXIT_SUCCESS;
}
