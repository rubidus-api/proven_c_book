#import "../../book/lib.typ": *

= The boundaries — running things overlapped, and when there is no OS

#organizer[
  The last chapter of Part XII. We see the two ways of running several things
#idx("coroutine")  overlapped (stackless coroutines and the job system), why
  allocators and pointer provenance become a problem in a program running along
  several strands, and what shape this library takes in a place with no operating
  system at all. The last section is *when it is better not to use it*.
]

#deepqa[
  Chapter 12 said cores became several because of the clock's limits, and that
  accidents such as false sharing arise. Then how many different meanings does the
  phrase "doing several things at once" have?
][
  At least two. *Concurrency* is several tasks progressing by turns while waiting for
  each other, and it holds even with one core — such is a server doing other work
  while waiting for input and output. *Parallelism* is several cores really
  calculating at the same moment. The former is a problem of *structure*, the latter a
  problem of *performance*. This chapter's coroutines treat the former and the job
  system the latter.
]

== Stackless coroutines — overlapping without threads

If a function can do some work, stop, and later start again *from that place*, much
becomes easy. Because the code can be written in order instead of a state machine
being written by hand.

#demo("examples/ch78/coro.c")

The heart of it is the last line — the state this coroutine remembers is *4 bytes*.
There is no separate stack, no thread and no allocation. It is a tidying up of an old
knack in C in which macros make re-entry points with `switch` and `case` (a cousin of
Duff's device — that syntax seen in chapter 30 is used again here).

The price is clear too. *Local variables do not survive* — stop and come back and they
are gone, so state that must be maintained has to be kept in a struct. It is why the
example kept `sent` as a struct member. And, being implemented with `switch`,
`PROVEN_CORO_YIELD` cannot be put inside another `switch`.

Even so the reason this model is loved in embedded work is plain. If one task is
4 bytes, hundreds may be raised with no burden, and since no stack is taken separately
the memory usage can be calculated at compile time.

#misconception[
  "A coroutine is a light version of a thread"
][
  What they do is entirely different. Threads are switched by the operating system
  *cutting in as it pleases* (pre-emption), while a coroutine stops only where it
  itself wrote `YIELD` (co-operation). So race conditions do not arise between
  coroutines — because two coroutines never run at the same moment. There is a price
  in exchange. If one coroutine calculates long without yielding, all the rest starve.
  And coroutines cannot use several cores — which is why the next section's job system
  exists separately.
]

== The job system — using several cores

`proven_job_sys_t` is a small system equipped with a queue of work and worker threads.
Its characteristic is that the queue's size is *fixed* — if it overflows, submission
fails, and that failure comes as a value. The judgement is that an infinitely growing
queue is merely a device for putting the problem off until memory is exhausted.

#antipattern[
  Overlapping closing with submitting
][
  ```c
  /* thread A */                    /* thread B */
  proven_job_submit(sys, job);      proven_job_system_destroy(sys);
  ```
  A pattern the header explicitly forbids. The correct order is *close, join all the
  producers, then destroy*. Such ordering contracts are not solved by hiding a lock
  inside the data structure — rather, a hidden lock increases the code that "mistakenly
  believes it works". It is also why this library puts no locks in its containers and
  pins down that *shared mutation is synchronised by the caller*.
]

== Threads, allocators, and provenance

In a program running along several strands the allocator demands special care. If two
threads use one arena together, the simple action seen in chapter 73 of "cutting from
the front" becomes a race at once. The prescription is mostly *one per thread* — and
then no synchronisation is needed at all.

Pointer *provenance*, whose name only was seen in chapter 14, becomes practical here
too. Since even at the same address it matters which allocation a pointer came from,
giving a block obtained from one allocator back to another is a contract violation
(as in chapter 73's counterexample) even if the address happens to match. The library's
name came from here.

== When there is no OS — freestanding

This is the constraint that most shaped this library. It is the demand to run in the
very environment chapter 53 gave as the reason the standard library is thin — a place
with no operating system, no heap and no files.

#idx("freestanding")What changes in a freestanding build is this.

- `platform/` is not included at all. Files, time and OS randomness disappear.
- Allocation is done with an arena over a static buffer (chapter 73). There is no
  `malloc`.
- Strings are handled with `_borrow` over stack and static buffers (chapter 74).
- The panic handler is registered by you — there may be no console to print to, so it
  becomes a matter of lighting an LED, kicking a watchdog or rebooting.
- Real-number formatting uses large-integer arithmetic, so it can be taken out whole
  if it is not needed.

Why the disciplines of the earlier chapters — "take the allocator as an argument", "a
view is borrowed", "no hidden globals" — were so persistent shows itself here. Those
disciplines were not a taste but *the minimum condition for running in this
environment*.

#realcase[
  The same code in two worlds — and its price
][
  Keeping as one set the code that runs both in embedded work and on a host really is
  of great value. If a protocol parser can be tested on a PC and put into the firmware
  as it stands, the time spent floundering on a board with no debugging environment
  shrinks greatly. It is why many embedded teams keep a separate "test build that runs
  on the host", and what makes that structure possible is exactly the design of
  *confining platform dependence to one layer*. The price is that the API becomes a
  little more formal — code that works anywhere is specialised to nowhere.
]

== When it is better not to use it

We close honestly. There are cases where this library is not the answer.

- *Short, script-like programs* — for a twenty-line tool, ownership discipline and
  allocator parameters are mere formality. The standard library is enough.
- *A codebase that already has other conventions* — a large project mostly already has
  its own string, container and error conventions. Mix two conventions and conversion
  code arises at every boundary, and those boundaries become the places of new bugs.
- *Places like C++ and Rust where the language already solves the problem* — in an
  environment where the language handles ownership and error propagation, there is
  little reason to lay a C library on top.
- *Work that one standard function finishes* — exactly as this book said in
  chapter 53. The right attitude is neither "do not use it" nor "use it
  unconditionally" but *knowing the contract and choosing*.

#qa[
  Then what remains of what was learned in this part if proven is not used?
][
  Almost all of it. Strings that carry their length, writes that refuse rather than
  truncate, errors that come back as values, allocation visible in the signature, the
  distinction of owning and borrowing, size calculation done with checked arithmetic,
  comparators that keep a total order, hashes that assume adversarial input — these are
  not a library but *design principles*, and they can be applied by hand to any C code.
  Open chapter 69's table again and the right-hand column is entirely such items. What
  this part really wanted to sell is not the code but that column.
]

#recap[
  A summary of the whole of Part XII — problems and answers.

  #dtable(
  columns: 3,
    [*chapter 69's problem*], [*the answer*], [*chapter*],
    [strings that do not know the size], [views (ptr+size), refusing rather than truncating], [chapters 72 and 74],
    [unconfirmed failure], [errors as values, `[[nodiscard]]`], [chapter 71],
    [format mismatch], [`{}` and `PROVEN_ARG` (`_Generic`)], [chapter 75],
    [unclear ownership], [the allocator parameter, owning versus borrowing], [chapter 73],
    [unchecked callbacks], [documented contracts, introsort, keyed hashes], [chapter 76],
    [the hidden type of bytes], [`proven_byte_t`], [chapter 72],
    [environments with no OS], [separating the platform layer, static arenas], [chapter 78],
)
]

With this, the promise this book made in chapter 1 has been kept — showing C's problems
first, and showing one answer to them through to the end. The two remaining chapters
are the story beyond these pages. In the next chapter we look round the terrain of
practice (build tools, version control, real projects), and in the last we gather up the
road this book has travelled.
