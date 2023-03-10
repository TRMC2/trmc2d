// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Standalone program for testing trmc2d conversion plugins.
 *
 * Usage:
 *   ./test-plugin plugin function parameters start stop step
 *
 * The requested plugin must be available in the current working
 * directory. The output is a table of converted values, as defined by
 * the arguments start, stop and step.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    /* Read the command line. */
    if (argc != 7) {
        fprintf(stderr, "Usage: %s plugin function parameters "
                "start stop step\n", argv[0]);
        return EXIT_FAILURE;
    }
    char plugin_name[strlen(argv[1]) + 6];
    strcpy(plugin_name, "./");  // prepend "./" to the provided name
    strcat(plugin_name, argv[1]);
    strcat(plugin_name, ".so");  // append ".so"
    char *function = argv[2];
    char *parameters = argv[3];
    double start = atof(argv[4]);
    double stop = atof(argv[5]);
    double step = atof(argv[6]);

    /* Load the plugin. */
    void *plugin = dlopen(plugin_name, RTLD_NOW);
    if (!plugin) {
        fprintf(stderr, "%s\n", dlerror());
        return EXIT_FAILURE;
    }
    char symbol[strlen(function) + 9];  // make room for "_cleanup"
    strcpy(symbol, function);
    double (*convert)(double, void *) = dlsym(plugin, symbol);
    if (!convert) {
        fprintf(stderr, "%s: could not find %s\n", plugin_name, symbol);
        return EXIT_FAILURE;
    }
    strcat(symbol, "_init");
    void *(*init)(char *) = dlsym(plugin, symbol);
    symbol[strlen(symbol) - 5] = '\0';  // remove trailing "_init"
    strcat(symbol, "_cleanup");
    void (*cleanup)(void *) = dlsym(plugin, symbol);

    /* Initialize. */
    void *data = NULL;
    if (init) {
        data = init(parameters);
        if (!data) {
            fprintf(stderr, "Initialization failed\n");
            return EXIT_FAILURE;
        }
    }

    /* Output a table of interpolated values. */
    for (double x = start; x < stop + step/2; x += step) {
        double y = convert(x, data);
        printf("%g\t%g\n", x, y);
    }

    /* Free memory. */
    if (cleanup)
        cleanup(data);

    return EXIT_SUCCESS;
}
