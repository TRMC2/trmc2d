/*
 * Glue code for loading the conversion functions as plugins.
 *
 * libtrmc2 expects us to give the conversion function as
 *
 *      int (*Etalon)(double*);
 *
 * while we expect plugins to give three functions:
 *
 *      void *convert_init(char *init_string);
 *      double *convert(double measure, void *data);
 *      void convert_cleanup(void *data);
 */

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "plugin.h"

#define NB_CONVERSION_FCS 34

static conversion_t conversion[NB_CONVERSION_FCS];

static int f(double *x, int n)
{
    conversion_t *c;
    double y;

    if (n < 0 || n >= NB_CONVERSION_FCS) return 1;
    c = &conversion[n];
    if (!c->used || !c->convert) return 1;
    y = c->convert(*x, c->data);
    if (y != y) return 1;  /* NaN */
    *x = y;
    return 0;
}

static int f00(double *x) { return f(x,  0); }
static int f01(double *x) { return f(x,  1); }
static int f02(double *x) { return f(x,  2); }
static int f03(double *x) { return f(x,  3); }
static int f04(double *x) { return f(x,  4); }
static int f05(double *x) { return f(x,  5); }
static int f06(double *x) { return f(x,  6); }
static int f07(double *x) { return f(x,  7); }
static int f08(double *x) { return f(x,  8); }
static int f09(double *x) { return f(x,  9); }
static int f10(double *x) { return f(x, 10); }
static int f11(double *x) { return f(x, 11); }
static int f12(double *x) { return f(x, 12); }
static int f13(double *x) { return f(x, 13); }
static int f14(double *x) { return f(x, 14); }
static int f15(double *x) { return f(x, 15); }
static int f16(double *x) { return f(x, 16); }
static int f17(double *x) { return f(x, 17); }
static int f18(double *x) { return f(x, 18); }
static int f19(double *x) { return f(x, 19); }
static int f20(double *x) { return f(x, 20); }
static int f21(double *x) { return f(x, 21); }
static int f22(double *x) { return f(x, 22); }
static int f23(double *x) { return f(x, 23); }
static int f24(double *x) { return f(x, 24); }
static int f25(double *x) { return f(x, 25); }
static int f26(double *x) { return f(x, 26); }
static int f27(double *x) { return f(x, 27); }
static int f28(double *x) { return f(x, 28); }
static int f29(double *x) { return f(x, 29); }
static int f30(double *x) { return f(x, 30); }
static int f31(double *x) { return f(x, 31); }
static int f32(double *x) { return f(x, 32); }
static int f33(double *x) { return f(x, 33); }

static Etalon f_table[NB_CONVERSION_FCS] = {
    f00, f01, f02, f03, f04, f05, f06, f07, f08, f09, f10, f11, f12,
    f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25,
    f26, f27, f28, f29, f30, f31, f32, f33
};

char* plugindir = ".";

/* argv = { plugin, convert_name [, init_data] } */
Etalon convert_init(int argc, char **argv)
{
    int n;
    conversion_t *c;
    char *convert_name, *init_data;
    char dlname[1024], init_name[256], cleanup_name[256];
    void *(*init)(char*);
    char *error;

    if (argc < 2 || argc > 3) return NULL;

    /* Get a free slot in the tables. */
    for (n = 0; n < NB_CONVERSION_FCS; n++)
        if (!conversion[n].used) break;
    if (n == NB_CONVERSION_FCS) return NULL;  /* no slot */
    c = &conversion[n];

    /* Build library and function names. */
    strcpy(dlname, plugindir);
    strcat(dlname, "/");
    strncat(dlname, argv[0], sizeof dlname - strlen(dlname) - 1);
    strncat(dlname, ".so", sizeof dlname - strlen(dlname) - 1);
    convert_name = argv[1];
    strncpy(init_name, convert_name, sizeof init_name - 1);
    init_name[sizeof init_name - 1] = '\0';
    strncat(init_name, "_init",
            sizeof init_name - strlen(init_name) - 1);
    strncpy(cleanup_name, convert_name, sizeof cleanup_name - 1);
    cleanup_name[sizeof cleanup_name - 1] = '\0';
    strncat(cleanup_name, "_cleanup",
            sizeof cleanup_name - strlen(cleanup_name) - 1);
    init_data = argc == 3 ? argv[2] : NULL;

    /* Load plugin. */
    c->plugin = dlopen(dlname, RTLD_NOW);
    if (!c->plugin) {
        error = dlerror();
        if (error) fprintf(stderr, "dlopen(): %s\n", error);
        return NULL;
    }
    c->convert = dlsym(c->plugin, convert_name);
    if (!c->convert) {
        error = dlerror();
        if (error) fprintf(stderr, "dlsym(): %s\n", error);
        return NULL;
    }
    init = dlsym(c->plugin, init_name);
    c->cleanup = dlsym(c->plugin, cleanup_name);

    /* Init plugin. */
    if (init) {
        c->data = init(init_data);
        if (!c->data) {
            fprintf(stderr, "%s() failed\n", init_name);
            return NULL;
        }
    }
    else c->data = NULL;

    return f_table[n];
}

void convert_cleanup(Etalon f)
{
    int n;
    conversion_t *c;

    if (!f) return;

    /* Find the correct slot in the tables. */
    for (n = 0; n < NB_CONVERSION_FCS; n++)
        if (f_table[n] == f) break;
    if (n == NB_CONVERSION_FCS) return;  /* no slot */
    c = &conversion[n];

    /* Cleanup if necessary. */
    if (c->cleanup) c->cleanup(c->data);

    /* Unload the plugin. */
    dlclose(c->plugin);

    /* Free the slot. */
    c->convert = NULL;
    c->data = NULL;
    c->cleanup = NULL;
}
