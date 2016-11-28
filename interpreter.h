/*
 * Interpreter for the TRMC2 language. Include "parse.h" before this.
 */

/* Push a static string on the error stack. */
void push_error(const char *err);

/* Usage: parse(command, trmc2_syntax, NULL); */
extern const syntax_tree trmc2_syntax[];

/* This is set to 1 by the "quit" command. */
extern int should_quit;
