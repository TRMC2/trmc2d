// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Glue code for the conversion functions.
 */

typedef int (*Etalon)(double*);

Etalon convert_init(int argc, char **argv);

void convert_cleanup(Etalon f);
