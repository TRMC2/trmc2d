/*
 * Generic parser for an SCPI-like language. See parse.h for details.
 *
 * Compile with -DDEBUG_PARSE to see the parsing on stderr.
 */

#include <string.h>
#include <stdlib.h>
#ifdef DEBUG_PARSE
# include <stdio.h>
#endif
#include "parse.h"

#define MAX_TOKENS 16
#define MAX_PARAMS 16

/*
 * Remove the numeric suffix from the token and return it.
 * *token is incremented past the added '\0'.
 */
static int split_suffix(char **token)
{
    char *p;
    int suffix;
    
    p = strpbrk(*token, "0123456789");
    if (!p) return -1;
    suffix = atoi(p);
    *p = '\0';
    *token = p+1;
    return suffix;
}

/* Parse the command according to the language. */
int parse(char *command, syntax_tree *language, void *data)
{
    char *token[MAX_TOKENS], *param[MAX_PARAMS];
    int suffix[MAX_TOKENS];
    parsed_command cmd = {0, 0, token, suffix, 0, param};
    char *p;
    int i;
    syntax_tree *node, *last_node;

    /* Remove trailing garbage. */
    i = strlen(command);
    while (i-- &&
            (command[i]==' ' || command[i]=='\n' || command[i]=='\r'))
        command[i] = '\0';

    /* Look for parameters. */
    cmd.n_param = 0;
    p = strchr(command, ' ');
    while (p) {
        *p++ = '\0';
        while (*p == ' ') p++;
        param[cmd.n_param++] = p;
        if (cmd.n_param == MAX_PARAMS) break;
        p = strchr(p, ',');
    }

    /* Empty? */
    if (!*command) return EMPTY_COMMAND;

    /* Query? */
    p = command + strlen(command) - 1;
    cmd.query = (*p == '?');
    if (cmd.query) *p = '\0';

    /* Cut command in tokens. */
    /* XXX: This should be done in a cleaner way. */
    cmd.n_tok = 0;
    p = token[cmd.n_tok] = command;
    suffix[cmd.n_tok] = split_suffix(&p);
    cmd.n_tok++;
    while (cmd.n_tok<MAX_TOKENS && *p && (p = strchr(p, ':')) != NULL) {
        *p = '\0';
        token[cmd.n_tok] = ++p;
        suffix[cmd.n_tok] = split_suffix(&p);
        cmd.n_tok++;
    }

#ifdef DEBUG_PARSE
    fprintf(stderr, "parsed as ");
    for (i=0; i<cmd.n_tok; i++) {
        fprintf(stderr, "[%s", token[i]);
        if (suffix[i] != -1) fprintf(stderr, "(%d)", suffix[i]);
        putc(']', stderr);
    }
    if (param[0]) fputs(" ", stderr);
    for (i=0; i<cmd.n_param; i++)
        fprintf(stderr, "{%s}", param[i]);
    fputs("\n", stderr);
#endif

    /* Walk down the syntax tree. */
    node = language;
    for (i=0; i<cmd.n_tok; i++) {
        if (!node) return TOO_MANY_TOKENS_IN_COMMAND;
        while (node->name && strcmp(token[i], node->name)) node++;
        if (!node->name) return NO_SUCH_COMMAND;
        last_node = node;
        node = node->child;
    }
    node = last_node;

    /* Invoke handler. */
    if (!node->handler) return NO_HANDLER;
    return node->handler(data, node->data, &cmd);
}
