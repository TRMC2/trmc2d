// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * expression.c: trmc2d plugin for evaluating expressions, based on
 * libmatheval.
 *
 * Syntax:
 *   channel<index>:conversion expression literal <expression>
 *   channel<index>:conversion expression file <filename>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>  /* for NAN */
#include <matheval.h>


/***********************************************************************
 * Evaluate a single expression given in the conversion command.
 */

double literal(double raw, void *data)
{
    return evaluator_evaluate_x(data, raw);
}

void *literal_init(char *init_string)
{
    return evaluator_create(init_string);
}

void literal_cleanup(void *data)
{
    evaluator_destroy(data);
}


/***********************************************************************
 * Evaluate a sequence of expressions read from a file.
 */

/*
 * This struct holds the "compiled program". Given the file contents:
 *   a = expr_A
 *   b = expr_B
 *   expr_T
 * the struct will hold:
 *   .count = 3,
 *   .vars  = {"x",    "a",    "b"},
 *   .exprs = {expr_A, expr_B, expr_T}
 * where "x" is a string constant, "a" and "b" live in the heap, and
 * expr_X are the expression evaluators.
 */
typedef struct {
    int count;     /* number of variables and expressions */
    char **vars;   /* variable names, starting with constant "x" */
    void **exprs;  /* compiled expressions */
} program_t;

/* Free memory. */
void file_cleanup(void *data)
{
    program_t *prog = data;

    if (!prog) return;
    for (int i = 1; i < prog->count; i++)  /* do not free("x") */
        free(prog->vars[i]);
    for (int i = 0; i < prog->count; i++)
        evaluator_destroy(prog->exprs[i]);
    if (prog->vars) free(prog->vars);
    if (prog->exprs) free(prog->exprs);
    free(prog);
}

/* Evaluate the program. */
double file(double raw, void *data)
{
    program_t *prog = data;
    double *values = malloc(prog->count * sizeof *values);
    double current_value = raw;
    for (int i = 0; i < prog->count; i++) {
        values[i] = current_value;
        current_value = evaluator_evaluate(prog->exprs[i],
                i + 1, prog->vars, values);
        if (isnan(current_value)) break;
    }
    free(values);
    return current_value;
}

/* Compile. */
void *file_init(char *init_string)
{
    FILE *f;
    char s[1024];
    program_t *prog;
    int allocated = 0;  /* length allocated to prog->vars and prog->exprs */
    char * const var_x = "x";
    char *varname = var_x;  /* Last variable defined so far. */

    /* Open the file for reading. */
    if (!init_string) {
        fprintf(stderr, "Missing file name.");
        return NULL;
    }
    f = fopen(init_string, "r");
    if (!f) { perror(init_string); return NULL; }

    /* Initialize the program structure. */
    prog = calloc(1, sizeof *prog);
    if (!prog) {
        perror("calloc");
        goto error;
    }

    /* Read line by line. */
    while (fgets(s, sizeof s, f)) {

        /* Skip comments. */
        if (*s == '\n' || *s == '#') continue;

        /* Expand the arrays if necessary. */
        if (allocated <= prog->count) {
            allocated += 16;
            prog->vars = realloc(prog->vars,
                    allocated * sizeof *prog->vars);
            prog->exprs = realloc(prog->exprs,
                    allocated * sizeof *prog->exprs);
            if (!prog->vars || !prog->exprs) {
                perror("realloc");
                goto error;
            }
        }

        /* Store the previous variable. */
        if (!varname) {
            fprintf(stderr, "More than one non-assignment expression.\n");
            goto error;
        }
        prog->vars[prog->count] = varname;

        /* Extract the variable name. */
        char *expr;
        char *equal = strchr(s, '=');
        if (equal) {  /* this is an assignment */
            *equal = '\0';
            for (char *p = equal - 1; p > s && isspace(*p); p--)
                *p = '\0';
            varname = malloc(strlen(s) + 1);
            if (!varname) {
                perror("malloc");
                free(prog->vars[prog->count]);
                goto error;
            }
            strcpy(varname, s);
            expr = equal + 1;
        } else {
            varname = NULL;
            expr = s;
        }

        /* Parse the expression. */
        for (char *p = expr+strlen(expr)-1; p > expr && isspace(*p); p--)
            *p = '\0';  /* need to remove trailing '\r' or '\n' */
        void *compiled = evaluator_create(expr);
        if (!compiled) {
            fprintf(stderr, "Could not parse expression %d: %s\n",
                    prog->count, expr);
            free(prog->vars[prog->count]);
            goto error;
        }
        prog->exprs[prog->count] = compiled;
        prog->count++;
    }

    /* Last statement should not be an assignment. */
    if (varname) {
        fprintf(stderr, "Last statement should not be an assignment.\n");
        goto error;
    }

    /* Shrink. */
    if (allocated > prog->count) {
        prog->vars = realloc(prog->vars, prog->count * sizeof *prog->vars);
        prog->exprs = realloc(prog->exprs, prog->count * sizeof *prog->exprs);
    }

    fclose(f);
    return prog;

    /* Exit path for erros generated while f is open. */
error:
    fclose(f);
    if (varname && varname != var_x) free(varname);
    file_cleanup(prog);
    return NULL;
}
