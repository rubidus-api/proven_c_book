#import "../../book/lib.typ": *

= Diagnosis and control — `<errno.h>`, `<assert.h>`, `<signal.h>`, `<setjmp.h>`

#prereq(
  ([chapter 50, Errors and contracts], [reporting an error as a value]),
  ([chapter 51, Undefined behaviour], [after a contract is broken]),
)

#deepqa[
  Chapter 50 said C's ways of reporting an error are only three — the return value,
  global state, and stopping the program. Then exactly when can the global called
  `errno` be believed?
][
  It can be believed *only right after a failure has been confirmed.* The
  standard's rule is two-layered. First, a successful call may also put a value in
  `errno` — so the order must not be "nonzero means failure" but "read the reason
  after the function has reported failure". Second, the value may be overwritten by
  the next library call at any time — so once read it is stored at once. These two
  rules are the price of the design that carries errors in global state.
]

#organizer[
  We look in one place at the four headers used when a program *has gone wrong*.
  The global holding an error number, the assertion that catches contract
  violations, signals flying in from outside, and the jump that skips over the
  stack. Why the discipline set up in chapter 50, "errors are values", matters so
  much becomes clear on seeing the real shape of the alternatives.
]

#chapter-questions()

== `errno` — the price of carrying errors in a global

#demo("examples-en/ch74/errno_demo.c")

The output shows the rules exactly. Set it to 0 just before the call, read it
after confirming failure, and store it before doing anything else. Merely slipping
one line of `printf` in between can change the value.

`errno` looks like a variable but is *a macro*. In a program running along several
strands each strand must see a different value, so the implementation defines it
as a macro pointing at thread-local storage.

#dtable(
  columns: 3,
  [*function*], [*what it does*], [*note*],
  [`strerror(n)`], [error number → a sentence], [★ a static buffer. not thread-safe],
  [`perror(s)`], [`s: reason` to `stderr`], [the habit of attaching a context string],
  [`strerror_r`], [fills a caller's buffer], [POSIX. there are two editions, hence confusion],
  [`strerror_s`], [the same intent], [annex K (chapter 77)],
)

The error numbers the standard names are only three — `EDOM` (domain), `ERANGE`
(range), `EILSEQ` (encoding). The rest, such as `ENOENT` and `EACCES`, are settled
by POSIX or the platform. That is, *code comparing `errno` values is that much less
portable*.

#qa[
  Why was `assert` made to switch off wholesale with `NDEBUG` — is it not better to always check?
][
  Because what `assert` checks is *the programmer's assumption, not the user's
  input*. "By the time we are here, p is not null" must be true whenever the code
  is right; if it is false, that is a bug. The original design puts such checks
  densely during development and removes their cost from the shipped build.

  Two things follow. First, *no side effects inside `assert`* — `assert(pop(&s) == 3)`
  disappears entirely in the release build. Second, checks on user input, file
  contents and network data must be made by *code that always runs*, not by
  `assert`. In chapter 50's vocabulary of contracts, `assert` confirms
  preconditions *during development*; reporting failure as a value is another job.
]

== `assert` — the cheapest way to write a contract as code

The macro learned in chapter 50. Pinning down the rules again:

- `assert` confirms *an invariant internal to the program*. Things like "if we got
  this far, p is not null".
- *It is not used to check values that came from outside.* User input, file
  contents and network data are checked with `if` and handled as errors.
- If `NDEBUG` is defined it *vanishes entirely* (chapter 17). So an expression with
  side effects must not be put in it.

#antipattern[
  Making `assert` do work
][
  ```c
  assert(fclose(f) == 0);      /* in a release build the fclose itself vanishes */
  assert(i++ < n);             /* there arises a build in which i is not incremented */
  ```
  Separate the check from the side effect.
  ```c
  int rc = fclose(f);
  assert(rc == 0);
  (void)rc;                    /* prevents an unused warning in release */
  ```
]

C11 brought in `static_assert` (in C23 it can be used without `_Static_assert`).
It confirms at compile time, so the run-time cost is zero, and it is used on
`sizeof` and constant conditions.

```c
static_assert(sizeof(int) >= 4, "this code assumes a 32-bit int");
```

== Signals — see the next chapter

`<signal.h>` deals with events that come from outside the program (Ctrl+C, an
invalid memory access, an arithmetic error). The standard defines only six signals
(`SIGINT`, `SIGSEGV`, `SIGFPE`, `SIGILL`, `SIGABRT`, `SIGTERM`); the rest belong to
the platform.

One discipline is worth putting down here — *there is almost nothing a handler may
do.* Neither `printf` nor `malloc` may be called. So the working idiom becomes
"the handler raises a flag, the main flow does the work."

```c
static volatile sig_atomic_t stop = 0;
static void on_int(int sig) { (void)sig; stop = 1; }
/* in the main loop: while (!stop) { ... } */
```

There is enough in this header to fill a chapter, so it has one — the history, the
shape of the functions with their arguments and return values, what `sig_atomic_t`
really is, POSIX's `sigaction` and the inside of its structures, and the real uses
in servers, shells and terminals, all in chapter 75.

== Non-local jumps — `setjmp`/`longjmp`

A device that remembers the present place with `setjmp` and comes back later from
somewhere deep with `longjmp`. It is an attempt to make something like exceptions
in a language that has none, and the price is correspondingly large — *nobody
cleans up the resources held by the functions that were skipped over.*

This header too has enough in it for a chapter of its own — the return-value rule
of the two words, the four contexts in which `setjmp` may appear, the inside of
`jmp_buf`, the `volatile` rule measured on a real build, the uses in real
software such as libjpeg and Lua, and today's alternatives, all in chapter 76.

#realcase[
  The shape of code made by the way of handling errors
][
  Write the same program three ways and its shape splits like this.

  - *Return value + `errno`*: an `if` attaches to every call and the error paths
    are visible. Cumbersome, but the flow is honest.
  - *`setjmp`/`longjmp`*: the body becomes clean but a human must remember all the
    resource cleanup, and where control jumps to is not visible in the code.
  - *`goto cleanup`*: the compromise most widely settled in C codebases. On failure
    everything gathers at one place and cleans up in reverse order. The Linux
    kernel pinned this pattern down as a convention, and it is exactly the
    disciplined use meant by chapter 30's "use goto with restraint".

  All three solve the same problem of "handling failure as a value and not
  forgetting the cleanup". Part XII's library lays a fourth answer on top — *making
  failure into a type*.
]

#recap[
  Diagnosis and control in summary.

  #dtable(
    columns: 3,
    [*tool*], [*where it is used*], [*rule*],
    [`errno`], [the reason a library call failed], [0 just before, read after confirming failure, store at once],
    [`assert`], [internal invariants], [no side effects, not used for checking input],
    [`static_assert`], [compile-time assumptions], [zero cost],
    [`signal`], [external events such as Ctrl+C], [only a flag in the handler],
    [`SIGSEGV`], [—], [catching it does not allow recovery],
    [`setjmp`/`longjmp`], [special recovery], [values undetermined unless `volatile`, no cleanup],
    [the practical idiom], [cleanup on failure], [gather at one place with `goto cleanup`],
  )
]

That is as far as the headers that existed from the C89 days. The next chapter is
what the standard has added since — and the story of what became of the attempt to
make "safe functions".
