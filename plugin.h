/*
 * Glue code for the conversion functions.
 */

typedef int (*Etalon)(double*);

typedef struct {
    int used;           /* slot of the table used? */
    void *plugin;       /* handle returned by dlopen() */
    double (*convert)(double, void*);
    void (*cleanup)(void*);
    void *data;
} conversion_t;

extern char *plugindir;

Etalon convert_init(int argc, char **argv);

void convert_cleanup(Etalon f);
