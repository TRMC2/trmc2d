/*
 * Glue code for the conversion functions.
 */

typedef int (*Etalon)(double*);

extern char *plugindir;

Etalon convert_init(int argc, char **argv);

void convert_cleanup(Etalon f);
