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
#include "io.h"
#include "interpreter.h"
#include "plugin.h"

/* Instrument identification. */
#define IDN "tempd temperature server, Institut NEEL, version " VERSION

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
 * Report an error, either immediately or through the error stack, as
 * per the client's choice. The error message should be a static string,
 * without the CRLF termination.
 */
void report_error(client_t *client, const char *err)
{
    if (client->verbose) {
        /* In verbose mode, the error is reported immediately. */
        queue_output(client, "ERROR: %s\r\n", err);
    } else {
        /* Otherwise it is sent to the error stack. */
        assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
        if (error_sp == MAX_ERRORS)
            error_stack[error_sp-1] = "Error stack overflow";
        else
            error_stack[error_sp++] = err;
    }
}

static int get_error(void *client,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    assert(error_sp >= 0 && error_sp <= MAX_ERRORS);
    if (!cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        report_error(client, "Malformed error command");
        return 1;
    }
    queue_output(client, "%s\r\n",
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
        report_error(client, "Malformed error command");
        return 1;
    }
    queue_output(client, "%d\r\n", error_sp);
    return 0;
}

static int clear_errors(void *client,
        unused(int cmd_data), parsed_command *cmd)
{
    assert(cmd->n_tok == 2);
    if (cmd->query || cmd->suffix[0] != -1
            || cmd->suffix[1] != -1 || cmd->n_param != 0) {
        report_error(client, "Malformed error command");
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
    b_calibration, b_vranges_cnt, b_vranges, b_iranges_cnt, b_iranges,
    c_vrange, c_irange, c_address, c_type, c_mode, c_avg, c_polling,
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
        report_error(client, "Malformed count command");
        return 1;
    }
    if (cmd_data == nb_boards)
        ret = GetNumberOfBoardTRMC(&n);
    else  /* cmd_data == nb_channels */
        ret = GetNumberOfChannelTRMC(&n);
    if (ret) {
        report_error(client, const_name(ret, error_codes));
        return 1;
    }
    queue_output(client, "%d\r\n", n);
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
    assert(((cmd_data == b_vranges_cnt || cmd_data == b_iranges_cnt)
            && cmd->n_tok == 3) || cmd->n_tok == 2);
    index = cmd->suffix[0];
    if (index == -1 || cmd->suffix[1] != -1
            || (cmd->query && cmd->n_param != 0)
            || (!cmd->query && cmd->n_param != 1)) {
        report_error(client, "Malformed board command");
        return 1;
    }
    if (!cmd->query && cmd_data != b_calibration) {
        report_error(client, "Read-only parameter");
        return 1;
    }
    board.Index = index;
    ret =  GetBoardTRMC(_BYINDEX, &board);
    if (ret) {
        report_error(client, const_name(ret, error_codes));
        return 1;
    }
    if (cmd->query) switch (cmd_data) {
        case b_type:
            queue_output(client, "%d (%s)\r\n", board.TypeofBoard,
                    const_name(board.TypeofBoard, BoardType_names));
            break;
        case b_address:
            queue_output(client, "%d\r\n", board.AddressofBoard);
            break;
        case b_status:
            queue_output(client, "%d (%s)\r\n", board.CalibrationStatus,
                    const_name(board.CalibrationStatus, board_mode_names));
            break;
        case b_calibration:
            queue_output(client, "%d\r\n", board.NumberofCalibrationMeasure);
            break;
        case b_vranges_cnt:
            queue_output(client, "%d\r\n", board.NumberofVRanges);
            break;
        case b_iranges_cnt:
            queue_output(client, "%d\r\n", board.NumberofIRanges);
            break;
        case b_vranges:
            for (i=0; i<board.NumberofVRanges; i++) {
                queue_output(client, "%g", board.VRangesTable[i]);
                if (i < board.NumberofVRanges - 1)
                    queue_output(client, ",");
            }
            queue_output(client, "\r\n");
            break;
        case b_iranges:
            for (i=0; i<board.NumberofIRanges; i++) {
                queue_output(client, "%g", board.IRangesTable[i]);
                if (i < board.NumberofIRanges - 1)
                    queue_output(client, ",");
            }
            queue_output(client, "\r\n");
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

/*
 * The format string is a NULL-terminated array of chars having the
 * values below.
 */
enum { RAW = 1, MEAS, RANGEI, RANGEV, TIME, STATUS, NUMBER, COUNT };

/*
 * Default formats. The first one is used if a conversion function has
 * been defined.
 */
const char format_raw_meas[] = { RAW, MEAS, 0 };
const char format_raw[]      = { RAW, 0 };

/*
 * Convert a single item of a user-supplied format specifier (a string)
 * to our internal representation (a char). Returns 0 if invalid.
 */
static char parse_format_item(const char *fmt)
{
    if (strcasecmp(fmt, "raw")       == 0) return RAW;
    if (strcasecmp(fmt, "converted") == 0) return MEAS;
    if (strcasecmp(fmt, "range_i")   == 0) return RANGEI;
    if (strcasecmp(fmt, "range_v")   == 0) return RANGEV;
    if (strcasecmp(fmt, "time")      == 0) return TIME;
    if (strcasecmp(fmt, "status")    == 0) return STATUS;
    if (strcasecmp(fmt, "number")    == 0) return NUMBER;
    if (strcasecmp(fmt, "count")     == 0) return COUNT;
    return 0;  /* invalid */
}

/* Convert a format to a printable string and send it to the client. */
static void queue_format(client_t *cl, const char *format)
{
    size_t n = strlen(format);
    for (size_t i = 0; i < n; i++) {
        switch (format[i]) {
            case RAW:    queue_output(cl, "raw");       break;
            case MEAS:   queue_output(cl, "converted"); break;
            case RANGEI: queue_output(cl, "range_i");   break;
            case RANGEV: queue_output(cl, "range_v");   break;
            case TIME:   queue_output(cl, "time");      break;
            case STATUS: queue_output(cl, "status");    break;
            case NUMBER: queue_output(cl, "number");    break;
            case COUNT:  queue_output(cl, "count");     break;
        }
        if (i < n - 1) queue_output(cl, ",");
    }
    queue_output(cl, "\r\n");
}

/* Send a measurement as per the requested format. */
static void queue_measurement(client_t *cl,
        const char *format, AMEASURE *m, int count)
{
    size_t n = strlen(format);
    for (size_t i = 0; i < n; i++) {
        switch (format[i]) {
            case RAW:    queue_output(cl, "%g", m->MeasureRaw);  break;
            case MEAS:   queue_output(cl, "%g", m->Measure);     break;
            case RANGEI: queue_output(cl, "%g", m->ValueRangeI); break;
            case RANGEV: queue_output(cl, "%g", m->ValueRangeV); break;
            case TIME:   queue_output(cl, "%d", m->Time);        break;
            case STATUS: queue_output(cl, "%d", m->Status);      break;
            case NUMBER: queue_output(cl, "%d", m->Number);      break;
            case COUNT:  queue_output(cl, "%d", count);          break;
        }
        if (i < n - 1) queue_output(cl, ",");
    }
    queue_output(cl, "\r\n");
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
            || (cmd->query && cmd->n_param != 0)) {
        report_error(client, "Malformed channel command");
        return 1;
    }
    if (!cmd->query) {
        int n_param_ok;
        switch (cmd_data) {
            case c_address:
            case c_type:
            case measure:
                report_error(client, "Read-only parameter");
                return 1;
            case c_conversion:
                n_param_ok = cmd->n_param == 2 || cmd->n_param == 3;
                break;
            case format:
                n_param_ok = cmd->n_param >= 1;
                break;
            default:
                n_param_ok = cmd->n_param == 1;
        }
        if (!n_param_ok) {
            report_error(client, "Bad parameter count");
            return 1;
        }
    }
    channel.Index = index;
    ret =  GetChannelTRMC(_BYINDEX, &channel);
    if (ret) {
        report_error(client, const_name(ret, error_codes));
        return 1;
    }
    if (cmd->query) switch (cmd_data) {
        case c_vrange:
            queue_output(client, "%g\r\n", channel.ValueRangeV);
            break;
        case c_irange:
            queue_output(client, "%g\r\n", channel.ValueRangeI);
            break;
        case c_address:
            queue_output(client, "%d, %d\r\n",
                    channel.BoardAddress, channel.SubAddress);
            break;
        case c_type:
            queue_output(client, "%d (%s)\r\n", channel.BoardType,
                    const_name(channel.BoardType, BoardType_names));
            break;
        case c_mode:
            queue_output(client, "%d (%s)\r\n", channel.Mode,
                    const_name(channel.Mode, Mode_names));
            break;
        case c_avg:
            queue_output(client, "%d\r\n", channel.PreAveraging);
            break;
        case c_polling:
            queue_output(client, "%d\r\n", channel.ScrutationTime);
            break;
        case c_priority:
            queue_output(client, "%d (%s)\r\n", channel.PriorityFlag,
                    const_name(channel.PriorityFlag, Priority_names));
            break;
        case c_fifosz:
            queue_output(client, "%d\r\n", channel.FifoSize);
            break;
        case c_conversion:
            channel_extras = get_channel_extras(index);
            if (!channel_extras->conversion) {
                report_error(client, "Channel has no conversion.");
                return 1;
            }
            queue_output(client, "%s\r\n", channel_extras->conversion);
            break;
        case format:
            channel_extras = get_channel_extras(index);
            if (channel_extras->format)
                queue_format(client, channel_extras->format);
            else
                queue_output(client, "No format defined.\r\n");
            break;
        case measure:
            ret = ReadValueTRMC(index, &meas);
            /*
             * A positive return value is the number of data points in
             * the FIFO before the read. A negative value is an error
             * code.
             */
            if (ret < 0) {
                report_error(client, const_name(ret, error_codes));
                return 1;
            }
            if (ret == 0) {
                report_error(client, "Measurement queue empty.");
                return 1;
            }
            channel_extras = get_channel_extras(index);
            const char *format = channel_extras->format;
            if (!format) {
                if (channel.Etalon) format = format_raw_meas;
                else format = format_raw;
            }
            queue_measurement(client, format, &meas, ret);
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
            case c_polling:
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
                        strcat(channel_extras->conversion, ",");
                }

                /* Use the give conversion. */
                if (channel.Etalon) convert_cleanup(channel.Etalon);
                channel.Etalon = convert_init(cmd->n_param, cmd->param);
                if (!channel.Etalon) {
                    report_error(client, "Conversion initialization failed.");
                    return 1;
                }
                break;
            case format:
                channel_extras = get_channel_extras(index);
                char *format = malloc(cmd->n_param + 1);
                if (channel_extras->format) free(channel_extras->format);
                channel_extras->format = format;
                for (int i = 0; i < cmd->n_param; i++) {
                    format[i] = parse_format_item(cmd->param[i]);
                    if (!format[i]) {
                        report_error(client, "Invalid format.");
                        free(format);
                        channel_extras->format = NULL;
                        return 1;
                    }
                }
                format[cmd->n_param] = '\0';
                break;
        }
        ret = SetChannelTRMC(&channel);
        if (ret) {
            report_error(client, const_name(ret, error_codes));
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
        report_error(client, "Malformed *idn command");
        return 1;
    }
    queue_output(client, "%s\r\n", IDN);
    return 0;
}

static int help(void *client, unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    /* Do not insist on having '?' on the help command. */
    if (cmd->suffix[0] != -1 || cmd->n_param > 1) {
        report_error(client, "Malformed help command");
        return 1;
    }
    if (cmd->n_param == 0)
        queue_output(client, "%s",
        "*idn?          - return the server identification string\r\n"
        "help? [topic]  - display help on topic (or this general help)\r\n"
        "    available topics: board, channel, regulation\r\n"
        "verbose N      - set (N = 1) or clear (N = 0) verbose mode\r\n"
        "start freq [,port] - start the TRMC2\r\n"
        "stop           - stop the periodic timer\r\n"
        "board:count?   - return the number of boards\r\n"
        "board<i>:      - prefix for commands addressing board i\r\n"
        "channel:count? - return the number of channels\r\n"
        "channel<i>:    - prefix for commands addressing channel i\r\n"
        "regulation<i>: - prefix for commands addressing regulation i\r\n"
        "error?         - pop and return last error from the error stack\r\n"
        "error:count?   - return number of errors in the stack\r\n"
        "error:clear    - clear the error stack\r\n"
        "quit           - stop the server\r\n"
        );
    else if (strcmp(cmd->param[0], "board") == 0)
        queue_output(client, "%s",
        "Board commands (should be prefixed with 'board<i>:'):\r\n"
        "type?            - return board type\r\n"
        "address?         - return board address\r\n"
        "status?          - return board status\r\n"
        "calibration file - use the file as a calibration table\r\n"
        "vranges:count?   - return the number of voltage ranges\r\n"
        "vranges?         - list the voltage ranges\r\n"
        "iranges:count?   - return the number of current ranges\r\n"
        "iranges?         - list the current ranges\r\n"
        );
    else if (strcmp(cmd->param[0], "channel") == 0)
        queue_output(client, "%s",
        "Channel commands (should be prefixed with 'channel<i>:'):\r\n"
        "type?           - return type of board hosting the channel\r\n"
        "address?        - return the board and channel address\r\n"
        "voltage:range V - set the voltage range\r\n"
        "current:range I - set the current range\r\n"
        "mode N          - set the channel mode\r\n"
        "averaging N     - set the averaging count\r\n"
        "polling N       - set the polling count\r\n"
        "priority N      - set the priority mode\r\n"
        "fifosize N      - set the FIFO size\r\n"
        "conversion plugin,function,initialization - define a conversion\r\n"
        "measure:format list - define the measurement format\r\n"
        "    possible list items: raw, converted, range_i, range_v,\r\n"
        "    time, status, number, count\r\n"
        "measure?        - return a measurement\r\n"
        );
    else if (strcmp(cmd->param[0], "regulation") == 0)
        queue_output(client, "%s",
        "Regulation commands (should be prefixed with 'regulation<i>:'):\r\n"
        "setpoint T   - define temperature setpoint\r\n"
        "p val        - set P coefficient\r\n"
        "i val        - set I coefficient\r\n"
        "d val        - set D coefficient\r\n"
        "max val      - set maximum heating power\r\n"
        "resistance R - set resistance of heating resistor\r\n"
        "channel<i>:weight W - set weight of channel i\r\n"
        );
    else {
        report_error(client, "Invalid help topic");
        return 1;
    }
    return 0;
}

static int verbose(void *client, unused(int cmd_data), parsed_command *cmd)
{
    assert(client != NULL);
    assert(cmd->n_tok == 1);
    if (cmd->suffix[0] != -1 || (cmd->query && cmd->n_param != 0)
            || (!cmd->query && cmd->n_param != 1)) {
        report_error(client, "Malformed verbose command");
        return 1;
    }
    if (!cmd->query)
        ((client_t *) client)->verbose = atoi(cmd->param[0]) != 0;
    else
        queue_output(client, "%d\r\n", ((client_t *) client)->verbose);
    return 0;
}

/* syntax: "start frequency [, serial_port_number]" */
static int start(void *client, unused(int cmd_data),
        parsed_command *cmd)
{
    INITSTRUCTURE init;

    /* Sanity check. */
    assert(cmd->n_tok == 1);
    if (cmd->query || cmd->suffix[0] != -1
            || cmd->n_param < 1 || cmd->n_param > 2) {
        report_error(client, "Malformed start command");
        return 1;
    }

    /* Parse parameters. */
    int freq = atoi(cmd->param[0]);
    if (freq == 0)
        init.Frequency = _NOTBEATING;
    else if (freq == 50)
        init.Frequency = _50HZ;
    else if (freq == 60)
        init.Frequency = _60HZ;
    else {
        report_error(client, "Invalid frequency");
        return 1;
    }
    int port = cmd->n_param>1 ? atoi(cmd->param[1]) : 1;
    if (port == 1)
        init.Com = _COM1;
    else if (port == 2)
        init.Com = _COM2;
    else {
        report_error(client, "Invalid serial port number");
        return 1;
    }

    /* Proceed to start the TRMC2. */
    int ret = StartTRMC(&init);
    if (ret) {
        report_error(client, const_name(ret, error_codes));
        return 1;
    }
    return 0;
}

static int stop(void *client, unused(int cmd_data), parsed_command *cmd)
{
    /* Sanity check. */
    assert(cmd->n_tok == 1);
    if (cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        report_error(client, "Malformed stop command");
        return 1;
    }

    int ret = StopTRMC();
    if (ret) {
        report_error(client, const_name(ret, error_codes));
        return 1;
    }
    return 0;
}

static int quit(void *client, unused(int cmd_data), parsed_command *cmd)
{
    if (cmd->query || cmd->suffix[0] != -1 || cmd->n_param != 0) {
        report_error(client, "Malformed quit command");
        return 1;
    }
    should_quit = 1;
    return 0;
}


/***********************************************************************
 * Raw access to libtrmc2.
 */

enum { Start, Stop, GetError, GetNumberOfChannel, GetChannel,
    SetChannel, GetRegulation, SetRegulation, GetNumberOfBoard,
    GetBoard, SetBoard, ReadValue };

static int raw_command(void *client, int cmd_data, parsed_command *cmd)
{
    int request_id = 0;
    if (cmd->n_param < 1) goto bad_arg_count;
    request_id = atoi(cmd->param[0]);
    switch (cmd_data) {
        case Start: {
            if (cmd->n_param != 4) goto bad_arg_count;
            INITSTRUCTURE init; // the field `futureuse' is ignored
            init.Com =               atoi(cmd->param[1]);
            init.Frequency =         atoi(cmd->param[2]);
            init.CommunicationTime = atoi(cmd->param[3]);
            int ret = StartTRMC(&init);
            queue_output(client, "%d,%d,%d,%d,%d\r\n", request_id, ret,
                    init.Com,
                    init.Frequency,
                    init.CommunicationTime);
            break;
        }
        case Stop: {
            if (cmd->n_param != 1) goto bad_arg_count;
            int ret = StopTRMC();
            queue_output(client, "%d,%d\r\n", request_id, ret);
            break;
        }
        case GetError: {
            if (cmd->n_param != 1) goto bad_arg_count;
            ERRORS errors;
            int ret = GetSynchroneousErrorTRMC(&errors);
            queue_output(client, "%d,%d,%d,%d,%d,%d\r\n", request_id, ret,
                    errors.CommError,
                    errors.CalcError,
                    errors.TimerError,
                    errors.Date);
            break;
        }
        case GetNumberOfChannel: {
            if (cmd->n_param != 1) goto bad_arg_count;
            int channel_count;
            int ret = GetNumberOfChannelTRMC(&channel_count);
            queue_output(client, "%d,%d,%d\r\n", request_id, ret,
                    channel_count);
            break;
        }
        case GetChannel:
        case SetChannel:
        {
            int is_getter = cmd_data == GetChannel;
            if (cmd->n_param != 13 + is_getter) goto bad_arg_count;
            CHANNELPARAMETER channel;
            int bywhat;
            if (is_getter)
                bywhat = atoi(cmd->param[1]);
            int N = 1 + is_getter;
            strncpy(channel.name, cmd->param[N+0], _LENGTHOFNAME);
            channel.ValueRangeI =    atof(cmd->param[N+1]);
            channel.ValueRangeV =    atof(cmd->param[N+2]);
            channel.BoardAddress =   atoi(cmd->param[N+3]);
            channel.SubAddress =     atoi(cmd->param[N+4]);
            channel.BoardType =      atoi(cmd->param[N+5]);
            channel.Index =          atoi(cmd->param[N+6]);
            channel.Mode =           atoi(cmd->param[N+7]);
            channel.PreAveraging =   atoi(cmd->param[N+8]);
            channel.ScrutationTime = atoi(cmd->param[N+9]);
            channel.PriorityFlag =   atoi(cmd->param[N+10]);
            channel.FifoSize =       atoi(cmd->param[N+11]);
            int ret;
            if (is_getter) {
                ret = GetChannelTRMC(bywhat, &channel);
            } else {
                /* Preserve the field `Etalon'. */
                CHANNELPARAMETER channel_old = channel;
                GetChannelTRMC(_BYINDEX, &channel_old);
                channel.Etalon = channel_old.Etalon;
                ret = SetChannelTRMC(&channel);
            }
            queue_output(client,
                    "%d,%d,%s,%e,%e,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
                    request_id, ret,
                    channel.name,
                    channel.ValueRangeI,
                    channel.ValueRangeV,
                    channel.BoardAddress,
                    channel.SubAddress,
                    channel.BoardType,
                    channel.Index,
                    channel.Mode,
                    channel.PreAveraging,
                    channel.ScrutationTime,
                    channel.PriorityFlag,
                    channel.FifoSize);
            break;
        }
        case GetRegulation:
        case SetRegulation:
        {
            if (cmd->n_param != 11 + 2 * _NB_REGULATING_CHANNEL)
                goto bad_arg_count;
            REGULPARAMETER r;
            strncpy(r.name, cmd->param[1], _LENGTHOFNAME);
            r.SetPoint =        atof(cmd->param[2]);
            r.P =               atof(cmd->param[3]);
            r.I =               atof(cmd->param[4]);
            r.D =               atof(cmd->param[5]);
            r.HeatingMax =      atof(cmd->param[6]);
            r.HeatingResistor = atof(cmd->param[7]);
            int N = 8;
            for (int i = 0; i < _NB_REGULATING_CHANNEL; i++)
                r.WeightofChannel[i] = atof(cmd->param[N+i]);
            N += _NB_REGULATING_CHANNEL;
            for (int i = 0; i < _NB_REGULATING_CHANNEL; i++)
                r.IndexofChannel[i] =  atoi(cmd->param[N+i]);
            N += _NB_REGULATING_CHANNEL;
            r.Index =           atoi(cmd->param[N+0]);
            r.ThereIsABooster = atoi(cmd->param[N+1]);
            r.ReturnTo0 =       atoi(cmd->param[N+2]);
            int ret;
            if (cmd_data == GetRegulation)
                ret = GetRegulationTRMC(&r);
            else
                ret = SetRegulationTRMC(&r);
            queue_output(client, "%d,%d,%s,%e,%e,%e,%e,%e,%e,",
                    request_id, ret,
                    r.name,
                    r.SetPoint,
                    r.P,
                    r.I,
                    r.D,
                    r.HeatingMax,
                    r.HeatingResistor);
            for (int i = 0; i < _NB_REGULATING_CHANNEL; i++)
                queue_output(client, "%e,", r.WeightofChannel[i]);
            for (int i = 0; i < _NB_REGULATING_CHANNEL; i++)
                queue_output(client, "%d,", r.IndexofChannel[i]);
            queue_output(client, "%d,%d,%d\r\n",
                    r.Index,
                    r.ThereIsABooster,
                    r.ReturnTo0);
            break;
        }
        case GetNumberOfBoard: {
            if (cmd->n_param != 1) goto bad_arg_count;
            int board_count;
            int ret = GetNumberOfBoardTRMC(&board_count);
            queue_output(client, "%d,%d,%d\r\n", request_id, ret,
                    board_count);
            break;
        }
        case GetBoard:
        case SetBoard:
        {
            int is_getter = cmd_data == GetBoard;
            if (cmd->n_param < 8 + is_getter) goto bad_arg_count;
            BOARDPARAMETER board;
            int bywhat;
            if (is_getter)
                bywhat = atoi(cmd->param[1]);
            int N = 1 + is_getter;
            board.TypeofBoard =                atoi(cmd->param[N+0]);
            board.AddressofBoard =             atoi(cmd->param[N+1]);
            board.Index =                      atoi(cmd->param[N+2]);
            board.CalibrationStatus =          atoi(cmd->param[N+3]);
            board.NumberofCalibrationMeasure = atoi(cmd->param[N+4]);
            board.NumberofIRanges =            atoi(cmd->param[N+5]);
            board.NumberofVRanges =            atoi(cmd->param[N+6]);
            N += 7;
            if (cmd->n_param !=
                    N + board.NumberofCalibrationMeasure +
                    board.NumberofIRanges + board.NumberofVRanges)
                goto bad_arg_count;
            for (int i = 0; i < board.NumberofCalibrationMeasure; i++)
                board.CalibrationTable[i] = atof(cmd->param[N+i]);
            N += board.NumberofCalibrationMeasure;
            for (int i = 0; i < board.NumberofIRanges; i++)
                board.IRangesTable[i] = atof(cmd->param[N+i]);
            N += board.NumberofIRanges;
            for (int i = 0; i < board.NumberofVRanges; i++)
                board.VRangesTable[i] = atof(cmd->param[N+i]);
            int ret;
            if (is_getter)
                ret = GetBoardTRMC(bywhat, &board);
            else
                ret = SetBoardTRMC(&board);
            queue_output(client, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    request_id, ret,
                    board.TypeofBoard,
                    board.AddressofBoard,
                    board.Index,
                    board.CalibrationStatus,
                    board.NumberofCalibrationMeasure,
                    board.NumberofIRanges,
                    board.NumberofVRanges);
            for (int i = 0; i < board.NumberofCalibrationMeasure; i++)
                queue_output(client, ",%e", board.CalibrationTable[i]);
            for (int i = 0; i < board.NumberofIRanges; i++)
                queue_output(client, ",%e", board.IRangesTable[i]);
            for (int i = 0; i < board.NumberofVRanges; i++)
                queue_output(client, ",%e", board.VRangesTable[i]);
            queue_output(client, "\r\n");
            break;
        }
        case ReadValue: {
            if (cmd->n_param != 2) goto bad_arg_count;
            int index =  atoi(cmd->param[1]);
            AMEASURE measure;
            int ret = ReadValueTRMC(index, &measure);
            queue_output(client, "%d,%d,%e,%e,%e,%e,%d,%d,%d,%d\r\n",
                    request_id, ret,
                    measure.MeasureRaw,
                    measure.Measure,
                    measure.ValueRangeI,
                    measure.ValueRangeV,
                    measure.Time,
                    measure.Status,
                    measure.Number,
                    measure.Nothing);
            break;
        }
    }
    return 0;
bad_arg_count:
    queue_output(client, "%d,Error: bad argument count\r\n", request_id);
    return 1;
}


/***********************************************************************
 * Language description.
 */

#define END_OF_LIST {NULL, NULL, 0, NULL}

const syntax_tree trmc2_syntax[] = {
    {"*idn", idn, 0, NULL},
    {"help", help, 0, NULL},
    {"verbose", verbose, 0, NULL},
    {"start", start, 0, NULL},
    {"stop", stop, 0, NULL},
    {"board", NULL, 0, (syntax_tree[]) {
        {"count", get_number, nb_boards, NULL},
        {"type", board_handler, b_type, NULL},
        {"address", board_handler, b_address, NULL},
        {"status", board_handler, b_status, NULL},
        {"calibration", board_handler, b_calibration, NULL},
        {"vranges", board_handler, b_vranges, (syntax_tree[]) {
            {"count", board_handler, b_vranges_cnt, NULL},
            END_OF_LIST
        }},
        {"iranges", board_handler, b_iranges, (syntax_tree[]) {
            {"count", board_handler, b_iranges_cnt, NULL},
            END_OF_LIST
        }},
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
        {"polling", channel_handler, c_polling, NULL},
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
    {"Start", raw_command, Start, NULL},
    {"Stop", raw_command, Stop, NULL},
    {"GetError", raw_command, GetError, NULL},
    {"GetNumberOfChannel", raw_command, GetNumberOfChannel, NULL},
    {"SetChannel", raw_command, SetChannel, NULL},
    {"GetChannel", raw_command, GetChannel, NULL},
    {"SetRegulation", raw_command, SetRegulation, NULL},
    {"GetRegulation", raw_command, GetRegulation, NULL},
    {"GetNumberOfBoard", raw_command, GetNumberOfBoard, NULL},
    {"GetBoard", raw_command, GetBoard, NULL},
    {"SetBoard", raw_command, SetBoard, NULL},
    {"ReadValue", raw_command, ReadValue, NULL},
    END_OF_LIST
};
