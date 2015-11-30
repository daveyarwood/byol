#include <math.h>
#include <limits.h>
#include "mpc.h"

////////////////////////////////////////////////////////////////////////////////

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

#define LASSERT(lval, cond, err) \
  if (!(cond)) { lval_del(lval); return lval_err(err); }

#define LASSERT_ONE_ARG(lval, err) \
  LASSERT(a, a->count == 1, err);

#define LASSERT_N_ARGS(lval, n, err) \
  LASSERT(a, a->count == n, err);

#define LASSERT_QEXPR(lval, err) \
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR, err);

#define LASSERT_NOT_EMPTY(lval, err) \
  LASSERT(a, a->cell[0]->count != 0, err);

////////////////////////////////////////////////////////////////////////////////

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum { LVAL_ERR, LVAL_LONG, LVAL_DBL, LVAL_SYM,
       LVAL_FN,  LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type;

  long lng;
  double dbl;
  char* err;
  char* sym;
  lbuiltin fn;

  int count;
  lval** cell;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;
};

////////////////////////////////////////////////////////////////////////////////

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

lval* lval_qexpr(void) {
  lval* v  = malloc(sizeof(lval));
  v->type  = LVAL_QEXPR;
  v->count = 0;
  v->cell  = NULL;
  return v;
}

lval* lval_fn(lbuiltin fn) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FN;
  v->fn   = fn;
  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    // do nothing special for numbers and fn pointers
    case LVAL_LONG:
    case LVAL_DBL:
    case LVAL_FN:
      break;
    // for errors and symbols, free the string data
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
    // for S/Q-expressions, delete all the elements inside
    case LVAL_SEXPR:
    case LVAL_QEXPR:
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

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    /* Copy functions and numbers directly */
    case LVAL_FN: x->fn = v->fn; break;
    case LVAL_LONG: x->lng = v->lng; break;
    case LVAL_DBL: x->dbl = v->dbl; break;

    /* Copy strings using malloc and strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    /* Copy lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell  = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }
  return x;
}

////////////////////////////////////////////////////////////////////////////////

lval* lval_conj(lval* sexp, lval* x) {
  sexp->count++;
  sexp->cell = realloc(sexp->cell, sizeof(lval*) * sexp->count);
  sexp->cell[sexp->count - 1] = x;
  return sexp;
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

lval* lval_join(lval* x, lval* y) {
  while (y->count) {
    x = lval_conj(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval* lval_cons(lval* x, lval* sexp) {
  /*
   * This is probably inefficient, but whatevs.
   * Also, it doesn't work if x is a qexpr for some reason.
   *
   * ¯\_(ツ)_/¯
   */
  lval* new_sexp = lval_qexpr();
  new_sexp = lval_conj(new_sexp, x);
  new_sexp = lval_join(new_sexp, sexp);
  lval_del(x);
  return new_sexp;
}

////////////////////////////////////////////////////////////////////////////////

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  /* Iterate over all items stored in the environment */
  for (int i = 0; i < e->count; i++) {
    /*
     * If the symbol is defined in the environment, return a copy of its value.
     */
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  /* If the symbol is not defined, return an error. */
  return lval_err("unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v) {
  /*
   * Iterate over all items stored in the environment to see if the key already
   * exists.
   */
  for (int i = 0; i < e->count; i++) {
    /*
     * If it does, delete the item at that position and replace it with a copy
     * of the new value.
     */
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If the symbol isn't already defined, allocate space for a new entry... */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /*
   * ...and copy the contents of the new symbol and value into the new
   * location.
   */
  e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count-1], k->sym);
  e->vals[e->count-1] = lval_copy(v);
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
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_FN:    printf("<function>"); break;
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

lval* builtin_head(lenv* e, lval* a) {
  LASSERT_ONE_ARG(a, "There must be only one argument to 'head'.");
  LASSERT_QEXPR(a, "The argument to 'head' must be a Q-expression.");
  LASSERT_NOT_EMPTY(a, "Can't take the 'head' of an empty Q-expression.");

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_ONE_ARG(a, "There must be only one argument to 'tail'.");
  LASSERT_QEXPR(a, "The argument to 'tail' must be a Q-expression.");
  LASSERT_NOT_EMPTY(a, "Can't take the 'tail' of an empty Q-expression.");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_init(lenv* e, lval* a) {
  LASSERT_ONE_ARG(a, "There must be only one argument to 'init'.");
  LASSERT_QEXPR(a, "The argument to 'init' must be a Q-expression.");
  LASSERT_NOT_EMPTY(a, "Can't take the 'init' of an empty Q-expression.");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, v->count - 1));
  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* lval_eval(lenv* e, lval* v);

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT_ONE_ARG(a, "There must be only one argument to 'eval'.");
  LASSERT_QEXPR(a, "The argument to 'eval' must be a Q-expression.");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
      "The arguments to 'join' must be Q-expressions.");
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_cons(lenv* e, lval* a) {
  LASSERT_N_ARGS(a, 2,
    "There must be two arguments to 'cons'.");

  LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
    "The second argument to 'cons' must be a Q-expression.");

  lval* x = lval_pop(a, 0);
  x = lval_cons(x, lval_pop(a, 0));

  lval_del(a);
  return x;
}

lval* builtin_len(lenv* e, lval* a) {
  LASSERT_ONE_ARG(a, "There must be only one arg to 'len'.");
  LASSERT_QEXPR(a, "The argument to 'len' must be a Q-expression.");

  lval* qexp = lval_pop(a, 0);

  int l = qexp->count;
  lval_del(a);
  return lval_long(l);
}

////////////////////////////////////////////////////////////////////////////////

lval* builtin_def(lenv* e, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "The first argument to 'def' must be a Q-expression.");

  /* The first argument to 'def' is a list of symbols */
  lval* syms = a->cell[0];

  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
      "The first argument to 'def' must be a list of symbols.");
  }

  LASSERT(a, syms->count == a->count - 1,
    "The number of symbols defined by 'def' must be equal to the number of "
    "values.");

  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  // return an empty sexp ()
  return lval_sexpr();
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

lval* builtin_op(lenv* e, lval* a, char* op) {
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

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "add");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "sub");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "mul");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "div");
}

lval* builtin_mod(lenv* e, lval* a) {
  return builtin_op(e, a, "mod");
}

lval* builtin_pow(lenv* e, lval* a) {
  return builtin_op(e, a, "pow");
}

lval* builtin_min(lenv* e, lval* a) {
  return builtin_op(e, a, "min");
}

lval* builtin_max(lenv* e, lval* a) {
  return builtin_op(e, a, "max");
}

////////////////////////////////////////////////////////////////////////////////

void lenv_add_builtin(lenv* e, char* name, lbuiltin fn) {
  lval* k = lval_sym(name);
  lval* v = lval_fn(fn);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* List functions */
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "init", builtin_init);
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "len",  builtin_len);

  /* Mathematical functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
  lenv_add_builtin(e, "%", builtin_mod);
  lenv_add_builtin(e, "^", builtin_pow);
  lenv_add_builtin(e, "add", builtin_add);
  lenv_add_builtin(e, "sub", builtin_sub);
  lenv_add_builtin(e, "mul", builtin_mul);
  lenv_add_builtin(e, "div", builtin_div);
  lenv_add_builtin(e, "mod", builtin_mod);
  lenv_add_builtin(e, "pow", builtin_pow);
  lenv_add_builtin(e, "min", builtin_min);
  lenv_add_builtin(e, "max", builtin_max);

  /* Variable functions */
  lenv_add_builtin(e, "def", builtin_def);
}

////////////////////////////////////////////////////////////////////////////////

lval* lval_eval_sexpr(lenv* e, lval* v) {
  /* Evaluate children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
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

  /* Ensure first element is a function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FN) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression does not start with a function.");
  }

  /*
   * Call the function on the arguments (remaining children) to get the
   * result.
   */
  lval* result = f->fn(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }

  if (v->type == LVAL_SEXPR) {
    /* Evaluate S-expressions */
    return lval_eval_sexpr(e, v);
  }

  /* All other lval types are returned as-is */
  return v;
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

  // if root (>) or s/qexpr, then create an empty list...
  lval* sexp = NULL;
  if (strcmp(t->tag, ">") == 0) { sexp = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { sexp = lval_sexpr(); }
  if (strstr(t->tag, "qexpr"))  { sexp = lval_qexpr(); }

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
  mpc_parser_t* Qexpr     = mpc_new("qexpr");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Lispy     = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                        \
      long     : /-?[0-9]+/ ;                                                \
      double   : /-?[0-9]+\\.[0-9]+/ ;                                       \
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                          \
      sexpr    : '(' <expr>* ')' ;                                           \
      qexpr    : '{' <expr>* '}' ;                                           \
      expr     : <double> | <long> | <symbol> | <sexpr> | <qexpr>;           \
      lispy    : /^/ <expr>* /$/ ;                                           \
    ",
    Long, Double, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    /* Attempt to parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success, evaluate and print the result */
      lval* result = lval_eval(e, lval_read(r.output));
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

  lenv_del(e);
  mpc_cleanup(7, Long, Double, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}
