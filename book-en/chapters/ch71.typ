#import "../../book/lib.typ": *

= Signals — `<signal.h>`

#prereq(
  ([chapter 70, Diagnostics and control], [`errno` and the handler's restrictions]),
  ([chapter 50, The three faces of `main`], [a program's start and end]),
  ([chapter 3, Programs and processes], [the process as a container]),
)

#deepqa[
  Chapter 70 said a signal handler may not call `printf` or `malloc`, and that
  about all it may do is assign to a `volatile sig_atomic_t`. But a signal
  ultimately calls *a function inside my own program* — why are the restrictions
  so severe?
][
  *Because there is no telling when it will cut in.* A signal does not respect the
  boundaries of functions. It can arrive while `malloc` has half-rewritten its
  free list, or while `printf` has written half of a buffer. Call the same
  function again from a handler in that state and the data structure breaks.

  A signal, in other words, is *a third thing* — neither a thread nor a function
  call. It stops the current flow, cuts in, and returns, so there is no way to know
  "what is half-done right now". Every rule in this chapter follows from that one
  sentence.
]

#organizer[
#idx("signal")  A close reading of one header, `<signal.h>`. Where it came from (a Unix
  inheritance), the exact shape of its two functions with their arguments and
  return values, what `sig_atomic_t` really is, the list of what a handler may do,
  why memory cannot be allocated inside one, how the kernel saves and restores
  registers, POSIX's `sigaction` and the inside of its structures, and how
  servers, the JVM and garbage collectors actually use signals.
]

#chapter-questions()

== Where it came from — a Unix inheritance

C did not invent signals. Unix made them in the 1970s as "the cheapest way to
tell a process something", and the C standard took only *the minimum that would
hold anywhere*.

Early Unix signals had a famous flaw. Once a handler ran, the disposition
immediately reverted to the default (so the first line of a handler had to
re-install itself), and if the same signal arrived in that gap the program died.
Because of that window, the signals of the time were called *unreliable
signals*.

4.2BSD produced a new interface that fixed this (the `sigvec` family), and that
design was tidied into POSIX's `sigaction`. What the 1989 C standard took,
however, was not the fixed one but *the common denominator* — which is why the
old window is still in the standard's `signal` today.

#qa[
  Why did the standard not take the better `sigaction`?
][
  Because of C's long-standing principle: *take only what holds where there is no
  operating system* (chapter 57). `sigaction` stands on operating-system notions —
  processes, signal masks, restarting system calls. C must run on embedded chips
  that have none of those, so the standard fixed only "if there is such a thing as
  a signal, this much exists."

  So this chapter is in two layers. *The standard layer* (works anywhere, with
  gaps) and *the POSIX layer* (no gaps, but only on Unix-like systems). Practical
  Unix code uses the latter almost without exception.
]

== Two functions — the exact shape

The standard defines only two.

#dtable(
  columns: 2,
  keycol: false,
  [*Declaration*], [*What it does*],
  [`void (*signal(int sig, void (*func)(int)))(int);`], [Sets the disposition of signal `sig` to `func`, and returns *the previous one*],
  [`int raise(int sig);`], [Sends signal `sig` to the caller itself],
)

Why `signal`'s declaration is rough was read by procedure in chapter 55 — *"a
function taking a signal number and a handler, returning the previous handler."*

=== `signal`'s arguments and return value

#dtable(
  columns: 3,
  [*Place*], [*What you give*], [*Meaning*],
  [`sig`], [A signal number], [One of the standard six, or a value the implementation defines],
  [`func`], [`SIG_DFL`], [Back to default handling (mostly termination)],
  [`func`], [`SIG_IGN`], [Ignore — the signal arrives and nothing happens],
  [`func`], [A function pointer], [Use that function as the handler],
  [return], [The previous disposition], [`SIG_DFL`, `SIG_IGN` or the previous handler],
  [return], [`SIG_ERR`], [Setting failed; `errno` then holds the reason],
)

*Not discarding the return value* is the first discipline. Miss a failure and the
program dies quietly when the signal finally arrives.

=== `raise`'s argument and return value

`raise(sig)` sends the signal to the caller. It returns 0 on success and non-zero
on failure. If a handler is installed it runs *before `raise` returns* — that is,
synchronously.

That is exactly what `abort` does: it raises `SIGABRT`, and terminates abnormally
if there is no handler or the handler returns (chapter 62).

#demo("examples-en/ch71/sig_basic.c")

The demonstration shows four things in the flesh. *The previous value comes back*
(first line), *`raise` calls the handler on the spot* (second), *`SIG_IGN` makes
the signal vanish* (third), and the last one matters — *this implementation
resets to `SIG_DFL` right after handling.*

=== The six signals the standard defines

#dtable(
  columns: 3,
  [*Name*], [*When it arrives*], [*Default action*],
  [`SIGABRT`], [A call to `abort()`], [Abnormal termination],
  [`SIGFPE`], [An arithmetic error (divide by zero, overflow, …)], [Abnormal termination],
  [`SIGILL`], [Executing an invalid instruction], [Abnormal termination],
  [`SIGINT`], [Interactive attention (usually Ctrl+C)], [Termination],
  [`SIGSEGV`], [An invalid memory access], [Abnormal termination],
  [`SIGTERM`], [A termination request], [Termination],
)

The standard does not fix the numeric values — the 2 and 15 the demonstration
printed belong to this implementation. The rule is *use the names, never the
numbers*.

== What a handler may do

The most important section in this chapter. The standard (§7.14.1.1) settles what
is permitted inside a handler *by enumeration*. Outside that list is undefined
behaviour.

#dtable(
  columns: 2,
  [*What is permitted*], [*Condition*],
  [*Assigning a value* to a `volatile sig_atomic_t` object], [Assigning, not reading],
  [Handling lock-free atomic objects], [`<stdatomic.h>`, when lock-free (chapter 74)],
  [Calling `abort`], [—],
  [Calling `_Exit`], [—],
  [Calling `quick_exit`], [—],
  [Calling `signal`], [Only with *the signal number that invoked it*],
)

Every other standard library function is forbidden — `printf`, `malloc`,
`strlen`, even `exit`. And touching objects with static or thread storage
duration is forbidden too unless it falls under the first row.

#misconception[
  "Logging with `printf` in a handler is convenient"
][
  The most common and longest-lived accident. `printf` is a function with internal
  buffers and locks, so cutting into its *intermediate state* breaks its data
  structures. On a good day the output interleaves; on a bad one it deadlocks — a
  signal arrives while `printf` holds a lock, the handler calls `printf` again, and
  it waits for a lock it holds itself.

  Worse, *it mostly works*. So the bug passes the tests, ships, and shows up as an
  occasional freeze under load.

  If something really must be printed from a handler on Unix, use `write` — a
  function POSIX separately guarantees to be *async-signal-safe*. The second
  demonstration does exactly that.
]

=== What `sig_atomic_t` really is

`sig_atomic_t` is an integer type whose value *never appears half-written* even
when a signal cuts in. Why does such a type need to exist? Because a large integer
may be stored in two steps on some machines, and a signal arriving in between
would see a half-changed value.

`volatile` is there for a different reason. The compiler may decide "nothing in
this loop changes `stop`" and delete the test altogether (chapter 14's
optimisation). `volatile` says *really read it every time*. The two do different
jobs, so *both* are needed — `volatile sig_atomic_t`.

#dtable(
  columns: 3,
  [*What*], [*What it prevents*], [*Without it*],
  [`sig_atomic_t`], [Seeing a half-written value], [A partially updated value can be read],
  [`volatile`], [The compiler eliding the read], [The loop never sees the flag],
)

Since C11, lock-free atomic types such as `atomic_int` may also be used in a
handler (chapter 74). In a program with several threads that is the more accurate
choice — `sig_atomic_t` guarantees only *signal versus main flow*, not thread
against thread.

== Handlers and memory — why `malloc` is not on the list

The most keenly felt absence from the permitted list is memory allocation. To
record anything inside a handler you need a vessel, and neither `malloc` nor
`free` may be called. Seeing why at the machine level makes the rule stick.

An allocator manages a data structure called the *free list* (chapter 42). One
`malloc` is a multi-step update that detaches a piece from that list and rewrites
the links of its neighbours. There is necessarily a moment when the update is
half done, and a signal can cut in *at exactly that moment*.

#dtable(
  columns: 2,
  [*The moment it cuts in*], [*If the handler calls `malloc`*],
  [The free-list links are half rewritten], [It follows a half-rewritten list and hands out the wrong piece],
  [Just before the block's size is written], [A later `free` returns it with the wrong size],
  [While the allocator's lock is held], [It waits for a lock it holds itself — deadlock],
)

The third row is the nastiest. Modern allocators keep an internal lock against
being called from several threads at once. Let the thread holding that lock take
a signal, and let the handler call `malloc` again, and the lock is never
released. *The program does not die; it stops* — the hardest kind of accident to
diagnose.

There is one worse combination. Escaping from a handler with `longjmp`
(chapter 72). Here the trouble comes without calling `malloc` again — because you
*jump out still holding the lock*. From then on the main flow's first `malloc`
hangs. Half the reason chapter 72 calls jumping from a handler dangerous is this.

#qa[
  Then what do you do when a handler really must record something?
][
  *Take it in advance.* Allocate the vessel you need *before* installing the
  handler, and let the handler only write into it. A static array is better still
  — there is no allocation at all.

  ```c
  static char report[4096];               /* obtained up front */
  static volatile sig_atomic_t report_len;
  ```

  If the amount to record is not bounded, it was never a job for a handler. Raise
  a flag and hand it to the main flow. If something truly must be recorded now —
  a crash report, where the next moment is certain death — put it in a
  pre-allocated buffer and push it out with `write`. That is what Redis's crash
  report, seen later, does.
]

== What a signal saves and restores — the register context and `errno`

A handler cuts in halfway through a function and *returns to exactly that place*,
even though half-computed values are scattered across the registers. How?

*Because the operating system saves every register and restores them.* When
delivering a signal the kernel builds a signal frame on the stack and puts the
whole current register set into it (on Linux, `ucontext_t` is that vessel). When
the handler returns, `sigreturn` puts those values back. So *whichever
instruction boundary it cut in at, the computation carries on.*

#demo("examples-en/ch71/sig_context.c")

The third part of the demonstration confirms it. A signal taken in the middle of
a thousand-round computation (at round 500) leaves the result equal to the one
computed undisturbed.

#qa[
  Does `setjmp`/`longjmp` not do the same thing?
][
  No. This is the decisive difference between the two devices.

  #dtable(
    columns: 3,
    keycol: false,
    [], [*A signal*], [*`longjmp` (chapter 72)*],
    [Who saves], [The kernel], [The `setjmp` macro],
    [What is saved], [*Every* register], [Only the callee-saved ones (eight slots)],
    [Where it returns], [The very instruction interrupted], [The place that called `setjmp`],
    [Local variables], [All intact], [*No guarantee unless `volatile`*],
  )

  A signal, then, is *a complete context switch*, and `longjmp` is *a partial
  restoration*. That is why a handler may return into the middle of an expression
  while `longjmp` may only be used in the four contexts the standard fixes
  (chapter 72).
]

=== `errno` is the exception — you must look after it yourself

If the kernel looks after the registers, what is left? *State at the C level.*
The one most often hit is `errno`.

A handler that calls nothing but `write` still changes `errno` when that call
fails. If the main flow was about to read the reason for a failed call, the value
it reads is the one the signal left behind.

The first part of the demonstration shows it in the flesh — `errno`, which was
`ENOENT` (2) before the signal, came back as `EBADF` (9) after the handler. An
attempt to open a missing file was turned into a bad-file-descriptor error.

The prescription is two lines. *Save on the way in, restore on the way out.*

```c
static void on_signal(int sig) {
    int saved = errno;          /* first line */
    /* … raise a flag, write, … */
    errno = saved;              /* last line */
}
```

Code that omits it *mostly works* — the fault shows only when a signal happens to
arrive at that moment. So it is better kept as a discipline.

#platform[
  Where the stack goes — the red zone and an alternate stack
][
  The kernel builds the signal frame *on the current stack*. Two pieces of
  practical knowledge follow.

  First, the x86-64 SysV convention has a 128-byte *red zone* below the stack
  pointer that a function may use without growing the stack. The kernel skips
  that zone when building a signal frame — otherwise it would overwrite the
  interrupted function's temporaries. The practice of building kernel code with
  `-mno-red-zone` comes from here.

  Second, if the `SIGSEGV` came from *the stack overflowing*, there is no stack on
  which to build the handler either. So POSIX provides `sigaltstack` to register a
  separate stack for handlers, used by passing `SA_ONSTACK`. Tools that diagnose
  stack overflow stand on this device.
]

== The working pattern — raise a flag and return at once

Given so short a permitted list, handlers in practice converge on one shape.

#demo("examples-en/ch71/sig_flag.c")

*The handler only raises a flag. Judgement and cleanup belong to the main flow.*
The demonstration's `serve` is that structure — it loops, sees the flags, and
either reloads its configuration or cleans up and goes down. Printing, closing
files and freeing all happen inside the loop.

Three things make the pattern good. *It is safe* — the handler does one
assignment, so it cannot leave the permitted list. *The moment is yours* — the
main flow decides whether to finish the current request first. *It is testable* —
raise the flag directly and the same path can be exercised without any signal.

#realcase("Why the output order looked reversed")[
  The first run of the demonstration printed the handler's output *before* the
  `puts`. Not a bug but buffering — `printf` and `puts` accumulate in the stdout
  buffer and flush later, while the handler's `write` goes straight out, bypassing
  it (chapter 59's buffering).

  That small observation re-explains the misconception above. The handler and the
  main flow share a buffer but *do not write by the same rules*. So the
  demonstration used `fflush` to line the order up, and practice avoids printing
  from handlers altogether.
]

== POSIX's `sigaction` — filling the standard's gaps

What Unix-like systems actually use is `sigaction`. It fills exactly three gaps in
the standard `signal`.

#dtable(
  columns: 3,
  [*Gap*], [*Standard `signal`*], [*`sigaction`*],
  [Does the handler survive?], [Implementation-defined (it vanished in the demonstration)], [It stays, unless `SA_RESETHAND` is given],
  [If the same signal arrives while handling], [Implementation-defined], [Blocked by default; `sa_mask` blocks more],
  [Interrupted system calls], [Not settled], [Restarted automatically with `SA_RESTART`],
)

#demo("examples-en/ch71/sig_action.c")

=== Inside `struct sigaction`

This structure is this chapter's data-type story. POSIX fixes four members and
does *not* fix their order (so it must be started with a designated initializer or
`memset`).

#dtable(
  columns: 3,
  [*Member*], [*Type*], [*What it is*],
  [`sa_handler`], [`void (*)(int)`], [The plain handler, shaped like the standard's],
  [`sa_sigaction`], [`void (*)(int, siginfo_t *, void *)`], [The one used with `SA_SIGINFO`; more information arrives],
  [`sa_mask`], [`sigset_t`], [Signals blocked *while this handler runs*],
  [`sa_flags`], [`int`], [Flags choosing the behaviour (below)],
)

`sa_handler` and `sa_sigaction` usually *overlap in a union* (chapter 44). So only
one is filled, and the `SA_SIGINFO` flag says which.

The four flags most often seen:

#dtable(
  columns: 2,
  [*Flag*], [*Meaning*],
  [`SA_SIGINFO`], [Use the three-argument handler — who sent it, and why],
  [`SA_RESTART`], [Automatically restart system calls interrupted by the signal],
  [`SA_NOCLDWAIT`], [Leave no zombie when a child ends (`SIGCHLD`)],
  [`SA_RESETHAND`], [Reset to the default after one delivery, the old way],
)

=== `siginfo_t` — who sent it, and why

With `SA_SIGINFO` the handler receives a `siginfo_t *`. The members most used:

#dtable(
  columns: 2,
  [*Member*], [*What it is*],
  [`si_signo`], [The signal number],
  [`si_code`], [*Why* it came — `SI_USER` (kill), `SI_KERNEL`, `SI_TIMER`, …],
  [`si_pid`], [The sending process's id],
  [`si_uid`], [The sending user's id],
  [`si_addr`], [For `SIGSEGV` and `SIGBUS`, *the address that faulted*],
)

The demonstration prints `si_code`. Raised at ourselves, Linux put `SI_TKILL`
(−6) there — "sent by the same thread". The names and meanings of these values
differ per implementation, so *check that platform's documentation before
branching on one.*

`si_addr` earns its keep in debugging. Record it in a `SIGSEGV` handler and you
learn "which address it died touching" — though it cannot be printed from inside
that handler (the permitted list), so raw bytes are usually written with `write`
and the program ended with `_Exit`.

=== Blocking a signal for a while — the mask

`sigprocmask` declares "I will not receive this signal for now". A signal arriving
while blocked does not vanish; it stays *pending* and is delivered the moment the
block is lifted. The demonstration's last block is that scene — raised while
blocked, the count stayed put; unblocked, it rose at once.

Where this is needed is clear. When a signal must not cut in while a data
structure is being fixed, block it for that stretch. It makes a *critical section*
against signals as well.

== Real uses

Now to what signals actually do in practice.

#dtable(
  columns: 3,
  [*Signal*], [*Use*], [*Representative case*],
  [`SIGTERM`], [*A graceful shutdown request* — time to clean up], [`kill`'s default, container shutdown in Docker and Kubernetes],
  [`SIGINT`], [The user's interruption (Ctrl+C)], [A command-line tool wrapping up its work],
  [`SIGKILL`], [Kill at once — *cannot be caught*], [The last resort when graceful shutdown fails],
  [`SIGHUP`], [Re-read configuration (by convention)], [nginx and Apache reloading without downtime],
  [`SIGCHLD`], [A child has ended], [Shells and servers reaping zombies],
  [`SIGPIPE`], [Wrote to a pipe whose reader is gone], [Servers mostly ignore it and handle `EPIPE`],
  [`SIGWINCH`], [The terminal was resized], [`vim` and `top` redrawing the screen],
  [`SIGUSR1`, `SIGUSR2`], [The application decides the meaning], [nginx's live binary upgrade, reopening log files],
)

=== Graceful shutdown — the most widely used pattern

It became especially important in the container world. An orchestrator taking a
container down sends `SIGTERM` first, and kills it with `SIGKILL` if it has not
finished within the grace period (usually 30 seconds). So a server receiving
`SIGTERM` must *stop accepting new requests, finish those in flight, close its
connections and go down*.

The flag pattern above does exactly this work. The handler only sets `stop = 1`
and the main loop sees it and walks through the cleanup.

=== `SIGPIPE` — the signal whose right answer is to ignore it

Write to a pipe or socket whose reader has already gone and `SIGPIPE` arrives. The
default action is *terminating the process* — for a web server, dying because a
client closed a window.

So network programs almost invariably begin like this:

```c
signal(SIGPIPE, SIG_IGN);   /* let write return -1/EPIPE instead of a signal */
```

Ignored, `write` returns the failure *as a value* (`errno == EPIPE`). Chapter 70's
"failure as a value" is the better arrangement here too.

=== Interrupted system calls — `EINTR`

When a signal arrives, slow system calls such as `read` and `write` are *cut off*,
return −1 and put `EINTR` in `errno`. Not knowing this produces the ghost bug of
"reads sometimes fail".

There are two prescriptions: give `SA_RESTART` so the kernel restarts them, or
retry by hand.

```c
ssize_t n;
do { n = read(fd, buf, len); } while (n < 0 && errno == EINTR);
```

`SA_RESTART` is not a cure-all either — some calls are not restarted (notably
those with timeouts), so robust code keeps the retry loop as well.

#realcase("The self-pipe trick and its descendants")[
  Mixing signals with an event loop was a long-standing nuisance. A signal arriving
  while waiting in `select` or `poll` breaks the loop, and almost nothing may be
  done inside the handler.

  So in the 1990s the *self-pipe trick* appeared. A program makes a pipe to itself,
  and the handler writes a single byte into it (`write` is on the safe list). The
  event loop then receives that as *an ordinary readable event* — the signal has
  been turned into a file descriptor.

  Today Linux offers `signalfd`, providing the idea in the kernel directly, and the
  BSDs have `kqueue`'s `EVFILT_SIGNAL`. Different names, same idea — *turn a signal
  from an asynchronous interruption into an event that queues.*
]

#qa[
  In a program with several threads, where does a signal go?
][
  A delicate place, and without knowing the rules it becomes a bug that is hard to
  reproduce.

  A signal sent to the process (`kill`) is delivered to *any one thread that has
  not blocked it* — which one is not fixed. `pthread_kill`, by contrast, goes to
  the thread named. And while handlers are shared by the whole process, *the mask
  is per thread*.

  So the standard practice is this — *block the signal in every thread and let one
  dedicated thread wait for it with `sigwait`*. The signal then turns from an
  asynchronous interruption into an ordinary function return, and inside that
  thread `printf` and `malloc` are free to use. It is the same idea as `signalfd`
  above.
]

=== Which software uses signals, and why

The reason for using signals differs from program to program, and those reasons
make the device's place clear.

#dtable(
  columns: 3,
  [*What*], [*What it uses them for*], [*Why it had to be a signal*],
  [nginx, Apache], [`SIGHUP` to reload configuration, `SIGUSR2` to swap the executable], [The cheapest channel by which an operator gets *from outside to inside*. No port, no socket],
  [PostgreSQL], [Query cancellation and shutdown requests (the handler only raises a flag)], [One process per connection, so a signal between processes *is* the means of communication],
  [Redis], [`SIGSEGV`/`SIGBUS` handlers that print a crash report], [The *last chance* to record the state at the moment of death — into a pre-allocated buffer, out through `write`],
  [The HotSpot JVM], [`SIGSEGV` for null checks and safepoints], [Removes the check from the normal path entirely and leaves the rare case to a hardware trap],
  [WebAssembly runtimes], [`SIGSEGV`/`SIGBUS` trap handlers for out-of-bounds access], [Leaves bounds checking to guard pages, removing a compare from every access],
  [The Boehm GC], [`mprotect` + `SIGSEGV` as a write barrier, signals to stop threads], [The only way to put a collector on a C program without help from the language],
  [libuv, Node.js], [A dedicated thread and a pipe turn signals into events], [To mix with an event loop, a signal must become a file descriptor],
  [CPython], [The handler only raises a flag; the bytecode loop checks it], [Interpreter state cannot be touched from a handler — the permitted list again],
  [libcurl], [Ignores `SIGPIPE`; older versions used `SIGALRM` to time out name resolution], [A legacy of days with no other way to impose a timeout. Risky enough to deserve its own off switch (`CURLOPT_NOSIGNAL`)],
)

They sort into three groups. *Instructions arriving from outside* (nginx,
PostgreSQL — the channel of operation), *lifting a hardware trap into user code*
(the JVM, WebAssembly, the GC, Redis — emptying the normal path and signalling
only the exception), and *turning signals into events* (libuv, CPython — the
modern prescription for getting around the permitted list).

The middle group is the interesting one. "Remove the null check and catch it with
`SIGSEGV`" is the archetype of an optimization that *pushes the cost onto the
rare case* — one instruction is deleted from the normal path at the price of a
long trip through the kernel and a handler when the accident happens. It is the
same calculation as chapter 11's "a rare branch may be expensive."

#antipattern("cleaning up directly in the handler")[
  ```c
  static void on_term(int sig) {
      (void)sig;
      fclose(logfile);        /* standard library — forbidden */
      free(buffer);           /* forbidden */
      printf("bye\\n");        /* forbidden */
      exit(0);                /* exit is forbidden too (_Exit is allowed) */
  }
  ```
  All four are outside the permitted list. And this code is more dangerous for
  *mostly working* — the trouble only surfaces under load or when signals crowd in.

  The correct form is one flag.

  ```c
  static volatile sig_atomic_t stop;
  static void on_term(int sig) { (void)sig; stop = 1; }
  ```
]

#recap[
  #dtable(
    columns: 2,
    [*What to keep*], [*The point*],
    [What it is], [Neither a thread nor a call — a third thing that cuts in unpredictably],
    [The standard's scope], [Two functions (`signal`, `raise`) and six signals],
    [`signal`'s return], [*The previous* disposition; `SIG_ERR` means failure],
    [Inside a handler], [The permitted list is all there is — in practice, one assignment],
    [`volatile sig_atomic_t`], [Both are needed: one against half values, one against optimisation],
    [Allocation], [`malloc` is barred by the free list and the lock — obtain vessels *in advance*],
    [Context], [The kernel restores every register. Only `errno` must be saved by hand],
    [POSIX], [`sigaction` fills the three gaps (reset, re-entry, `EINTR`)],
    [In practice], [Handler raises a flag, the main flow cleans up; ignore `SIGPIPE`],
    [Modern alternatives], [`signalfd`, `kqueue`, a dedicated thread — turn signals into *events*],
  )
]

We have handled the interruption that arrives from outside. The next chapter is
its opposite — the device with which a program cuts its own flow and leaps back
up the stack, `setjmp` and `longjmp`.
