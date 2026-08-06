/* Gnarly declarations that really shipped — and how a procedure unravels them. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

/* -- (1) the standard's signal — exactly as it stands in C §7.14.1.1:
      void (*signal(int sig, void (*func)(int)))(int);
   Split into layers with typedef it becomes this. */
typedef void handler_t(int);          /* name the function type itself */
static void on_int(int sig) { (void)sig; }

/* -- (2) the shape of X11's XSetErrorHandler:
      int (*XSetErrorHandler(int (*handler)(Display *, XErrorEvent *)))();
   Copied as is, with stand-in types for Display and XErrorEvent. */
typedef struct { int dummy; } Display;
typedef struct { int code; } XErrorEvent;

static int my_error_handler(Display *d, XErrorEvent *e)
{ (void)d; printf("  error handler: code=%d\n", e->code); return 0; }

/* Raw: "a function taking a handler and returning the previous handler" */
static int (*set_error_handler(int (*handler)(Display *, XErrorEvent *)))
           (Display *, XErrorEvent *)
{
    static int (*current)(Display *, XErrorEvent *);
    int (*prev)(Display *, XErrorEvent *) = current;
    current = handler;
    return prev;
}

/* The same with typedef — one line becomes three, and all three read */
typedef int error_handler_t(Display *, XErrorEvent *);
static error_handler_t *set_error_handler2(error_handler_t *handler)
{ return set_error_handler(handler); }

/* -- (3) an array of pointers to functions — a dispatch table ---------- */
static int cmd_add(int a, int b) { return a + b; }
static int cmd_mul(int a, int b) { return a * b; }

/* int (*table[2])(int, int) — an array of "pointer to function taking two
   ints and returning int". Read by the procedure, that is what comes out. */
static int (*table[2])(int, int) = { cmd_add, cmd_mul };

/* -- (4) a function returning a pointer to an array -------------------- */
static int (*rows(void))[4]            /* "function returning pointer to int[4]" */
{
    static int grid[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };
    return grid;                        /* decays to int (*)[4] */
}

int main(void)
{
    /* (1) signal returns the *previous* handler — hence the function-pointer return */
    handler_t *old = signal(SIGINT, on_int);
    printf("signal returns the previous handler: %s\n",
           old == SIG_ERR ? "SIG_ERR" : "a previous value");
    (void)signal(SIGINT, old == SIG_ERR ? SIG_DFL : old);

    /* (2) the same pattern — install, and get the previous one back */
    puts("\ninstalling an X11-style error handler:");
    int (*prev)(Display *, XErrorEvent *) = set_error_handler(my_error_handler);
    printf("  previous handler: %s\n", prev ? "present" : "none (first install)");
    Display d = { 0 };
    XErrorEvent e = { .code = 42 };
    error_handler_t *prev2 = set_error_handler2(my_error_handler);  /* typedef form */
    printf("  the typedef version does the same: previous handler %s\n",
           prev2 == my_error_handler ? "identical" : "different");
    my_error_handler(&d, &e);

    /* (3) the dispatch table */
    printf("\ndispatch table: add(3,4)=%d, mul(3,4)=%d\n", table[0](3, 4), table[1](3, 4));

    /* (4) a pointer to an array */
    int (*g)[4] = rows();
    printf("pointer to array: g[2][1] = %d (one step is %zu bytes)\n",
           g[2][1], sizeof *g);
    return 0;
}
