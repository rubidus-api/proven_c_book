#import "../../book/lib.typ": *

= Diagnosis and control — `<errno.h>`, `<assert.h>`, `<signal.h>`, `<setjmp.h>`

#organizer[
  We look in one place at the four headers used when a program *has gone wrong*.
  The global holding an error number, the assertion that catches contract
  violations, signals flying in from outside, and the jump that skips over the
  stack. Why the discipline set up in chapter 45, "errors are values", matters so
  much becomes clear on seeing the real shape of the alternatives.
]

#deepqa[
  Chapter 45 said C's ways of reporting an error are only three — the return value,
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

== `errno` — the price of carrying errors in a global

#demo("examples/ch62/errno_demo.c")

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
  [`strerror_s`], [the same intent], [annex K (chapter 63)],
)

The error numbers the standard names are only three — `EDOM` (domain), `ERANGE`
(range), `EILSEQ` (encoding). The rest, such as `ENOENT` and `EACCES`, are settled
by POSIX or the platform. That is, *code comparing `errno` values is that much less
portable*.

== `assert` — the cheapest way to write a contract as code

The macro learned in chapter 45. Pinning down the rules again:

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

== Signals — interference flying in from outside

`<signal.h>` handles events coming from outside the program (Ctrl+C, a wrong
memory access, an arithmetic error). The signals the standard settles are only six
(`SIGINT`, `SIGSEGV`, `SIGFPE`, `SIGILL`, `SIGABRT`, `SIGTERM`); the rest are the
platform's.

The heart of it is the fact that *there is almost nothing that can be done inside a
handler.* What the standard permits is about this much.

- assigning a value to a variable of type `volatile sig_atomic_t`
- calling `_Exit` or `abort`
- setting the handler for the same signal again

Neither `printf` nor `malloc` may be called — because the signal can cut in while
those functions are halfway through executing (they are not *async-signal-safe*).
The idiom in the field is "the handler only raises a flag; the real handling
happens in the main flow."

```c
static volatile sig_atomic_t stop = 0;
static void on_int(int sig) { (void)sig; stop = 1; }
/* in the main loop: while (!stop) { ... } */
```

#misconception[
  "`SIGSEGV` can be caught and the program kept running"
][
  It can be caught but it cannot be kept running. `SIGSEGV` is a signal that comes
  *after the contract has already been broken* (chapter 46's undefined behaviour).
  Return normally from the handler and the same instruction is executed again and
  repeats endlessly, or it runs on over a damaged state. Leaving a stack trace for
  debugging and ending with `_Exit` is the realistic best, and "recovery" is mending
  the code so that the access is not made in the first place.
]

== Non-local jumps — `setjmp`/`longjmp`

A device that remembers the present place with `setjmp` and comes back later from
somewhere deep with `longjmp`. It is an attempt to make something like exceptions
in a language that has none, and the price is correspondingly large.

- The only local variables whose values can be believed after a `longjmp` are those
  declared `volatile`. The rest may have been in registers, so their values are
  undetermined.
- *Cleanup code does not run.* The resources (files, memory) held by the functions
  skipped over leak as they are. Because there is no device like C++'s destructors.
- If the function that called `setjmp` has already returned, `longjmp` is outside
  the contract.
- Escaping from a signal handler with `longjmp` is especially dangerous.

So the conclusion in the field is usually "do not use it". If there is a place for
it, it is confined to structures such as an interpreter's error recovery or a
parser's deep failure, where *the resources to clean up are bound into a single
arena* (Part XII's arena makes that condition).

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
    disciplined use meant by chapter 29's "use goto with restraint".

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
