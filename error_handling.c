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

////////////////////////////////////////////////////////////////////////////////

typedef struct {
  int type;
  long num;
  double dbl;
  int err;
} lval;

enum { LVAL_NUM, LVAL_DBL, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_MOD_DBL, LERR_BAD_OP, LERR_BAD_NUM };

lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num  = x;
  return v;
}

lval lval_dbl(double x) {
  lval v;
  v.type = LVAL_DBL;
  v.dbl  = x;
  return v;
}

lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err  = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    case LVAL_DBL:
      printf("%f", v.dbl);
      break;
    case LVAL_ERR:
      switch (v.err) {
        case LERR_DIV_ZERO:
          printf("Error: division by zero");
          break;
        case LERR_MOD_DBL:
          printf("Error: can't use modulo operator on doubles");
          break;
        case LERR_BAD_OP:
          printf("Error: invalid operator");
          break;
        case LERR_BAD_NUM:
          printf("Error: invalid number");
          break;
      }
    break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

////////////////////////////////////////////////////////////////////////////////

lval lval_add(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(x.num + y.num);
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return lval_dbl(x.num + y.dbl);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return lval_dbl(x.dbl + y.num);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return lval_dbl(x.dbl + y.dbl);
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_subtract(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(x.num - y.num);
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return lval_dbl(x.num - y.dbl);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return lval_dbl(x.dbl - y.num);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return lval_dbl(x.dbl - y.dbl);
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_multiply(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(x.num * y.num);
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return lval_dbl(x.num * y.dbl);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return lval_dbl(x.dbl * y.num);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return lval_dbl(x.dbl * y.dbl);
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_divide(lval x, lval y) {
  if ((y.type == LVAL_NUM && y.num == 0) ||
      (y.type == LVAL_DBL && y.dbl == 0.0)) {
    return lval_err(LERR_DIV_ZERO);
  }

  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(x.num / y.num);
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return lval_dbl(x.num / y.dbl);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return lval_dbl(x.dbl / y.num);
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return lval_dbl(x.dbl / y.dbl);
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_mod(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(x.num % y.num);
  } else {
    return lval_err(LERR_MOD_DBL);
  }
}

lval lval_pow(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return lval_num(pow(x.num, y.num));
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return lval_dbl(pow(x.num, y.dbl));
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return lval_dbl(pow(x.dbl, y.num));
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return lval_dbl(pow(x.dbl, y.dbl));
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_min(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return x.num < y.num ? x : y;
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return x.num < y.dbl ? x : y;
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return x.dbl < y.num ? x : y;
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return x.dbl < y.dbl ? x : y;
  }

  // we should never get this far
  lval empty;
  return empty;
}

lval lval_max(lval x, lval y) {
  if (x.type == LVAL_NUM && y.type == LVAL_NUM) {
    return x.num > y.num ? x : y;
  }

  if (x.type == LVAL_NUM && y.type == LVAL_DBL) {
    return x.num > y.dbl ? x : y;
  }

  if (x.type == LVAL_DBL && y.type == LVAL_NUM) {
    return x.dbl > y.num ? x : y;
  }

  if (x.type == LVAL_DBL && y.type == LVAL_DBL) {
    return x.dbl > y.dbl ? x : y;
  }

  // we should never get this far
  lval empty;
  return empty;
}

////////////////////////////////////////////////////////////////////////////////

/* Use operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {
  /* If either value is an error, return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  /* Otherwise, do some math on the number values. */
  if (strcmp(op, "+") == 0) { return lval_add(x, y); }
  if (strcmp(op, "-") == 0) { return lval_subtract(x, y); }
  if (strcmp(op, "*") == 0) { return lval_multiply(x, y); }
  if (strcmp(op, "/") == 0) { return lval_divide(x, y); }
  if (strcmp(op, "%") == 0) { return lval_mod(x, y); }
  if (strcmp(op, "^") == 0) { return lval_pow(x, y); }
  if (strcmp(op, "add") == 0) { return lval_add(x, y); }
  if (strcmp(op, "sub") == 0) { return lval_subtract(x, y); }
  if (strcmp(op, "mul") == 0) { return lval_multiply(x, y); }
  if (strcmp(op, "div") == 0) { return lval_divide(x, y); }
  if (strcmp(op, "mod") == 0) { return lval_mod(x, y); }
  if (strcmp(op, "pow") == 0) { return lval_pow(x, y); }
  if (strcmp(op, "min") == 0) { return lval_min(x, y); }
  if (strcmp(op, "max") == 0) { return lval_max(x, y); }

  /* If we've gotten this far, the operator is invalid */
  return lval_err(LERR_BAD_OP);
}

/* Evaluate a parse tree */
lval eval(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) {
    /* Check to see if there is some error parsing the number as a long */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    if (errno == ERANGE) {
      return lval_err(LERR_BAD_NUM);
    } else {
      return lval_num(x);
    }
  }

  if (strstr(t->tag, "double")) {
    /* Check to see if there is some error parsing the number as a double */
    errno = 0;
    double x = strtod(t->contents, NULL);
    if (errno == ERANGE) {
      return lval_err(LERR_BAD_NUM);
    } else {
      return lval_dbl(x);
    }
  }

  /* The operator is always the second child. */
  char* op = t->children[1]->contents;

  /* Determine the initial value based on the operator. */
  lval x;
  x.type = LVAL_NUM;
  int i = 2;
  if (strcmp(op, "+") == 0) { x.num = 0; }
  else if (strcmp(op, "-") == 0) { x.num = 0; }
  else if (strcmp(op, "*") == 0) { x.num = 1; }
  else if (strcmp(op, "add") == 0) { x.num = 0; }
  else if (strcmp(op, "sub") == 0) { x.num = 0; }
  else if (strcmp(op, "mul") == 0) { x.num = 1; }
  else if (strcmp(op, "min") == 0) { x.num = LONG_MAX; }
  else if (strcmp(op, "max") == 0) { x.num = LONG_MIN; }
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
  mpc_parser_t* Double    = mpc_new("double");
  mpc_parser_t* Operator  = mpc_new("operator");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Lispy     = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                        \
      number   : /-?[0-9]+/ ;                                                \
      double   : /-?[0-9]+\\.[0-9]+/ ;                                       \
      operator : '+' | '-' | '*' | '/' | '%' | '^'                           \
               | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"pow\"   \
               | \"min\" | \"max\" ;                                         \
      expr     : <double> | <number> | '(' <operator> <expr>+ ')' ;          \
      lispy    : /^/ <operator> <expr>+ /$/ ;                                \
    ",
    Number, Double, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success, evaluate and print the result */
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* On error, print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(5, Number, Double, Operator, Expr, Lispy);

  return 0;
}
