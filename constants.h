/*
 * Text <-> value mappings for the #defines and enums of Trmc.h.
 */

typedef struct {
    char *name;
    int value;
} define;

extern define Com_names[];
extern define Frequency_names[];
extern define bywhat_names[];
extern define BoardType_names[];
extern define Mode_names[];
extern define Priority_names[];
extern define Index_names[];
extern define tristate_names[];
extern define board_mode_names[];
extern define error_codes[];
extern define parse_errors[];

/* Get the #defined value from the name. */
int lookup(char *name, define *table);

/* Get the #defined name from the value. */
char *const_name(int value, define *table);
