/*
 * Interpreter for the TRMC2 language: callbacks and syntax description
 * for parse().
 *
 * The client parameter of all callbacks is the third parameter of
 * parse(). It is assumed to be a client_t* pointing to the current
 * client. Hence its use as queue_output(client, format, ...);
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
#include "plugin.h"

/* Instrument identification. */
#define IDN "tempd temperature server, Institut NEEL, Nov 2016\n"

#ifdef __gnu_linux__
# define unused(x) x __attribute__((unused))
#else
# define unused(x) x
#endif


/***********************************************************************
 * Error handling.
 */

#define MAX_ERRORS 256

static const char *error_stack[MAX_ERRORS];
static int error_sp = 0;

/*
 * Push a static string on the error stack. Don't put '\n' at the end of
 * the string.
 */
void push_error(const char *err)
{
    assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
    if (error_sp == MAX_ERRORS)
        error_stack[error_sp-1] = "Error stack overflow";
    else
        error_stack[error_sp++] = err;
}

static int get_error(void *client,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
    if (!cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed error command");
        return 1;
    }
    queue_output(client, "%s\n",
            error_sp ? error_stack[--error_sp] : "No errors");
    return 0;
}

static int error_count(void *client,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    assert(cmd->n_tok == 2);
    if (!cmd->query || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        push_error("Malformed error command");
        return 1;
    }
    queue_output(client, "%d\n", error_sp);
    return 0;
}

static int clear_errors(unused(void *client),
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

/* Command enumeration. */
enum {nb_boards, nb_channels, b_type, b_address, b_status,
    b_calibration, b_vranges, b_vrange, b_iranges, b_irange, c_vrange,
    c_irange, c_address, c_type, c_mode, c_avg, c_scrutation,
    c_priority, c_fifosz, c_conversion, format, measure};

static int get_number(void *client, int cmd_data, parsed_command *cmd)
{
    int ret;
    int n;

    assert(client != NULL);
    assert(cmd->n_tok == 2);
    assert(cmd_data == nb_boards || cmd_data == nb_channels);
    if (cmd->query != 1 || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        push_error("Malformed count command");
        return 1;
    }
    if (cmd_data == nb_boards)
        ret = GetNumberOfBoardTRMC(&n);
    else  /* cmd_data == nb_channels */
        ret = GetNumberOfChannelTRMC(&n);
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    queue_output(client, "%d\n", n);
    return 0;
}


/***********************************************************************
 * Manage boards.
 */

/* Handle boards by calling GetBoardTRMC() and SetBoardTRMC(). */
static int board_handler(void *client, int cmd_data, parsed_command *cmd)
{
    int ret, index, i;
    BOARDPARAMETER board;

    assert(client != NULL);
    assert(cmd->n_tok == 2);
    index = cmd->suffix[0];
    if (index == -1 || cmd->suffix[1] != -1
            || (cmd->query && cmd->n_param != 0)
            || (!cmd->query && cmd->n_param != 1)) {
        push_error("Malformed board command");
        return 1;
    }
    if (!cmd->query && cmd_data != b_calibration) {
        push_error("Read-only parameter");
        return 1;
    }
    board.Index = index;
    ret =  GetBoardTRMC(_BYINDEX, &board);
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    if (cmd->query) switch (cmd_data) {
        case b_type:
            queue_output(client, "%d (%s)\n", board.TypeofBoard,
                    const_name(board.TypeofBoard, BoardType_names));
            break;
        case b_address:
            queue_output(client, "%d\n", board.AddressofBoard);
            break;
        case b_status:
            queue_output(client, "%d (%s)\n", board.CalibrationStatus,
                    const_name(board.CalibrationStatus, board_mode_names));
            break;
        case b_calibration:
            queue_output(client, "%d\n", board.NumberofCalibrationMeasure);
            break;
        case b_vranges:
            queue_output(client, "%d\n", board.NumberofVRanges);
            break;
        case b_iranges:
            queue_output(client, "%d\n", board.NumberofIRanges);
            break;
        case b_vrange:
            for (i=0; i<board.NumberofVRanges; i++) {
                queue_output(client, "%g\n", board.VRangesTable[i]);
            }
            break;
        case b_irange:
            for (i=0; i<board.NumberofIRanges; i++) {
                queue_output(client, "%g\n", board.IRangesTable[i]);
            }
    }
    else {      /* cmd_data == b_calibration */
    }

    return 0;
}


/***********************************************************************
 * Manage channels.
 */

/*
 * Extra data we have to keep for channels, not present in
 * CHANNELPARAMETER.
 */
typedef struct {
    int index;
    char *conversion;
    char *format;
} channel_t;

/*
 * Return a pointer to the channel_t struct associated to this channel.
 * Allocate memory if needed.
 *
 * Warning: the data structure may be moved around by subsequent calls
 * to this function.
 */
static channel_t *get_channel_extras(int index)
{
    static channel_t *channels;
    static int allocated;
    static int count;

    /* See if we already have a slot allocated to this channel. */
    for (int i = 0; i < count; i++) {
        if (channels[i].index == index)
            return &channels[i];
    }

    /* Add an extra element to the array. */
    if (count == allocated) {
        allocated += 16;
        channels = realloc(channels, allocated * sizeof *channels);
    }
    channels[count].index = index;
    channels[count].conversion = NULL;
    channels[count].format = NULL;
    return &channels[count++];
}

/* Handle channels by calling GetChannelTRMC() and SetChannelTRMC(). */
static int channel_handler(void *client, int cmd_data, parsed_command *cmd)
{
    int ret, index;
    CHANNELPARAMETER channel;
    AMEASURE meas;
    channel_t *channel_extras;

    assert(client != NULL);
    index = cmd->suffix[0];
    if (index == -1 || cmd->suffix[1] != -1
            || (cmd->query && cmd->n_param != 0)
            || (!cmd->query && cmd->n_param != 1)) {
        push_error("Malformed channel command");
        return 1;
    }
    if (!cmd->query && (cmd_data == c_address
                || cmd_data == c_type || cmd_data == measure)) {
        push_error("Read-only parameter");
        return 1;
    }
    channel.Index = index;
    ret =  GetChannelTRMC(_BYINDEX, &channel);
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    if (cmd->query) switch (cmd_data) {
        case c_vrange:
            queue_output(client, "%g\n", channel.ValueRangeV);
            break;
        case c_irange:
            queue_output(client, "%g\n", channel.ValueRangeI);
            break;
        case c_address:
            queue_output(client, "%d, %d\n",
                    channel.BoardAddress, channel.SubAddress);
            break;
        case c_type:
            queue_output(client, "%d\n", channel.BoardType);
            break;
        case c_mode:
            queue_output(client, "%d (%s)\n", channel.Mode,
                    const_name(channel.Mode, Mode_names));
            break;
        case c_avg:
            queue_output(client, "%d\n", channel.PreAveraging);
            break;
        case c_scrutation:
            queue_output(client, "%d\n", channel.ScrutationTime);
            break;
        case c_priority:
            queue_output(client, "%d (%s)\n", channel.PriorityFlag,
                    const_name(channel.PriorityFlag, Priority_names));
            break;
        case c_fifosz:
            queue_output(client, "%d\n", channel.FifoSize);
            break;
        case c_conversion:
            channel_extras = get_channel_extras(index);
            if (!channel_extras->conversion) {
                push_error("Channel has no conversion.");
                return 1;
            }
            queue_output(client, "%s\n", channel_extras->conversion);
            break;
        case format:
            break;
        case measure:
            ret = ReadValueTRMC(index, &meas);
            if (ret) {
                push_error(const_name(ret, error_codes));
                return 1;
            }
            if (channel.Etalon)
                queue_output(client, "%g, %g\n",
                        meas.MeasureRaw, meas.Measure);
            else
                queue_output(client, "%g\n", meas.MeasureRaw);
            break;
    } else {  /* !query */
        switch (cmd_data) {
            case c_vrange:
                channel.ValueRangeV = atof(cmd->param[0]);
                break;
            case c_irange:
                channel.ValueRangeI = atof(cmd->param[0]);
                break;
            case c_mode:
                channel.Mode = atoi(cmd->param[0]);
                break;
            case c_avg:
                channel.PreAveraging = atoi(cmd->param[0]);
                break;
            case c_scrutation:
                channel.ScrutationTime = atoi(cmd->param[0]);
                break;
            case c_priority:
                channel.PriorityFlag = atoi(cmd->param[0]);
                break;
            case c_fifosz:
                channel.FifoSize = atoi(cmd->param[0]);
                break;
            case c_conversion:

                /* Remember the conversion parameters. */
                channel_extras = get_channel_extras(index);
                if (channel_extras->conversion)
                    free(channel_extras->conversion);
                size_t sz = 0;
                for (int i = 0; i < cmd->n_param; i++)
                    sz += strlen(cmd->param[i]) + 1;
                channel_extras->conversion = malloc(sz);
                channel_extras->conversion[0] = '\0';
                for (int i = 0; i < cmd->n_param; i++) {
                    strcat(channel_extras->conversion, cmd->param[i]);
                    if (i < cmd->n_param - 1)
                        strcat(channel_extras->conversion, " ");
                }

                /* Use the give conversion. */
                if (channel.Etalon) convert_cleanup(channel.Etalon);
                channel.Etalon = convert_init(cmd->n_param, cmd->param);
                if (!channel.Etalon) {
                    push_error("Conversion initialization failed.");
                    return 1;
                }
                break;
            case format:
                break;
        }
        ret = SetChannelTRMC(&channel);
        if (ret) {
            push_error(const_name(ret, error_codes));
            return 1;
        }
    }

    return 0;
}


/***********************************************************************
 * Miscellaneous commands.
 */

int should_quit = 0;

static int idn(void *client, unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    if (!cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed *idn command");
        return 1;
    }
    queue_output(client, IDN);
    return 0;
}

/* syntax: "start serial_port_number, frequency" */
static int start(unused(void *client), unused(int cmd_data),
        parsed_command *cmd)
{
    INITSTRUCTURE init;

    /* Sanity check. */
    assert(cmd->n_tok == 1);
    if (cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 2) {
        push_error("Malformed start command");
        return 1;
    }

    /* Parse parameters. */
    int port = atoi(cmd->param[0]);
    if (port == 1)
        init.Com = _COM1;
    else if (port == 2)
        init.Com = _COM2;
    else {
        push_error("Invalid serial port number");
        return 1;
    }
    int freq = atoi(cmd->param[1]);
    if (freq == 0)
        init.Frequency = _NOTBEATING;
    else if (freq == 50)
        init.Frequency = _50HZ;
    else if (freq == 60)
        init.Frequency = _60HZ;
    else {
        push_error("Invalid frequency");
        return 1;
    }

    /* Proceed to start the TRMC2. */
    int ret = StartTRMC(&init);
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    return 0;
}

static int stop(unused(void *client), unused(int cmd_data),
        parsed_command *cmd)
{
    /* Sanity check. */
    assert(cmd->n_tok == 1);
    if (cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        push_error("Malformed stop command");
        return 1;
    }

    int ret = StopTRMC();
    if (ret) {
        push_error(const_name(ret, error_codes));
        return 1;
    }
    return 0;
}

static int quit(unused(void *client),
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

#define END_OF_LIST {NULL, NULL, 0, NULL}

const syntax_tree trmc2_syntax[] = {
    {"*idn", idn, 0, NULL},
    {"start", start, 0, NULL},
    {"stop", stop, 0, NULL},
    {"board", NULL, 0, (syntax_tree[]) {
        {"count", get_number, nb_boards, NULL},
        {"type", board_handler, b_type, NULL},
        {"address", board_handler, b_address, NULL},
        {"status", board_handler, b_status, NULL},
        {"calibration", board_handler, b_calibration, NULL},
        {"vranges", board_handler, b_vranges, NULL},
        {"vrange", board_handler, b_vrange, NULL},
        {"iranges", board_handler, b_iranges, NULL},
        {"irange", board_handler, b_irange, NULL},
        END_OF_LIST
    }},
    {"channel", NULL, 0, (syntax_tree[]) {
        {"count", get_number, nb_channels, NULL},
        {"voltage", NULL, 0, (syntax_tree[]) {
            {"range", channel_handler, c_vrange, NULL},
            END_OF_LIST
        }},
        {"current", NULL, 0, (syntax_tree[]) {
            {"range", channel_handler, c_irange, NULL},
            END_OF_LIST
        }},
        {"addresses", channel_handler, c_address, NULL},
        {"type", channel_handler, c_type, NULL},
        {"mode", channel_handler, c_mode, NULL},
        {"averaging", channel_handler, c_avg, NULL},
        {"scrutationtime", channel_handler, c_scrutation, NULL},
        {"priority", channel_handler, c_priority, NULL},
        {"fifosize", channel_handler, c_fifosz, NULL},
        {"conversion", channel_handler, c_conversion, NULL},
        {"measure", channel_handler, measure, (syntax_tree[]) {
            {"format", channel_handler, format, NULL},
            END_OF_LIST
        }},
        END_OF_LIST
    }},
    {"regulation", NULL, 0, (syntax_tree[]) {
        {"setpoint", NULL, 0, NULL},
        {"p", NULL, 0, NULL},
        {"i", NULL, 0, NULL},
        {"d", NULL, 0, NULL},
        {"max", NULL, 0, NULL},
        {"resistance", NULL, 0, NULL},
        {"channel", NULL, 0, NULL},
        END_OF_LIST
    }},
    {"error", get_error, 0, (syntax_tree[]) {
        {"count", error_count, 0, NULL},
        {"clear", clear_errors, 0, NULL},
        END_OF_LIST
    }},
    {"quit", quit, 0, NULL},
    END_OF_LIST
};
