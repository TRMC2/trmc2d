/*
 * Text <-> value mappings for the #defines and enums of Trmc.h.
 *
 * All the tables below should be NULL-name terminated.
 */

#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "parse.h"      /* for parse_errors */

define const Com_names[] = {{"_COM1", 1}, {"_COM2", 2}, {NULL, 0}};

define const Frequency_names[] = {
    {"_NOTBEATING", 0}, {"_50HZ", 1}, {"_60HZ", 2}, {NULL, 0}
};

define const bywhat_names[] = {{"_BYINDEX", 1}, {"_BYADDRESS", 2}, {NULL, 0}};

define const BoardType_names[] = {
    {"_TYPEREGULMAIN", 0}, {"_TYPEREGULAUX", 1},
    {"_TYPEA", 2}, {"_TYPEB", 3}, {"_TYPEC", 4}, {"_TYPED", 5},
    {"_TYPEE", 6}, {"_TYPEF", 7}, {"_TYPEG", 8}, {NULL, 0}
};

define const Mode_names[] = {
    {"_INIT_MODE", -2}, {"_NOT_USED_MODE", -1}, {"_FIX_RANGE_MODE", 0},
    {"_FIX_CURRENT_MODE", 1}, {"_FIX_VOLTAGE_MODE", 2},
    {"_PRIORITY_CURRENT_MODE", 3}, {"_PRIORITY_VOLTAGE_MODE", 4},
    {"_SPECIAL_MODE", 5}, {NULL, 0}
};

define const Priority_names[] = {
    {"_NO_PRIORITY", 0}, {"_PRIORITY", 1}, {"_ALWAYS", 2}, {NULL, 0}
};

define const Index_names[] = {
    {"_REGULMAINBOARD", 0},
    {"_REGULAUXBOARD", 1},
    {"_FIRSTBOARD", 2},
    {NULL, 0}
};

define const tristate_names[] = {{"_AUTOMATIC", -1}, {"_NO", 0}, {"_YES", 1}};

define const board_mode_names[] = {
    {"_CALIBRATION_FAILED", -1},
    {"_NORMAL_MODE", 0},
    {"_START_CALIBRATION_MODE", 2},
    {"_CALIBRATION_MODE", 1},
    {NULL, 0}
};

define const error_codes[] = {
    {"_TIMER_NOT_RUNNING", 4},
    {"_TIMER_ALREADY_RUNNING", 3},
    {"_WRONG_RANGEINDEX", 2},
    {"_CHANNEL_HAS_BEEN_MODIFIED", 1},
    {"_RETURN_OK", 0},
    {"_TRMC_NOT_INITIALIZED", -25},
    {"_NO_SUCH_BOARD", -43},
    {"_NO_BOARD_AT_THIS_ADDRESS", -16},
    {"_NO_BOARD_WITH_THIS_INDEX", -27},
    {"_NO_SUCH_CHANNEL", -19},
    {"_INVALID_SUBADDRESS", -18},
    {"_INVALID_MODE", -20},
    {"_INVALID_PRIORITY", -21},
    {"_INVALID_BYWHAT", -26},
    {"_INVALID_ADDRESS", -28},
    {"_RANGE_CHANGE_NOT_POSSIBLE", -12},
    {"_WRONG_MODE_IN_RANGE", -15},
    {"_BOARD_IN_CALIBRATION", -42},
    {"_INVALID_CALIBRATION_PARAMETER", -47},
    {"_INVALID_CALIBRATION_STATUS", -46},
    {"_CALIBRATION_FAILED", -1},
    {"_NOT_USED_CHANNEL_AND_CALIBRATION_INCOMPATIBLE", -5},
    {"_NO_PRIORITY_WITH_ZERO_SCRUTATION", -49},
    {"_NO_SUCH_REGULATION", -50},
    {"_INVALID_REGULPARAMETER", -51},
    {"_INVALID_CHANNELPARAMETER", -52},
    {"_HEATINGMAX_TOO_LARGE", -53},
    {"_CHANNEL_NOT_IN_USE", -38},
    {"_INVALID_COM", -45},
    {"_INVALID_FREQUENCY", -48},
    {"_CANNOT_ALLOCATE_MEM", -6 },
    {"_COMM_NOT_ESTABLISH", -36},
    {"_COM_NOT_AVAILABLE", -35},
    {"_14_NOT_ANSWERWED", -2 },
    {"_14_ANSWERWED", -3 },
    {"_WRONG_CODE_IN_BASE", -4 },
    {"_WRONG_ANSWER_IN_BASE", -37 },
    {"_TIMER_NOT_CAPABLE", -29},
    {"_INTERNAL_INCONSISTENCY", -44},
    {NULL, 0}
};

define const parse_errors[] = {
    {"EMPTY_COMMAND", EMPTY_COMMAND},
    {"TOO_MANY_TOKENS_IN_COMMAND", TOO_MANY_TOKENS_IN_COMMAND},
    {"NO_SUCH_COMMAND", NO_SUCH_COMMAND},
    {"NO_HANDLER", NO_HANDLER},
    {NULL, 0}
};

/* Get the #defined value from the name. */
int lookup(const char *name, const define *table)
{
    int i;

    for (i = 0; table[i].name; i++)
        if (strcmp(table[i].name, name) == 0) return table[i].value;
    fprintf(stderr, "Could not find \"%s\" in table {\"%s\", ...}\n",
            name, table[0].name);
    return 0;
}

/* Get the #defined name from the value. */
const char *const_name(int value, const define *table)
{
    int i;
    static char buffer[256];

    for (i = 0; table[i].name; i++)
        if (table[i].value == value) return table[i].name;
    snprintf(buffer, sizeof buffer, "(%d)", value);
    return buffer;
}
