/*
 * Generic parser for an SCPI-like language.
 *
 * The language is defined by a syntax tree. Each command has the form
 *      token1[s1]:token2[s2]:...:tokenn[sn][?] [parameters]
 * where the successive tokens are the names of the nodes along the path
 * from the root to the desired command. Each token can have an optional
 * numeric suffix (a non-negative integer). Each command can have a
 * query form (with `?' appended), which expects a response, and an
 * imperative form (without `?') which expects no response. Commands can
 * accept an optional comma-separated list of parameters.
 *
 * Examples:
 *      channel2:voltage:range 0.1     sets the voltage range to 100 mV
 *                                     on channel 2
 *      channel2:voltage:range?        query the voltage range
 *      channel2:measure?              query a measurement.
 *
 * See test-parse.c for a trivial example on how to use this parser.
 */

/*
 * Parsed form of a command. The parser gives this to the command
 * handler.
 */
typedef struct {
    int query;      /* boolean: is this a query? */
    int n_tok;      /* number of tokens */
    char **tok;     /* array of token names */
    int *suffix;    /* array of suffixes (-1 = no suffix) */
    int n_param;    /* number of parameters */
    char **param;   /* list of parameters */
} parsed_command;

/*
 * Prototype of command handlers:
 *
 *      parse_data  is the data given to the parser
 *      cmd_data    is the data associated to the command node
 *      command     is the parsed command as described above.
 *
 * The handler is expected to return a non-negative status code.
 * Negative status codes are reserved for parse errors.
 */
typedef int (*command_handler)(void *parse_data, int cmd_data,
        parsed_command *command);

/*
 * The language is defined by an array of those structures: one for each
 * top-level node. The array is terminated by a structure with a NULL
 * name. child is a pointer to a NULL-name terminated array of the
 * node's children, or NULL if this is a leaf node.
 *
 * Each command will invoke only one handler: the one associated to the
 * node pointed by the entire command.
 */
typedef struct _syntax_node {
    char *name;                     /* node name */
    command_handler handler;        /* command handler */
    int data;                       /* command data */
    struct _syntax_node *child;     /* list of children */
} syntax_tree;

/*
 * Parse command according to language. data is passed as parse_data to
 * the handler. Returs the return code of the invoked handler or a
 * negative error code (see below) in case of parsing error.
 */
int parse(char *command, syntax_tree *language, void *data);

/* Error codes returned by the parser. */
#define EMPTY_COMMAND    -1     /* empty command */
#define TOO_MANY_TOKENS_IN_COMMAND -2  /* tokens found past a leaf node */
#define NO_SUCH_COMMAND  -3     /* no node name matchs a given token */
#define NO_HANDLER       -4     /* handler for command is NULL */
