/*
 * Simple non-network shell for the TRMC2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
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

    #define LINE_LENGTH 1024

    #define COLOR_TEMPD   "\x1b[33m"
    #define COLOR_ARROW   "\x1b[1;34m"
    #define COLOR_DEFAULT "\x1b[0m"

    const char *rl_prompt;
    void (*rl_handler)(char *);
    char *rl_line;
    size_t rl_pos;

    void rl_callback_handler_install(const char *prompt,
            void (*handler)(char *))
    {
        rl_prompt = prompt;
        rl_handler = handler;
        fputs(rl_prompt, stdout);
        fflush(stdout);
    }

    void rl_callback_handler_remove(void)
    {
        rl_prompt = NULL;
    }

    /* Handle terminal input assuming canonical mode. */
    void rl_callback_read_char(void)
    {
        if (!rl_line)
            rl_line = malloc(LINE_LENGTH);

        char *p = rl_line + rl_pos;
        size_t available = LINE_LENGTH - rl_pos;
        ssize_t sz = read(STDIN_FILENO, p, available);
        if (sz < 0) {  // error, presumably EINTR
            if (errno != EINTR) perror("stdin");
        } else if (sz == 0) {  // EOF
            rl_handler(NULL);
        } else {  // something actually read
            rl_pos += sz;
            char *eol = rl_line + rl_pos - 1;
            if (*eol == '\n') {
                *eol = '\0';
                rl_handler(rl_line);  // this free()s rl_line
                rl_line = NULL;
                rl_pos = 0;
                if (rl_prompt) {
                    fputs(rl_prompt, stdout);
                    fflush(stdout);
                }
            }
        }
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
        rl_callback_handler_remove();
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
    if (should_quit)
        rl_callback_handler_remove();
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

    /* Process commands. */
    rl_callback_handler_install(prompt, handle_line);
    while (!should_quit) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);

        /* Restart on interrupted system call. */
        if (ret == -1 && errno == EINTR)
                continue;

        if (FD_ISSET(STDIN_FILENO, &fds))
            rl_callback_read_char();
    }

    return EXIT_SUCCESS;
}
