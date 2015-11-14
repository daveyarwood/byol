#include <math.h>
#include <limits.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

#else
#include <editline/readline.h>

#endif

/* Use operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  if (strcmp(op, "%") == 0) { return x % y; }
  if (strcmp(op, "^") == 0) { return pow(x, y); }
  if (strcmp(op, "add") == 0) { return x + y; }
  if (strcmp(op, "sub") == 0) { return x - y; }
  if (strcmp(op, "mul") == 0) { return x * y; }
  if (strcmp(op, "div") == 0) { return x / y; }
  if (strcmp(op, "mod") == 0) { return x % y; }
  if (strcmp(op, "pow") == 0) { return pow(x, y); }
  if (strcmp(op, "min") == 0) {
    if (x < y) { return x; }
    else { return y; }
  }
  if (strcmp(op, "max") == 0) {
    if (x > y) { return x; }
    else { return y; }
  }
  return 0;
}

/* Evaluate a parse tree */
long eval(mpc_ast_t* t) {
  /* If tagged as a number, return it directly */
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  /* The operator is always the second child. */
  char* op = t->children[1]->contents;

  /* Determine the initial value based on the operator. */
  long x;
  int i = 2;
  if (strcmp(op, "+") == 0) { x = 0; }
  else if (strcmp(op, "-") == 0) { x = 0; }
  else if (strcmp(op, "*") == 0) { x = 1; }
  else if (strcmp(op, "add") == 0) { x = 0; }
  else if (strcmp(op, "sub") == 0) { x = 0; }
  else if (strcmp(op, "mul") == 0) { x = 1; }
  else if (strcmp(op, "min") == 0) { x = LONG_MAX; }
  else if (strcmp(op, "max") == 0) { x = LONG_MIN; }
  /* For these operators, the initial value is the first argument. */
  else if (strcmp(op, "/") == 0) { x = eval(t->children[2]); i = 3; }
  else if (strcmp(op, "%") == 0) { x = eval(t->children[2]); i = 3; }
  else if (strcmp(op, "div") == 0) { x = eval(t->children[2]); i = 3; }
  else if (strcmp(op, "mod") == 0) { x = eval(t->children[2]); i = 3; }
  else if (strcmp(op, "pow") == 0) { x = eval(t->children[2]); i = 3; }

  /* Reduce over the arguments. */
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}

int main(int argc, char** argv) {
  mpc_parser_t* Number    = mpc_new("number");
  mpc_parser_t* Operator  = mpc_new("operator");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Lispy     = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                        \
      number   : /-?[0-9]+(\\.[0-9]+)?/ ;                                    \
      operator : '+' | '-' | '*' | '/' | '%' | '^'                           \
               | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"pow\"   \
               | \"min\" | \"max\" ;                                         \
      expr     : <number> | '(' <operator> <expr>+ ')' ;                     \
      lispy    : /^/ <operator> <expr>+ /$/ ;                                \
    ",
    Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success, evaluate and print the result */
      long result = eval(r.output);
      printf("%li\n", result);
      mpc_ast_delete(r.output);
    } else {
      /* On error, print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}
