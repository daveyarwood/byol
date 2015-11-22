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

typedef struct lval {
  int type;
  long lng;
  double dbl;
  char* err;
  char* sym;
  int count;
  struct lval** cell;
} lval;

enum { LVAL_LONG, LVAL_DBL, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

lval* lval_long(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_LONG;
  v->lng  = x;
  return v;
}

lval* lval_dbl(double x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_DBL;
  v->dbl  = x;
  return v;
}

lval* lval_err(char* msg) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err  = malloc(strlen(msg) + 1);
  strcpy (v->err, msg);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym  = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v  = malloc(sizeof(lval));
  v->type  = LVAL_SEXPR;
  v->count = 0;
  v->cell  = NULL;
  return v;
}

lval* lval_conj(lval* sexp, lval* x) {
  sexp->count++;
  sexp->cell = realloc(sexp->cell, sizeof(lval*) * sexp->count);
  sexp->cell[sexp->count - 1] = x;
  return sexp;
}

void lval_del(lval* v) {
  switch (v->type) {
    // do nothing special for numbers
    case LVAL_LONG: case LVAL_DBL: break;
    // for errors and symbols, free the string data
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    // for sexprs, delete all the elements inside
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      // also free the memory allocated to contain the pointers
      free(v->cell);
      break;

    // free the memory allocated for the lval struct itself
    free(v);
  }
}

/*
 * Returns the element at index `i` of an S-expression. Shortens the list of
 * elements in the S-expression by deleting the element that was popped.
 */
lval* lval_pop(lval* sexp, int i) {
  lval* x = sexp->cell[i];

  /* Shift memory after the item at `i` over the top. */
  memmove(&sexp->cell[i], &sexp->cell[i+1],
          sizeof(lval*) * (sexp->count-i-1));

  sexp->count--;

  /* Reallocate the memory used. */
  sexp->cell = realloc(sexp->cell, sizeof(lval*) * sexp->count);

  return x;
}

/* Like lval_pop, but also deletes the S-expression. */
lval* lval_take(lval* sexp, int i) {
  lval* x = lval_pop(sexp, i);
  lval_del(sexp);
  return x;
}

////////////////////////////////////////////////////////////////////////////////

void lval_expr_print(lval* v, char open, char close);

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_LONG:  printf("%li", v->lng); break;
    case LVAL_DBL:   printf("%f", v->dbl); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);

  for (int i = 0; i < v->count; i++) {
    // print the value
    lval_print(v->cell[i]);

    // unless this is the last element, print a space
    if (i != v->count - 1) {
      putchar(' ');
    }
  }

  putchar(close);
}

////////////////////////////////////////////////////////////////////////////////

/*
 * These operators are defined in the context of a reduce operation, where `x`
 * is the accumulator, and `y` is the next argument. Rather than returning a
 * new lval* value (which would require allocating memory for it), these
 * functions mutate `x` in place.
 */

void lval_add(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    (*x)->lng += y->lng;
  }
  else if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    (*x)->dbl = (*x)->lng + y->dbl;
    (*x)->type = LVAL_DBL;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    (*x)->dbl += y->lng;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    (*x)->dbl += y->dbl;
  }
}

void lval_subtract(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    (*x)->lng -= y->lng;
  }
  else if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    (*x)->dbl = (*x)->lng - y->dbl;
    (*x)->type = LVAL_DBL;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    (*x)->dbl -= y->lng;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    (*x)->dbl -= y->dbl;
  }
}

void lval_multiply(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    (*x)->lng *= y->lng;
  }
  else if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    (*x)->dbl = (*x)->lng * y->dbl;
    (*x)->type = LVAL_DBL;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    (*x)->dbl *= y->lng;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    (*x)->dbl *= y->dbl;
  }
}

void lval_divide(lval** x, lval* y) {
  if ((y->type == LVAL_LONG && y->lng == 0) ||
      (y->type == LVAL_DBL && y->dbl == 0.0)) {
    lval_del(*x);
    *x = lval_err("division by zero");
  } else {
    if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
      (*x)->lng /= y->lng;
    }
    else if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
      (*x)->dbl = (*x)->lng / y->dbl;
      (*x)->type = LVAL_DBL;
    }
    else if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
      (*x)->dbl /= y->lng;
    }
    else if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
      (*x)->dbl /= y->dbl;
    }
  }
}

void lval_mod(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    (*x)->lng = (*x)->lng % y->lng;
  } else {
    lval_del(*x);
    *x = lval_err("modulo arguments must be whole numbers");
  }
}

void lval_pow(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    (*x)->lng = pow((*x)->lng, y->lng);
  }
  else if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    (*x)->dbl = pow((*x)->lng, y->dbl);
    (*x)->type = LVAL_DBL;
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    (*x)->dbl = pow((*x)->dbl, y->lng);
  }
  else if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    (*x)->dbl = pow((*x)->dbl, y->dbl);
  }
}

void lval_min(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    if ((*x)->lng > y->lng) { (*x) = y; }
  }

  if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    if ((*x)->lng > y->dbl) { (*x) = y; }
  }

  if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    if ((*x)->dbl > y->lng) { (*x) = y; }
  }

  if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    if ((*x)->dbl > y->dbl) { (*x) = y; }
  }
}

void lval_max(lval** x, lval* y) {
  if ((*x)->type == LVAL_LONG && y->type == LVAL_LONG) {
    if ((*x)->lng < y->lng) { (*x) = y; }
  }

  if ((*x)->type == LVAL_LONG && y->type == LVAL_DBL) {
    if ((*x)->lng < y->dbl) { (*x) = y; }
  }

  if ((*x)->type == LVAL_DBL && y->type == LVAL_LONG) {
    if ((*x)->dbl < y->lng) { (*x) = y; }
  }

  if ((*x)->type == LVAL_DBL && y->type == LVAL_DBL) {
    if ((*x)->dbl < y->dbl) { (*x) = y; }
  }
}

////////////////////////////////////////////////////////////////////////////////

lval* builtin_op(lval* a, char* op) {
  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_LONG &&
        a->cell[i]->type != LVAL_DBL) {
      lval_del(a);
      return lval_err("Only number arguments are supported.");
    }
  }

  lval* x = lval_pop(a, 0);

  /* Special behavior for operators that support 1-argument arity */
  if (a->count == 0) {
    /* `-` does unary negation, e.g. (- 3) => -3 */
    if (strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) {
      if (x->type == LVAL_LONG) {
        x->lng = -x->lng;
      }
      if (x->type == LVAL_DBL) {
        x->dbl = -x->dbl;
      }
    }
  }

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { lval_add(&x, y); }
    if (strcmp(op, "-") == 0) { lval_subtract(&x, y); }
    if (strcmp(op, "*") == 0) { lval_multiply(&x, y); }
    if (strcmp(op, "/") == 0) { lval_divide(&x, y); }
    if (strcmp(op, "%") == 0) { lval_mod(&x, y); }
    if (strcmp(op, "^") == 0) { lval_pow(&x, y); }
    if (strcmp(op, "add") == 0) { lval_add(&x, y); }
    if (strcmp(op, "sub") == 0) { lval_subtract(&x, y); }
    if (strcmp(op, "mul") == 0) { lval_multiply(&x, y); }
    if (strcmp(op, "div") == 0) { lval_divide(&x, y); }
    if (strcmp(op, "mod") == 0) { lval_mod(&x, y); }
    if (strcmp(op, "pow") == 0) { lval_pow(&x, y); }
    if (strcmp(op, "min") == 0) { lval_min(&x, y); }
    if (strcmp(op, "max") == 0) { lval_max(&x, y); }

    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  /* Error checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  /* Empty expression */
  if (v->count == 0) {
    return v;
  }

  /* Single expression */
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  lval* f = lval_pop(v, 0);

  /* Ensure first element is a symbol */
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with a symbol.");
  }

  /* Call first element as a built-in operator */
  lval* result = builtin_op(v, f->sym);
  lval_del(f);
  return result;
}

lval* lval_eval(lval* v) {
  if (v->type == LVAL_SEXPR) {
    /* Evaluate S-expressions */
    return lval_eval_sexpr(v);
  } else {
    /* All other lval types are returned as-is */
    return v;
  }
}

////////////////////////////////////////////////////////////////////////////////

lval* lval_read_long(mpc_ast_t* t) {
  /* Check to see if there is some error parsing as a long */
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  if (errno == ERANGE) {
    return lval_err("invalid long");
  } else {
    return lval_long(x);
  }
}

lval* lval_read_double(mpc_ast_t* t) {
  /* Check to see if there is some error parsing as a double */
  errno = 0;
  double x = strtod(t->contents, NULL);
  if (errno == ERANGE) {
    return lval_err("invalid double");
  } else {
    return lval_dbl(x);
  }
}

lval* lval_read(mpc_ast_t* t) {
  // for numbers and symbols, return conversion to that type
  if (strstr(t->tag, "long"))   { return lval_read_long(t);     }
  if (strstr(t->tag, "double")) { return lval_read_double(t);   }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  // if root (>) or sexpr, then create an empty list...
  lval* sexp = NULL;
  if (strcmp(t->tag, ">") == 0) { sexp = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { sexp = lval_sexpr(); }

  // ...and fill it with any valid expression contained within
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0)  { continue; }
    sexp = lval_conj(sexp, lval_read(t->children[i]));
  }

  return sexp;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  mpc_parser_t* Long      = mpc_new("long");
  mpc_parser_t* Double    = mpc_new("double");
  mpc_parser_t* Symbol    = mpc_new("symbol");
  mpc_parser_t* Sexpr     = mpc_new("sexpr");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Lispy     = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                        \
      long     : /-?[0-9]+/ ;                                                \
      double   : /-?[0-9]+\\.[0-9]+/ ;                                       \
      symbol   : '+' | '-' | '*' | '/' | '%' | '^'                           \
               | \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"pow\"   \
               | \"min\" | \"max\" ;                                         \
      sexpr    : '(' <expr>* ')' ;                                           \
      expr     : <double> | <long> | <symbol> | <sexpr> ;                    \
      lispy    : /^/ <expr>* /$/ ;                                           \
    ",
    Long, Double, Symbol, Sexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success, evaluate and print the result */
      lval* result = lval_eval(lval_read(r.output));
      lval_println(result);
      lval_del(result);
      mpc_ast_delete(r.output);
    } else {
      /* On error, print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(6, Long, Double, Symbol, Sexpr, Expr, Lispy);

  return 0;
}
