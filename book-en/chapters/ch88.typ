#import "../../book/lib.typ": *

= The boundaries — running things overlapped, and when there is no OS

#prereq(
  ([chapter 74, Operations that do not split], [operations that do not split]),
  ([chapter 77, A program's map of memory], [when there is no OS]),
)

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

#organizer[
  The last chapter of Part XII. We see the two ways of running several things
#idx("coroutine")  overlapped (stackless coroutines and the job system), why
  allocators and pointer provenance become a problem in a program running along
  several strands, and what shape this library takes in a place with no operating
  system at all. The last section is *when it is better not to use it*.
]

#chapter-questions()

== Stackless coroutines — overlapping without threads

If a function can do some work, stop, and later start again *from that place*, much
becomes easy. Because the code can be written in order instead of a state machine
being written by hand.

#demo("examples-en/ch88/coro.c")

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

There are five doors, and their order is itself the contract.

```c
proven_job_sys_t *sys;
proven_err_t e = proven_job_system_init(alloc, 4, 256, &sys);  /* ① 4 workers, queue 256 */
bool ok = proven_job_submit(sys, routine, arg);                /* ② submit (false if full) */
bool did = proven_job_execute_one(sys);                        /* ③ this thread handles one too */
proven_job_system_close(sys);                                  /* ④ take no more */
proven_job_system_destroy(sys);                                /* ⑤ join the workers and clean up */
```

That it is an *opaque type* stands out — the inside of `proven_job_sys_t` is not in the
header and it is handled only by pointer (the opaque type of chapter 55 in the flesh).
Platform resources such as threads and locks are inside, and their layout is not to be
exposed to user code.

That ④ and ⑤ are divided is a contract too. *Closing* is "no more submissions are
taken", and *destroying* is "wait for the workers to finish and then clean up". Between
them, the header requires the producer threads to be joined.

`proven_job_execute_one` is a little special. It lets *the submitting side handle one
item of work itself* — so when submission fails because the queue is full, instead of
merely waiting one can handle one and try again (a simple form of work stealing).

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
threads use one arena together, the simple action seen in chapter 83 of "cutting from
the front" becomes a race at once. The prescription is mostly *one per thread* — and
then no synchronisation is needed at all.

Pointer *provenance*, whose name only was seen in chapter 14, becomes practical here
too. Since even at the same address it matters which allocation a pointer came from,
giving a block obtained from one allocator back to another is a contract violation
(as in chapter 83's counterexample) even if the address happens to match. The library's
name came from here.

== When there is no OS — freestanding

This is the constraint that most shaped this library. It is the demand to run in the
very environment chapter 56 gave as the reason the standard library is thin — a place
with no operating system, no heap and no files.

#idx("freestanding")What changes in a freestanding build is this.

- `platform/` is not included at all. Files, time and OS randomness disappear.
- Allocation is done with an arena over a static buffer (chapter 83). There is no
  `malloc`.
- Strings are handled with `_borrow` over stack and static buffers (chapter 84).
- The panic handler is registered by you — there may be no console to print to, so it
  becomes a matter of lighting an LED, kicking a watchdog or rebooting.
- Real-number formatting uses large-integer arithmetic, so it can be taken out whole
  if it is not needed.

Why the disciplines of the earlier chapters — "take the allocator as an argument", "a
view is borrowed", "no hidden globals" — were so persistent shows itself here. Those
disciplines were not a taste but *the minimum condition for running in this
environment*.

=== The actual build procedure

Left in words alone it stays vague, so here is the order.

+ *Take `platform/` out of the compilation list.* Leave only `src/proven/*.c`. Files,
  time, OS randomness, streams, mmap and the job system go out with it.
+ *Define `PROVEN_FREESTANDING`* (`-DPROVEN_FREESTANDING=1`). `proven_heap_allocator()`
  then returns *an unusable value* (all zeros), and if it is used by mistake
  `proven_alloc_is_valid` says false.
+ *Take the backing memory statically.* Put one array where the linker script knows it
  and lay an arena over it (chapter 83).
+ *Register a panic handler.* There being no console, one of an LED, a watchdog or a
  reboot (chapter 81's example).
+ *Take out what is not needed.* If real-number formatting is not used,
  `-DPROVEN_FMT_NO_FLOAT` strips the large-integer arithmetic code out whole.

```c
/* the skeleton of a freestanding program */
static proven_byte_t g_pool[8 * 1024];      /* the memory budget is settled here */

int main(void)
{
    proven_set_panic_handler(board_panic);
    proven_arena_t arena = proven_arena_create(
        (proven_mem_mut_t){ .ptr = g_pool, .size = sizeof g_pool });
    proven_allocator_t alloc = proven_arena_as_allocator(&arena);

    for (;;) {
        proven_arena_reset(&arena);          /* taken back each turn */
        handle_one_event(alloc);
    }
}
```

These twenty lines contain all of this part's discipline — *the allocator is an
argument*, *lifetime is per arena*, *`main` does not return* (chapter 50), and *the
memory budget is written as a number in the source*.

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

== Where this library stands — what it is, and what it is not

Before closing the part its place has to be made clear. The design seen so far —
taking an allocator as a parameter, returning failure as a value, views that
carry a length, refusing rather than truncating — was not invented by proven.
Zig's allocator parameter, Rust's `Result` and slices, recent C++'s `span` and
`expected`, and the in-house C conventions of many companies have all moved in
the same direction. There is considerable common ground about it.

*proven, however, is not that common ground itself.* It is *one attempt* to
implement that direction in C23 — neither a standard nor an industry component.
The distinction matters for a simple reason: the direction has been verified in
many places, this implementation has not yet. What to take from this part is the
direction rather than the code, and if you do take the code, take it knowing what
follows.

=== What has been verified so far

What this book has put in print is exactly this much.

#dtable(
  columns: 2,
  [*confirmed*], [*not confirmed*],
  [this book's 94 examples build and run under GCC 14 and Clang 22], [behaviour under other compilers (MSVC, older GCC)],
  [they all pass on x86-64 Linux], [continuous verification on other systems and architectures],
  [the contracts of the API the examples use match the documentation], [coverage of the whole API],
  [the platform layer has two branches, POSIX and Win32], [continuous automated verification of the Win32 branch],
)

That is, what this book vouches for is that *the code printed here runs in this
environment* — not that the whole library is verified in every environment. The
book claims no more than that.

=== Stability — what may still change

proven is not at 1.0. The edition this book uses is a snapshot of the
`v26.07.23b` line, and that means:

- *The API may change.* Names and signatures are still being tidied. There is no
  guarantee that this book's examples compile unchanged against the next edition.
- *No ABI is promised.* Struct layouts may change, so mixing pre-compiled
  binaries is not advisable at this stage; building from source alongside your
  own code is safer.
- *Pin the edition.* If you use it in earnest, vendor a specific snapshot into
  your repository (as this book does under `vendor/proven`) and move it
  deliberately.

=== What is not there yet

Set down honestly, the following are absent or thin in this edition.

- *Performance and size comparisons.* No benchmarks against the standard library
  or other C libraries are published. That is why this book makes no performance
  claim — a performance claim without data is advertising.
- *Outside users.* There is little record of use in projects beyond the author's.
- *A wide platform matrix.* The freestanding branch is designed for, but the list
  of continuously verified targets is narrow.
- *A long compatibility history.* The trust an old library earns — "it has been
  carried across many editions already" — accumulates only with time.

#qa[
  Then what is this part to be read for?
][
  Two things. One is *how to read a design* — the eye that asks, of whatever
  library you meet, "how does it report failure, who supplies the memory, where
  is the length, what happens at the boundary?" That eye remains whether or not
  you use proven.

  The other is *grounds for a choice*. If, looking at the table above, you judge
  that it is too early for your project, that too is the result of reading this
  part properly. It means the same as the preface saying that deciding you do not
  need proven is a fine outcome too.
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
  chapter 56. The right attitude is neither "do not use it" nor "use it
  unconditionally" but *knowing the contract and choosing*.

#qa[
  Then what remains of what was learned in this part if proven is not used?
][
  Almost all of it. Strings that carry their length, writes that refuse rather than
  truncate, errors that come back as values, allocation visible in the signature, the
  distinction of owning and borrowing, size calculation done with checked arithmetic,
  comparators that keep a total order, hashes that assume adversarial input — these are
  not a library but *design principles*, and they can be applied by hand to any C code.
  Open chapter 79's table again and the right-hand column is entirely such items. What
  this part really wanted to sell is not the code but that column.
]

#recap[
  A summary of the whole of Part XII — problems and answers.

  #dtable(
  columns: 3,
    [*chapter 79's problem*], [*the answer*], [*chapter*],
    [strings that do not know the size], [views (ptr+size), refusing rather than truncating], [chapters 82 and 84],
    [unconfirmed failure], [errors as values, `[[nodiscard]]`], [chapter 81],
    [format mismatch], [`{}` and `PROVEN_ARG` (`_Generic`)], [chapter 85],
    [unclear ownership], [the allocator parameter, owning versus borrowing], [chapter 83],
    [unchecked callbacks], [documented contracts, introsort, keyed hashes], [chapter 86],
    [the hidden type of bytes], [`proven_byte_t`], [chapter 82],
    [environments with no OS], [separating the platform layer, static arenas], [chapter 88],
)
]

With this, the promise this book made in chapter 1 has been kept — showing C's problems
first, and showing one answer to them through to the end. The two remaining chapters
are the story beyond these pages. In the next chapter we look round the terrain of
practice (build tools, version control, real projects), and in the last we gather up the
road this book has travelled.
