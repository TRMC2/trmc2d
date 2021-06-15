/*
 * Text <-> value mappings for the #defines and enums of Trmc.h.
 */

typedef struct {
    const char *name;
    int value;
} define;

extern const define Com_names[];
extern const define Frequency_names[];
extern const define bywhat_names[];
extern const define BoardType_names[];
extern const define Mode_names[];
extern const define Priority_names[];
extern const define Index_names[];
extern const define tristate_names[];
extern const define board_mode_names[];
extern const define error_codes[];
extern const define parse_errors[];

/* Get the #defined name from the value. */
const char *const_name(int value, const define *table);
