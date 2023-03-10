// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * interpolate.c: trmc2d plugin for interpolation, based on GSL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_errno.h>

#define NAN (0.0/0.0)   /* Not a Number. */

typedef struct {
    gsl_interp_accel *acc;
    gsl_spline *spline;
} spline_data;

/* Forward declaration. */
static void cleanup(void *data);

/*
 * init_string is the name of the file containing the conversion table.
 * Allocates and returns a conversion_table*.
 */
static void *init(const gsl_interp_type *type, char *init_string)
{
    FILE *f;
    char s[1024];
    int items;      /* items read by sscanf() */
    int n = 0;      /* length of the tables */
    double *x = NULL;
    double *y = NULL;
    int allocated = 0;      /* length alocated to x and y */
    spline_data *data = NULL;
    int err;

    /* Open the file for reading. */
    if (!init_string) return NULL;
    f = fopen(init_string, "r");
    if (!f) return NULL;

    /* Read by lines. */
    while (fgets(s, sizeof s, f)) {

        /* Skip comments. */
        if (*s == '\n' || *s == '#') continue;

        /* Expand the arrays if necessary. */
        if (allocated <= n) {
            allocated += 1024;
            x = realloc(x, allocated * sizeof *x);
            y = realloc(y, allocated * sizeof *y);
            if (!x || !y) goto error;
        }

        /* Parse. */
        items = sscanf(s, "%lf %lf", &x[n], &y[n]);
        if (items != 2) goto error;
        n++;
    }

    /* Ask the GSL to not abort on errors. */
    gsl_set_error_handler_off();

    /* Init data. */
    data = malloc(sizeof *data);
    if (!data) goto error;
    data->spline = NULL;
    data->acc = gsl_interp_accel_alloc();
    if (!data->acc) goto error;
    data->spline = gsl_spline_alloc(type, n);
    if (!data->spline) goto error;
    err = gsl_spline_init(data->spline, x, y, n);
    if (err) goto error;

    fclose(f);
    free(x);
    free(y);
    return data;

    /* Exit path for erros generated while f is open. */
error:
    fclose(f);
    if (x) free(x);
    if (y) free(y);
    cleanup(data);
    return NULL;
}

/* Interpolation function. */
static double interpolate(double x, void *data)
{
    spline_data *d = data;

    return gsl_spline_eval(d->spline, x, d->acc);
}

/* Free memory. */
static void cleanup(void *data)
{
    spline_data *d = data;

    if (!d) return;
    if (d->spline) gsl_spline_free(d->spline);
    if (d->acc) gsl_interp_accel_free(d->acc);
    free(d);
}


/***********************************************************************
 * Exported functions.
 */

void *linear_init(char *init_string)
{ return init(gsl_interp_linear, init_string); }

void *spline_init(char *init_string)
{ return init(gsl_interp_cspline, init_string); }

void *akima_init(char *init_string)
{ return init(gsl_interp_akima, init_string); }

double linear(double x, void *data) { return interpolate(x, data); }
double spline(double x, void *data) { return interpolate(x, data); }
double akima (double x, void *data) { return interpolate(x, data); }
void linear_cleanup(void *data) { cleanup(data); }
void spline_cleanup(void *data) { cleanup(data); }
void akima_cleanup (void *data) { cleanup(data); }
