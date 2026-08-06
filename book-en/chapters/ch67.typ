#import "../../book/lib.typ": *

= Operations that do not split — `<stdatomic.h>`

#prereq(
  ([chapter 11, Memory divides], [the cache and the layers of memory]),
  ([chapter 41, Lifetime and storage duration], [lifetime and sharing]),
)

#deepqa[
  Chapter 12 said the CPU does several things in one beat and even executes out of
  order, and chapter 11 said each core carries its own cache. Then if two strands
  increase the same variable by 1 at once — is the result 2?
][
  It may not be. `x = x + 1` is not one step to the machine but *three* (read, add,
  write), and if two strands interleave these three steps they read the same value
  and write the same value — one increment vanishes whole. On top of that, caches
  differ per core, so the lag of "written but not visible to the other" is
  compounded. This chapter's first example actually counts that loss out.
]

#organizer[
#idx("atomic operations")  We see what happens when several strands touch the same
  memory at once, and learn the tool C11 brought into that place — atomic types and
  operations. Why a data race is *outside the contract* rather than "slow", why
#idx("data race")  `volatile` is not the answer, and when to touch and when to
  leave alone the difficult handle called the memory order.
]

#chapter-questions()

== The lost update — confirmed with the eyes

#demo("examples-en/ch67/atomic.c")

Four strands turned the same loop 200,000 times each. The expected value is
800,000, but the unprotected `long` did not even get near it. The vanished share
differs on every run — and this non-determinism is precisely the character of this
class of bug. *A loss that does not reproduce.*

The atomic variable's side is exactly 800,000 every time. Because
`atomic_fetch_add` performs "read-add-write" as *one lump that cannot be split*.
The name atom means just that — that which is not split further.

#misconception[
  "A race condition is a probabilistic performance problem where the value is
  occasionally off"
][
  Wrong in two layers. First, it is not that the value goes off but that *the
  contract breaks*. The standard from C11 onward settles it thus — if two strands
  touch the same memory at once, at least one of them writing, and it is neither
  atomic nor ordered, that is a *data race* and the whole program is undefined
  behaviour (chapter 49). It means there is no guarantee that it ends at the level
  of "one value being wrong".

  Second, the compiler optimises on this premise. Assuming there is no race, it
  puts the variable in a register, and then however much the other side changes the
  value this loop *sees the old value forever*. This really is the common identity
  of the infinite loop that "works in a debug build and hangs in release"
  (chapter 17's release-only bug is replayed here).
]

== Why `volatile` is not the answer

In code that has long used C one often sees the practice of exchanging signals
between strands with `volatile int flag;`. It is a wrong practice now. What
`volatile` promises is one thing only — *the compiler will not remove or merge
these accesses*. As will be seen in chapter 49, its purpose was hardware registers
(places whose value changes from outside).

There are two things `volatile` does not promise. *Atomicity* — the `x++` of a
`volatile long x;` is still three steps and splits. And *ordering and visibility* —
it does not prevent the CPU from changing the order of writes or from being late to
show them to another core.

#dtable(
  columns: 4,
  [], [*not split*], [*visibility between cores*], [*blocks reordering*],
  [`volatile`], [no], [no], [no (against other accesses)],
  [`_Atomic`], [yes], [yes], [yes (as much as the chosen order)],
  [a mutex], [yes (the whole region)], [yes], [yes],
)

Summed up: *`volatile` for hardware registers, atomic types or mutexes for sharing
between strands.* Java's and C\#'s `volatile` share only the name and differ in
meaning (there it really does guarantee visibility), so bringing the habits of
those languages into C is a common passage to accidents.

== Atomic types and operations

Using them is simple. Attach `_Atomic` before the type, or use the names the header
gives (`atomic_int`, `atomic_long`, `atomic_bool`, `atomic_size_t` …).

```c
#include <stdatomic.h>
atomic_int  counter = 0;      /* = _Atomic int */
_Atomic long total;
```

The operations are called as functions (or macros of those names). Using the
ordinary operators (`++`, `+=`) also behaves atomically, but *the fact of being
atomic is not visible in the code* and so is easy for a reader to miss, which is
why the explicit functions are recommended.

#dtable(
  columns: 3,
  [*operation*], [*what it does*], [*where it is used*],
  [`atomic_load`], [read], [seeing the current value],
  [`atomic_store`], [write], [setting a value],
  [`atomic_fetch_add`, `_sub`], [add and return *the previous value*], [counters and statistics],
  [`atomic_fetch_or`, `_and`, `_xor`], [bit manipulation], [sets of flags],
  [`atomic_exchange`], [swap and return the previous value], [replacing in place],
  [`atomic_compare_exchange_strong`], [change only if equal to the expected value (CAS)], [lock-free data structures],
  [`atomic_compare_exchange_weak`], [the same, but it may fail in vain], [inside a loop],
  [`atomic_flag_test_and_set`], [the most primitive test-and-set], [spinlocks],
)

That `fetch_add` returns *the previous value* is a place often confused. To obtain
"which number this is", use the return value; to know "how much it is now", it is
the return value plus the increment, or a separate `atomic_load` — and the latter
may change again in the meantime.

#antipattern[
  Believing that several atomic operations make their bundle atomic too
][
  ```c
  if (atomic_load(&count) < LIMIT)      /* ① read and */
      atomic_fetch_add(&count, 1);      /* ② add — somebody cuts in between */
  ```
  If another strand raises the value between ① and ②, the limit is exceeded.
  *Atomicity is a property of one operation, not of a region.* To protect a region,
  bind it with a CAS loop or use a mutex.
  ```c
  int cur = atomic_load(&count);
  do {
      if (cur >= LIMIT) break;                 /* limit check and update as one lump */
  } while (!atomic_compare_exchange_weak(&count, &cur, cur + 1));
  ```
  That on failure *the current value comes back held in `cur`* is the heart of this
  idiom. So there is no need to read again inside the loop.
]

#qa[
  What differs between `compare_exchange`'s strong and weak? Why is there a
  separate edition that "fails in vain"?
][
  Some CPUs (the ARM family and others) implement CAS as a pair of instructions,
  "reserve and later write conditionally". If the reservation is broken in between
  by an interrupt or by cache circumstances, it comes back as a failure even though
  the value equalled the expectation — that is a *spurious failure*. `weak` exposes
  this failure as it is and in exchange is faster, while `strong` retries
  internally to guarantee "failure only when the value differed" and in exchange is
  a little slower.

  The rule is simple. *`weak` inside a loop, `strong` when trying only once without
  a loop.* Since the loop will turn again anyway, a spurious failure is harmless
  and only the gain remains.
]

== Memory order — not touching it is the default

If no order is specified, as in `atomic_fetch_add(&x, 1)`, the strongest order,
*sequential consistency* (`memory_order_seq_cst`), is used. It means every strand
sees the order of atomic operations as one consistent story, and it is the model
easiest for a human to reason about. In exchange it is the most expensive.

C provides six orders. We learn their faces from a table — *most programs need only
the default.*

#dtable(
  columns: 3,
  [*order*], [*guarantee*], [*where it is used*],
  [`seq_cst`], [one order globally], [the default. if in doubt, this],
  [`acquire`], [later accesses cannot rise above it], [taking a lock, reads on the consumer side],
  [`release`], [earlier accesses cannot sink below it], [releasing a lock, writes on the producer side],
  [`acq_rel`], [both, in a read-modify-write operation], [state transitions with CAS],
  [`relaxed`], [atomicity only. no ordering guarantee], [pure counters and statistics],
  [`consume`], [effectively abandoned (implementations raise it to acquire)], [not used],
)

The two most common practical uses are these. First, *the producer-consumer flag*:
fill the data and then raise the flag with `release`, and the consumer sees the
flag with `acquire` and then reads the data. This pair guarantees "if the flag is
visible the data is visible too." Second, *a pure statistics counter*: only the
final sum need be right and there is no need to order it against other data, so
`relaxed` is exactly the right tool.

#antipattern[
  Switching to `relaxed` for performance, just to see
][
  ```c
  atomic_store_explicit(&ready, 1, memory_order_relaxed);   /* the flag */
  ```
  `relaxed` guarantees *only the atomicity of this variable*. There is no guarantee
  that the data filled in beforehand is visible to the other side, so the consumer
  can see the flag raised and yet read *the data from before it was filled*. Such
  code mostly runs fine on x86 and then appears as an unreproducible bug on an ARM
  device — because the reordering each piece of hardware permits differs.

  The rule: *when a flag and data form a pair, `release`/`acquire`.* Until you
  understand that pair, leave the default as it is. The time lost far exceeds the
  nanoseconds saved here.
]

== The phrase "lock-free"

That is what the example's last line asked with `atomic_is_lock_free`. If an atomic
type is handled by a single CPU instruction it is *lock-free*, and if not the
library uses a hidden lock behind the scenes. On today's mainstream machines
integers of pointer size or smaller are mostly lock-free. Wrap a large struct in
`_Atomic`, on the other hand, and — the syntax passes but — a hidden lock attaches
and performance can become unexpectedly bad.

#misconception[
  "Atomic operations are always faster than a mutex"
][
  Mostly right when contention is low, but it reverses when contention is heavy. If
  several cores fight over the same cache line, that line keeps travelling between
  the cores (chapter 11's false sharing is replayed here). A CAS loop turns again
  on every failure, and when contention is heavy this retrying is waste entire — a
  mutex puts the failing strand to sleep while spinning burns a core.

  And *writing lock-free data structures yourself* is a task of another order of
  difficulty. The ABA problem (a value going from A to B and back to A, fooling the
  CAS), when memory may be reclaimed, progress guarantees — these are topics for a
  paper each. The right answer in the field is usually this: *atomic types for
  counters and flags, a verified library or a mutex for data structures.*
]

== Where to use it and where not to

#dtable(
  columns: 3,
  [*situation*], [*recommended tool*], [*reason*],
  [statistics counters], [`atomic_fetch_add` (`relaxed`)], [no ordering is needed],
  [a shutdown-request flag], [`atomic_bool` + `release`/`acquire`], [it pairs with data],
  [initialising exactly once], [`call_once` (`<threads.h>`) or CAS], [do not write double-checked locking by hand],
  [invariants over several values], [a mutex], [atomicity is per single variable],
  [sharing a large struct], [a mutex], [an `_Atomic` struct means a hidden lock],
  [sharing with a signal handler], [`sig_atomic_t` or a lock-free atomic type], [chapter 65's restrictions],
  [hardware registers], [`volatile`], [it is not sharing between strands],
)

#realcase[
  Why C11 brought in a memory model
][
  In the standard before C11 there was *no concept at all of there being several
  strands.* Threads were the business of a library (POSIX threads and the like),
  and the language defined optimisation on the premise that "a program flows in one
  stream". In that gap questions piled up which nobody could answer exactly — must
  the compiler assume another strand sees this write, is this reordering legal, is
  this mutex-less code wrong.

  Around 2004 Java tidied up its memory model first, and C++11 and C11 continued
  that current by introducing *a memory model at the level of the language*. What
  entered then was the definition of a data race, atomic types, and the six memory
  orders. That we can today say in one line "a data race is undefined behaviour" is
  thanks to that tidying — before it there was not even a language in which to write
  that sentence.
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [data race], [not slowness but *outside the contract*. optimisation changes the code],
    [`volatile`], [not a tool for sharing between strands. it is for hardware],
    [the default order], [`seq_cst`. leaving it as it is is mostly the right answer],
    [`fetch_add`], [it returns *the previous value*],
    [two operations], [bundling them is not atomic — a CAS loop or a mutex],
    [`weak`/`strong`], [`weak` if inside a loop],
    [lock-free], [mostly yes for small integers. a hidden lock for large structs],
    [data structures], [do not write them yourself],
  )
]

We have seen operations that do not split. The next chapter is a tool in the
opposite direction — C23's checked arithmetic, which asks according to the contract
whether an operation *overflowed its vessel*.
