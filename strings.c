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

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum { LVAL_ERR, LVAL_LONG, LVAL_DBL, LVAL_BOOL,
       LVAL_SYM, LVAL_FN,  LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type;

  /* Basic */
  long lng;
  double dbl;
  int bl;
  char* err;
  char* sym;

  /* Function */
  lbuiltin builtin; // when not NULL, this is a builtin fn
  lenv* env;
  lval* args;
  lval* body;

  /* Expression */
  int count;
  lval** cell;
};

char* ltype_name(int t) {
  switch(t) {
    case LVAL_ERR:   return "Error";
    case LVAL_LONG:  return "Long";
    case LVAL_DBL:   return "Double";
    case LVAL_BOOL:  return "Boolean";
    case LVAL_SYM:   return "Symbol";
    case LVAL_FN:    return "Function";
    case LVAL_SEXPR: return "S-expression";
    case LVAL_QEXPR: return "Q-expression";
    default:         return "Unknown";
  }
}

struct lenv {
  lenv* parent;
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

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  v->err  = malloc(512);

  /* printf the error string with a maximum of 511 characters */
  vsnprintf(v->err, 511, fmt, va);

  /* Reallocate to the number of bytes actually used */
  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);

  return v;
}

lval* lval_bool(int x) {
  if (x != 0 && x != 1)
  {
    return lval_err("Cannot create boolean value from the number %d", x);
  }

  lval* v = malloc(sizeof(lval));
  v->type = LVAL_BOOL;
  v->bl   = x;
  return v;
}

lval* lval_sym(char* s) {
  if (strcmp(s, "true") == 0)
  {
    return lval_bool(1);
  }

  if (strcmp(s, "false") == 0)
  {
    return lval_bool(0);
  }

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

lval* lval_fn(lbuiltin builtin) {
  lval* v    = malloc(sizeof(lval));
  v->type    = LVAL_FN;
  v->builtin = builtin;
  return v;
}

lenv* lenv_new(void);
lenv* lenv_copy(lenv* e);
void lenv_del(lenv* e);

lval* lval_lambda(lval* args, lval* body) {
  lval* v = malloc(sizeof(lval));

  v->type = LVAL_FN;
  v->builtin = NULL;
  v->env = lenv_new();
  v->args = args;
  v->body = body;

  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    // do nothing special for numbers / booleans
    case LVAL_LONG:
    case LVAL_DBL:
    case LVAL_BOOL:
      break;
    // for fns, nothing special needs to be done if it's a builtin;
    // if it's a user-defined fn, free the associated data
    case LVAL_FN:
      if (!v->builtin) {
        lenv_del(v->env);
        lval_del(v->args);
        lval_del(v->body);
      }
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
    /* Copy numbers and booleans directly */
    case LVAL_LONG: x->lng = v->lng; break;
    case LVAL_DBL: x->dbl = v->dbl; break;
    case LVAL_BOOL: x->bl = v->bl; break;

    case LVAL_FN:
      if (v->builtin) {
        x->builtin = v->builtin;
      } else {
        x->builtin = NULL;
        x->env = lenv_copy(v->env);
        x->args = lval_copy(v->args);
        x->body = lval_copy(v->body);
      }
      break;

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

int lval_eq(lval* x, lval* y) {
  if (x->type != y->type) { return 0; }

  switch (x->type) {
    case LVAL_LONG:
      if (y->type == LVAL_LONG) { return x->lng == y->lng; }
      if (y->type == LVAL_DBL)  { return x->lng == y->dbl; }

    case LVAL_DBL:
      if (y->type == LVAL_LONG) { return x->dbl == y->lng; }
      if (y->type == LVAL_DBL)  { return x->dbl == y->dbl; }

    case LVAL_ERR:
      return strcmp(x->err, y->err) == 0;

    case LVAL_SYM:
      return strcmp(x->sym, y->sym) == 0;

    case LVAL_FN:
      if (x->builtin || y->builtin) {
        return x->builtin == y->builtin;
      } else {
        return lval_eq(x->args, y->args) &&
               lval_eq(x->body, y->body);
      }

    case LVAL_SEXPR:
    case LVAL_QEXPR:
      if (x->count != y->count) { return 0; }
      for (int i = 0; i < x->count; i++) {
        if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
      }
      return 1;
    break;
  }

  // we should never get this far
  return -1;
}


int lval_compare(lval* x, lval* y, char* op) {
  if (strcmp(op, "==") == 0) {
    return lval_eq(x, y);
  }

  if (strcmp(op, "!=") == 0) {
    return !lval_eq(x, y);
  }

  if (strcmp(op, ">") == 0) {
    if(x->type == LVAL_LONG && y->type == LVAL_LONG) {
      return x->lng > y->lng;
    }

    if(x->type == LVAL_LONG && y->type == LVAL_DBL) {
      return x->lng > y->dbl;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_LONG) {
      return x->dbl > y->lng;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_DBL) {
      return x->dbl > y->dbl;
    }
  }

  if (strcmp(op, "<") == 0) {
    if(x->type == LVAL_LONG && y->type == LVAL_LONG) {
      return x->lng < y->lng;
    }

    if(x->type == LVAL_LONG && y->type == LVAL_DBL) {
      return x->lng < y->dbl;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_LONG) {
      return x->dbl < y->lng;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_DBL) {
      return x->dbl < y->dbl;
    }
  }

  if (strcmp(op, ">=") == 0) {
    if(x->type == LVAL_LONG && y->type == LVAL_LONG) {
      return x->lng >= y->lng;
    }

    if(x->type == LVAL_LONG && y->type == LVAL_DBL) {
      return x->lng >= y->dbl;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_LONG) {
      return x->dbl >= y->lng;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_DBL) {
      return x->dbl >= y->dbl;
    }
  }

  if (strcmp(op, "<=") == 0) {
    if(x->type == LVAL_LONG && y->type == LVAL_LONG) {
      return x->lng <= y->lng;
    }

    if(x->type == LVAL_LONG && y->type == LVAL_DBL) {
      return x->lng <= y->dbl;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_LONG) {
      return x->dbl <= y->lng;
    }

    if(x->type == LVAL_DBL && y->type == LVAL_DBL) {
      return x->dbl <= y->dbl;
    }
  }

  // we should never get this far
  return -1;
}

////////////////////////////////////////////////////////////////////////////////

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->parent = NULL;
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

lenv* lenv_copy(lenv* e) {
  lenv* n   = malloc(sizeof(lenv));
  n->parent = e->parent;
  n->count  = e->count;
  n->syms   = malloc(sizeof(char*) * n->count);
  n->vals   = malloc(sizeof(lval*) * n->count);
  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
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

  /*
   * If the symbol is not defined in the environment itself, tcheck its parent
   * environment.
   */
  if (e->parent) {
    return lenv_get(e->parent, k);
  }

  /* If the symbol is not defined anywhere, return an error. */
  return lval_err("unbound symbol: '%s'", k->sym);
}

/* Defines a value in the local environment. */
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

/* Defines a value in the global environment. */
void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->parent) { e = e->parent; }
  lenv_put(e, k, v);
}

////////////////////////////////////////////////////////////////////////////////

void lval_expr_print(lval* v, char open, char close);

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_LONG:  printf("%li", v->lng); break;
    case LVAL_DBL:   printf("%f", v->dbl); break;
    case LVAL_BOOL:  printf(v->bl == 0 ? "false" : "true"); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    case LVAL_FN:
      if (v->builtin) {
        printf("<builtin>");
      } else {
        printf("(\\ ");
        lval_print(v->args);
        putchar(' ');
        lval_print(v->body);
        putchar(')');
      }
      break;
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

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args); \
    return err; \
  }

#define LASSERT_NUM(fn, args, num) \
  LASSERT(args, args->count == num, \
    "Invalid number of arguments passed to '%s'. Got %i, expected %i.", \
    fn, args->count, num)

#define LASSERT_AT_LEAST_NUM(fn, args, num) \
  LASSERT(args, args->count >= num, \
    "Invalid number of arguments passed to '%s'. " \
    "Got %i, expected at least %i.", \
    fn, args->count, num)

#define LASSERT_AT_MOST_NUM(fn, args, num) \
  LASSERT(args, args->count <= num, \
    "Invalid number of arguments passed to '%s'. " \
    "Got %i, expected at most %i.", \
    fn, args->count, num)

#define LASSERT_TYPE(fn, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Incorrect type for argument #%i passed to '%s'. Got %s, expected %s.", \
    index + 1, fn, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUMBER_TYPE(fn, args, index) \
  LASSERT(args, (args->cell[index]->type == LVAL_LONG) || \
                (args->cell[index]->type == LVAL_DBL), \
    "Incorrect type for argument #%i passed to '%s'. " \
    "Got %s, expected %s or %s.", \
    index + 1, fn, ltype_name(args->cell[index]->type), \
    ltype_name(LVAL_LONG), ltype_name(LVAL_DBL))

#define LASSERT_NOT_EMPTY(fn, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Empty Q-expression passed to '%s' as argument #%i.", fn, index + 1);

// Returns a Q-expression containing the first element in the list.
lval* builtin_head(lenv* e, lval* a) {
  LASSERT_NUM("head", a, 1);
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("head", a, 0);

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

// Like head, but returns the element itself (not a Q-expression).
lval* builtin_first(lenv* e, lval* a) {
  LASSERT_NUM("first", a, 1);
  LASSERT_TYPE("first", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("first", a, 0);

  lval* v = lval_take(a, 0);
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  v = lval_take(v, 0);
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT_NUM("tail", a, 1);
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("tail", a, 0);

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_init(lenv* e, lval* a) {
  LASSERT_NUM("init", a, 1);
  LASSERT_TYPE("init", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY("init", a, 0);

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
  LASSERT_NUM("eval", a, 1);
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type == LVAL_SEXPR) {
      a->cell[i]->type = LVAL_QEXPR;
    }
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
  }

  lval* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_cons(lenv* e, lval* a) {
  LASSERT_NUM("cons", a, 2);
  LASSERT_TYPE("cons", a, 1, LVAL_QEXPR);

  lval* x = lval_pop(a, 0);
  x = lval_cons(x, lval_pop(a, 0));

  lval_del(a);
  return x;
}

lval* builtin_len(lenv* e, lval* a) {
  LASSERT_NUM("len", a, 1);
  LASSERT_TYPE("len", a, 0, LVAL_QEXPR);

  lval* qexp = lval_pop(a, 0);

  int l = qexp->count;
  lval_del(qexp);
  lval_del(a);
  return lval_long(l);
}

////////////////////////////////////////////////////////////////////////////////

lval* builtin_compare(lenv* e, lval* a, char* op, int math, int invert) {
  /* There must be at least one argument. */
  LASSERT_AT_LEAST_NUM(op, a, 1);

  /* If this is a mathematical comparison, then all args must be numbers */
  if (math) {
    for (int i = 0; i < a->count; i++) {
      LASSERT_NUMBER_TYPE(op, a, i);
    }
  }

  int result = 1;
  for (int i = 1; i < a->count; i++) {
    if (lval_compare(a->cell[i-1], a->cell[i], op) == 0) {
      result = 0;
      break;
    }
  }

  lval_del(a);

  if (invert) { result = !result; }

  return lval_bool(result);
}

lval* builtin_eq(lenv* e, lval* a) {
  return builtin_compare(e, a, "==", 0, 0);
}

lval* builtin_not_eq(lenv* e, lval* a) {
  return builtin_compare(e, a, "==", 0, 1);
}

lval* builtin_gt(lenv* e, lval* a) {
  return builtin_compare(e, a, ">", 1, 0);
}

lval* builtin_lt(lenv* e, lval* a) {
  return builtin_compare(e, a, "<", 1, 0);
}

lval* builtin_gte(lenv* e, lval* a) {
  return builtin_compare(e, a, ">=", 1, 0);
}

lval* builtin_lte(lenv* e, lval* a) {
  return builtin_compare(e, a, "<=", 1, 0);
}

lval* builtin_if(lenv* e, lval* a) {
  LASSERT_AT_LEAST_NUM("if", a, 2);
  LASSERT_AT_MOST_NUM("if", a, 3);
  LASSERT_TYPE("if", a, 0, LVAL_BOOL);

  lval* result;

  if (a->cell[0]->bl) {
    if (a->cell[1]->type == LVAL_QEXPR && a->cell[1]->count > 0) {
      a->cell[1]->type = LVAL_SEXPR;
    }
    result = lval_eval(e, lval_pop(a, 1));
  } else if (a->count == 3) {
    if (a->cell[2]->type == LVAL_QEXPR && a->cell[2]->count > 0) {
      a->cell[2]->type = LVAL_SEXPR;
    }
    result = lval_eval(e, lval_pop(a, 2));
  } else {
    /* return () if no 'else' clause and the condition is false */
    result = lval_sexpr();
  }

  lval_del(a);

  return result;
}

lval* builtin_or(lenv* e, lval* a) {
  LASSERT_AT_LEAST_NUM("||", a, 1);
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("||", a, i, LVAL_BOOL);
  }

  int result = 0;
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->bl) {
      result = 1;
      break;
    }
  }

  lval_del(a);

  return lval_bool(result);
}

lval* builtin_and(lenv* e, lval* a) {
  LASSERT_AT_LEAST_NUM("&&", a, 1);
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("&&", a, i, LVAL_BOOL);
  }

  int result = 1;
  for (int i = 0; i < a->count; i++) {
    if (!a->cell[i]->bl) {
      result = 0;
      break;
    }
  }

  lval_del(a);

  return lval_bool(result);
}

lval* builtin_not(lenv* e, lval* a) {
  LASSERT_NUM("!", a, 1);
  LASSERT_TYPE("!", a, 0, LVAL_BOOL);

  lval* this = lval_take(a, 0);
  lval* that = lval_bool(!this->bl);

  lval_del(this);
  return that;
}

////////////////////////////////////////////////////////////////////////////////

lval* builtin_var(lenv* e, lval* a, char* fn) {
  LASSERT_TYPE(fn, a, 0, LVAL_QEXPR);

  lval* syms = a->cell[0];
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
      "The first argument to '%s' must be a list of symbols. "
      "Got %s, expected %s.",
      fn,
      ltype_name(syms->cell[i]->type),
      ltype_name(LVAL_SYM));
  }

  LASSERT(a, syms->count == a->count - 1,
    "The number of symbols defined by '%s' must be equal to the number of "
    "values. Got %i, expected %i.",
    fn, syms->count, a->count - 1);

  for (int i = 0; i < syms->count; i++) {
    /* If fn is 'def', define globally; if '=' (put), define locally */
    if (strcmp(fn, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i+1]);
    }

    if (strcmp(fn, "=") == 0) {
      lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
  }

  lval_del(a);
  // return an empty sexp ()
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
  return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
  return builtin_var(e, a, "=");
}

lval* builtin_print_env(lenv* e, lval* a) {
  for (int i = 0; i < e->count; i++) {
    printf("%s: ", e->syms[i]);
    lval_println(e->vals[i]);
  }

  // return an empty sexp ()
  return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
      "The first argument to '\\' must be a list of symbols. "
      "Got %s, expected %s.",
      ltype_name(a->cell[0]->cell[i]->type),
      ltype_name(LVAL_SYM));
  }

  lval* args = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(args, body);
}

lval* builtin_exit(lenv* e, lval* a) {
  printf("Adiós!");
  putchar('\n');
  exit(0);

  // return an empty sexp () to appease the compiler
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
    LASSERT_NUMBER_TYPE(op, a, i);
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

void lenv_add_builtin(lenv* e, char* name, lbuiltin builtin) {
  lval* k = lval_sym(name);
  lval* v = lval_fn(builtin);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* List functions */
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "first", builtin_first);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "rest", builtin_tail); // alias for `tail`
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

  /* Comparison/equality functions */
  lenv_add_builtin(e, "if", builtin_if);
  lenv_add_builtin(e, "==", builtin_eq);
  lenv_add_builtin(e, "!=", builtin_not_eq);
  lenv_add_builtin(e, ">", builtin_gt);
  lenv_add_builtin(e, "<", builtin_lt);
  lenv_add_builtin(e, ">=", builtin_gte);
  lenv_add_builtin(e, "<=", builtin_lte);
  lenv_add_builtin(e, "||", builtin_or);
  lenv_add_builtin(e, "or", builtin_or); // alias
  lenv_add_builtin(e, "&&", builtin_and);
  lenv_add_builtin(e, "and", builtin_and); // alias
  lenv_add_builtin(e, "!", builtin_not);
  lenv_add_builtin(e, "not", builtin_not); // alias

  /* Variable functions */
  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=", builtin_put);
  lenv_add_builtin(e, "print-env", builtin_print_env);

  /* Function functions */
  lenv_add_builtin(e, "\\", builtin_lambda);

  /* REPL functions */
  lenv_add_builtin(e, "exit", builtin_exit);
}

////////////////////////////////////////////////////////////////////////////////

lval* lval_call(lenv* e, lval* f, lval* a) {
  /* If f is a builtin, simply call it as usual */
  if (f->builtin) { return f->builtin(e, a); }

  // Otherwise...

  /* Record argument counts */
  int given = a->count;
  int total = f->args->count;

  /* While there are still args to process... */
  while (a->count) {
    /* If we've run out of args to bind... */
    if (f->args->count == 0) {
      lval_del(a); return lval_err(
        "Function passed too many arguments. "
        "Got %i, expected %i.", given, total);
    }

    lval* sym = lval_pop(f->args, 0);

    /* Special case to deal with '&' */
    if (strcmp(sym->sym, "&") == 0) {
      /* Ensure '&' is followed by another symbol */
      if (f->args->count != 1) {
        lval_del(a);
        return lval_err(
          "Function format invalid. "
          "Symbol '&' not followed by a single symbol.");
      }

      /* Bind the next symbol to the list of remaining arguments. */
      lval* next_sym = lval_pop(f->args, 0);
      lenv_put(f->env, next_sym, builtin_list(e, a));
      lval_del(sym);
      lval_del(next_sym);
      break;
    }

    lval* val = lval_pop(a, 0);
    lenv_put(f->env, sym, val);

    lval_del(sym);
    lval_del(val);

  }

  lval_del(a);

  /* If '&' remains in the argument list, bind the next thing to an empty list */
  if (f->args->count > 0 && strcmp(f->args->cell[0]->sym, "&") == 0) {
    /* Ensure that there IS a next thing */
    if (f->args->count != 2) {
      return lval_err(
        "Function format invalid. "
        "Symbol '&' not followed by a single symbol.");
    }

    /* Pop and delete the '&' symbol */
    lval_del(lval_pop(f->args, 0));

    /* Pop the next symbol and create an empty list */
    lval* sym = lval_pop(f->args, 0);
    lval* val = lval_qexpr();

    /* Bind to environment and delete */
    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  /* If all of the args have been bound... */
  if (f->args->count == 0) {
    /* Set the parent environment to the evaluation environment */
    f->env->parent = e;

    /* Evaluate and return */
    return builtin_eval(f->env, lval_conj(lval_sexpr(), lval_copy(f->body)));
  }

  /* Otherwise, return a partially evaluated function */
  return lval_copy(f);
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

  /* Ensure first element is a function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FN) {
    lval* err = lval_err(
      "S-expression starts with incorrect type. "
      "Got %s, expected %s.",
      ltype_name(f->type), ltype_name(LVAL_FN));

    lval_del(f);
    lval_del(v);
    return err;
  }

  /*
   * Call the function on the arguments (remaining children) to get the
   * result.
   */
  lval* result = lval_call(e, f, v);
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

  // if root (>), read the last child (ignoring any others).
  //
  // t->children_num - 1 is some sort of regex (probably \$\), which causes an
  // address boundary error. t->children_num - 2 is what we really want.
  //
  // if t->children_num - 2 is a regex too (probably \^\), then we assume this
  // is an empty line and return an empty S-expression as a result
  if (strcmp(t->tag, ">") == 0) {
    mpc_ast_t* last_child = t->children[t->children_num - 2];

    if (strcmp(last_child->tag, "regex") == 0) {
      return lval_sexpr();
    } else {
      return lval_read(last_child);
    }
  }

  // if sexpr or qexpr, then create an empty list...
  lval* sexp = NULL;
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

void run_lispy_code(char* input_string, mpc_parser_t *parser, lenv* env) {
  mpc_result_t r;
  if (mpc_parse("<stdin>", input_string, parser, &r)) {
    lval* result = lval_eval(env, lval_read(r.output));
    lval_del(result);
    mpc_ast_delete(r.output);
  } else {
    mpc_err_print(r.error);
    mpc_err_delete(r.error);
  }
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
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!\\?&%\\|]+/ ;                   \
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

  /* Define additional builtins using Lispy syntax */
  run_lispy_code("(def {def\\}"
                 "  (\\ {args body}"
                 "    {def (head args) (\\ (tail args) body)}))",
                 Lispy, e);

  run_lispy_code("(def\\ {apply f xs}"
                 "  {eval (join (list f) xs)})",
                 Lispy, e);

  run_lispy_code("(def\\ {nth coll n}"
                 "  {if (== n 0)"
                 "    {first coll}"
                 "    {nth (rest coll) (- n 1)}})",
                 Lispy, e);

  run_lispy_code("(def\\ {second xs} {nth xs 1})", Lispy, e);
  run_lispy_code("(def\\ {third xs} {nth xs 2})", Lispy, e);
  run_lispy_code("(def\\ {fourth xs} {nth xs 3})", Lispy, e);
  run_lispy_code("(def\\ {fifth xs} {nth xs 4})", Lispy, e);

  run_lispy_code("(def\\ {last coll} {first (reverse coll)})", Lispy, e);

  run_lispy_code("(def\\ {flip f x y}"
                 "  {eval {f y x}})",
                 Lispy, e);

  run_lispy_code("(def\\ {reverse coll}"
                 "  {if (== coll {})"
                 "    {}"
                 "    {join (reverse (tail coll)) (head coll)}})",
                 Lispy, e);

  run_lispy_code("(def\\ {contains? coll x}"
                 "  {if (== (len coll) 0)"
                 "    false"
                 "    {if (== (first coll) x)"
                 "      true"
                 "      {contains? (rest coll) x}}})",
                 Lispy, e);

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
