// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * interpolate-linear.c: trmc2d plugin for linear interpolation.
 */

#include <stdio.h>
#include <stdlib.h>

#define NAN (0.0/0.0)   /* Not a Number. */

typedef struct {
    int n;          /* length of the tables */
    double *x;
    double *y;
    int last;       /* last interval used = [last, last+1] */
} conversion_table;

/* Forward declaration. */
void linear_cleanup(void *data);

/*
 * init_string is the name of the file containing the conversion table.
 * Allocates and returns a conversion_table*.
 */
void *linear_init(char *init_string)
{
    FILE *f;
    char s[1024];
    int n;
    conversion_table *t;
    int allocated = 0;      /* length alocated to t->x and t->y */

    /* Open the file for reading. */
    if (!init_string) return NULL;
    f = fopen(init_string, "r");
    if (!f) return NULL;

    /* Init the table. */
    t = calloc(1, sizeof *t);
    if (!t) goto error;

    /* Read by lines. */
    while (fgets(s, sizeof s, f)) {

        /* Skip comments. */
        if (*s == '\n' || *s == '#') continue;

        /* Expand the arrays if necessary. */
        if (allocated <= t->n) {
            allocated += 1024;
            t->x = realloc(t->x, allocated * sizeof *t->x);
            t->y = realloc(t->y, allocated * sizeof *t->y);
            if (!t->x || !t->y) goto error;
        }

        /* Parse. */
        n = sscanf(s, "%lf %lf", &t->x[t->n], &t->y[t->n]);
        if (n != 2) goto error;
        t->n++;
    }

    /* Shrink. */
    if (allocated > t->n) {
        t->x = realloc(t->x, t->n * sizeof *t->x);
        t->y = realloc(t->y, t->n * sizeof *t->y);
    }

    fclose(f);
    return t;

    /* Exit path for erros generated while f is open. */
error:
    fclose(f);
    linear_cleanup(t);
    return NULL;
}

/* Interpolation function. */
double linear(double x, void *data)
{
    conversion_table *t = data;

    /* Return Not a Number if out of table. */
    if (x<t->x[0] || x>t->x[t->n-1]) return NAN;

    /* Optimization: look first close to the last used interval. */
    int i, j;
    if (x > t->x[t->last]) i = t->last;
    else if (t->last>0 && x>t->x[t->last-1]) i = t->last - 1;
    else i = 0;
    if (x < t->x[t->last+1]) j = t->last + 1;
    else if (t->last<t->n-2 && x<t->x[t->last+2]) j = t->last + 2;
    else j = t->n-1;

    /* Search the right interval by bisection. */
    while (j-i > 1) {
        int k = (i+j) / 2;
        if (x > t->x[k]) i = k;
        else j = k;
    }

    /* Save for next time. */
    t->last = i;

    /* Interpolate. */
    double slope = (t->y[i+1] - t->y[i]) / (t->x[i+1] - t->x[i]);
    return t->y[i] + (x - t->x[i]) * slope;
}

/* Free memory. */
void linear_cleanup(void *data)
{
    conversion_table *t = data;

    if (!t) return;
    if (t->x) free(t->x);
    if (t->y) free(t->y);
    free(t);
}
