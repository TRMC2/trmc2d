/*
 * expression.c: tempd plugin for evaluating expressions, based on
 * libmatheval.
 *
 * Syntax:
 *   channel<index>:conversion expression literal <expression>
 */

#include <matheval.h>

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
