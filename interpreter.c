/*
 * Interpreter for the TRMC2 language: callbacks and syntax description
 * for parse().
 *
 * The parse_data parameter of all callbacks is the third parameter of
 * parse(). It is assumed to be a client_t* pointing to the current
 * client. Hence its use as queue_output(parse_data, format, ...);
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <Trmc.h>
#include "parse.h"
#include "constants.h"
#include "interpreter.h"
#include "io.h"

/* Instrument identification. */
#define IDN "tempd temperature server, Laboratoire Louis Néel, Jul 2004\n"

#ifdef __gnu_linux__
# define unused(x) x __attribute__((unused))
#else
# define unused(x) x
#endif


/***********************************************************************
 * Error handling.
 */

#define MAX_ERRORS 256

static char *error_stack[MAX_ERRORS];
static int error_sp = 0;

/*
 * Push a static string on the error stack. Don't put '\n' at the end of
 * the string.
 */
void push_error(char *err)
{
    assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
    if (error_sp == MAX_ERRORS)
        error_stack[error_sp-1] = "Error stack overflow";
    else
        error_stack[error_sp++] = err;
}

static int get_error(void *parse_data,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(parse_data != NULL);
    assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
    if (!cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed error command");
        return 1;
    }
    queue_output(parse_data, "%s\n",
            error_sp ? error_stack[--error_sp] : "No errors");
    return 0;
}

static int error_count(void *parse_data,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(parse_data != NULL);
    assert(cmd->n_tok == 2);
    if (!cmd->query || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        push_error("Malformed error command");
        return 1;
    }
    queue_output(parse_data, "%d\n", error_sp);
    return 0;
}

static int clear_errors(unused(void *parse_data),
        unused(int cmd_data), parsed_command *cmd)
{
    assert(cmd->n_tok == 2);
    if (cmd->query || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        push_error("Malformed error command");
        return 1;
    }
    error_sp = 0;
    return 0;
}


/***********************************************************************
 * Get number of boards or channels.
 */

enum {nb_boards, nb_channels};

static int get_number(void *parse_data, int cmd_data, parsed_command *cmd)
{
    int ret;
    int n;
    int (*trmc_function)(int*);

    assert(parse_data != NULL);
    assert(cmd->n_tok == 2);
    if (cmd->query != 1 || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        push_error("Malformed count command");
        return 1;
    }
    switch (cmd_data) {
        case nb_boards:
            trmc_function = GetNumberOfBoardTRMC;
            break;
        case nb_channels:
            trmc_function = GetNumberOfChannelTRMC;
    }
    ret = trmc_function(&n);
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    queue_output(parse_data, "%d\n", n);
    return 0;
}


/***********************************************************************
 * Manage boards.
 */

/* board sub-commands. */
enum {type, address, status, calibration, vranges, vrange, iranges,
    irange};

/* Handle board by calling GetBoardTRMC() and SetBoardTRMC(). */
static int board_handler(void *parse_data, int cmd_data, parsed_command *cmd)
{
    int ret, index, i;
    BOARDPARAMETER board;

    assert(parse_data != NULL);
    assert(cmd->n_tok == 2);
    index = cmd->suffix[0];
    if (index == -1 || cmd->suffix[1] != -1
            || (cmd->query && cmd->n_param != 0)
            || (!cmd->query && cmd->n_param != 1)) {
        push_error("Malformed board command");
        return 1;
    }
    if (!cmd->query && cmd_data != calibration) {
        push_error("Read-only parameter");
        return 1;
    }
    ret =  GetBoardTRMC(_BYINDEX, &board);
    if (cmd->query) switch (cmd_data) {
        case type:
            queue_output(parse_data, "%d\n", board.TypeofBoard);
            break;
        case address:
            queue_output(parse_data, "%d\n", board.AddressofBoard);
            break;
        case status:
            queue_output(parse_data, "%d\n", board.CalibrationStatus);
            break;
        case calibration:
            queue_output(parse_data, "%d\n", board.NumberofCalibrationMeasure);
            break;
        case vranges:
            queue_output(parse_data, "%d\n", board.NumberofVRanges);
            break;
        case iranges:
            queue_output(parse_data, "%d\n", board.NumberofIRanges);
            break;
        case vrange:
            for (i=0; i<board.NumberofVRanges; i++) {
                queue_output(parse_data, "%g\n", board.VRangesTable[i]);
            }
            break;
        case irange:
            for (i=0; i<board.NumberofIRanges; i++) {
                queue_output(parse_data, "%g\n", board.IRangesTable[i]);
            }
    }
    else {      /* cmd_data == calibration */
    }

    return 0;
}


/***********************************************************************
 * Miscellaneous commands.
 */

int should_quit = 0;

static int idn(void *parse_data,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(parse_data != NULL);
    if (!cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed *idn command");
        return 1;
    }
    queue_output(parse_data, IDN);
    return 0;
}

static int quit(unused(void *parse_data),
        unused(int cmd_data), parsed_command *cmd)
{
    if (cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed quit command");
        return 1;
    }
    should_quit = 1;
    return 0;
}


/***********************************************************************
 * Language description.
 */

syntax_tree trmc2_syntax[] = {
    {"*idn", idn, 0, NULL},
    {"board", NULL, 0, (syntax_tree[]) {
        {"count", get_number, nb_boards, NULL},
        {"type", board_handler, type, NULL},
        {"address", board_handler, address, NULL},
        {"status", board_handler, status, NULL},
        {"calibration", board_handler, calibration, NULL},
        {"vranges", board_handler, vranges, NULL},
        {"vrange", board_handler, vrange, NULL},
        {"iranges", board_handler, iranges, NULL},
        {"irange", board_handler, irange, NULL},
        {NULL, NULL, 0, NULL}
    }},
    {"channel", NULL, 0, (syntax_tree[]) {
        {"count", get_number, nb_channels, NULL},
        {"voltage", NULL, 0, (syntax_tree[]) {
            {"range", NULL, 0, NULL},
            {NULL, NULL, 0, NULL}
        }},
        {"current", NULL, 0, (syntax_tree[]) {
            {"range", NULL, 0, NULL},
            {NULL, NULL, 0, NULL}
        }},
        {"addresses", NULL, 0, NULL},
        {"type", NULL, 0, NULL},
        {"mode", NULL, 0, NULL},
        {"averaging", NULL, 0, NULL},
        {"scrutationtime", NULL, 0, NULL},
        {"priority", NULL, 0, NULL},
        {"fifosize", NULL, 0, NULL},
        {"conversion", NULL, 0, NULL},
        {"measure", NULL, 0, NULL},
        {NULL, NULL, 0, NULL}
    }},
    {"regulation", NULL, 0, (syntax_tree[]) {
        {"setpoint", NULL, 0, NULL},
        {"p", NULL, 0, NULL},
        {"i", NULL, 0, NULL},
        {"d", NULL, 0, NULL},
        {"max", NULL, 0, NULL},
        {"resistance", NULL, 0, NULL},
        {"channel", NULL, 0, NULL},
        {NULL, NULL, 0, NULL}
    }},
    {"error", get_error, 0, (syntax_tree[]) {
        {"count", error_count, 0, NULL},
        {"clear", clear_errors, 0, NULL},
        {NULL, NULL, 0, NULL}
    }},
    {"quit", quit, 0, NULL},
    {NULL, NULL, 0, NULL}
};
