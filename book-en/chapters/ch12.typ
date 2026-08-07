#import "../../book/lib.typ": *

= The machinery of speed — the birth of the standard

#prereq(
  ([chapter 4, A simple model of the machine], [machines differ from one another]),
  ([chapter 11, Memory divides], [the layers of memory]),
)

#deepqa[
  The exchange in chapter 4 promised that "the number of steps (the clock) is
  not the whole story — doing several things in one step is the real weapon of a
  modern CPU." How is more than one thing per step possible? Does one
  instruction not have to finish before the next?
][
  Discard "finish before you start." Split the handling of one instruction into
  several stages, and carry several instructions at *different* stages at the
  same time — while one instruction is being calculated, fetch the next and
  decode the one after. That is this chapter's first piece of machinery: the
  pipeline.
]

#organizer[
  If the cache reduced waiting, this time it is the machinery that overlaps
#idx("pipeline")  execution itself — pipelining, branch prediction, and the
  multicore turn forced by the limits of the clock. We also look at the strange
  accident that arrives with several cores (false sharing). Alongside, we see how
  C came to write its first contract (C89) in the same era of turmoil.
]

#chapter-questions()

== The pipeline — instructions on an assembly line

Think of a laundry. If washing takes 30 minutes, drying 30 and ironing 30, then
finishing one load before starting the next takes 270 minutes for three loads.
But put the second load in the washer the moment the first moves to the dryer,
and three machines handle *different loads at the same time*; once the flow
fills up, one load comes out every 30 minutes.

The CPU's handling of an instruction divides into stages in exactly the same
way — fetch, decode, execute, write back. Making an assembly line of those
stages is the *pipeline*. Individual instructions do not get faster — what
increases is *the number of instructions completed per unit of time.* Modern
CPU pipelines run to more than ten stages, and go further by having several
lines so that multiple instructions complete per beat. Chapter 4's "one step per
beat" collapses like this — the apparent order is kept, but inside things
overlap.

Let us record one trick from the same era. To save the management cost of each
turn of a loop (counting, judging the fork), there is a technique of handling
*several items per turn* — loop unrolling. In 1983 a programmer named Tom Duff
pushed this technique to its limit by grotesquely overlapping two pieces of C
syntax, and the code became a legend under the name *Duff's device*. The code
itself cannot be shown yet — this book has not introduced those constructs. A
reunion is booked (chapter 31). One sentence suffices for now: in those days
people squeezed the machine's beats with acrobatics like this, and today the
acrobatics are the compiler's job (chapter 13).

== Branch prediction — guessing the fork in advance

The pipeline has a sore spot: *the fork*. Meet an instruction like "if the
result is zero, jump over there" (a branch) and, until the result is known,
there is no telling which road's instructions to load onto the line. Halt the
line and wait, and the gain from overlapping evaporates.

So the CPU *guesses*. Like a café that prepares a regular's order in advance, it
learns from history that "this fork usually goes this way" and loads that road's
#idx("branch prediction")instructions onto the line ahead of time — *branch
prediction*. Right, and it was free; wrong, and it throws away everything
prepared, flushes the line and starts again. Because that penalty is as large as
the depth of the pipeline, modern CPUs invest serious circuitry in the predictor,
and hit rates in everyday code run well past 90%.

#realcase[
  The episode of the sorted array being faster
][
  There is a widely circulated measurement among programmers — the same code
  that sums "only the values above a threshold" in a large array runs several
  times faster if the array is *sorted beforehand*. The number of additions is
  the same, so why? The answer is branch prediction. In a shuffled array the
  "greater/smaller" fork is random every time, so the prediction is wrong about
  half the time and pays the line-flush penalty each time. In a sorted array the
  answer at the fork stays on one side for a long stretch and the predictions are
  almost all right. An invisible guessing circuit split the speed of the same
  code several-fold (we use this feel again in chapter 30 — "an `if` is not
  free").
]

== The limit of the clock, and multicore

Despite all this overlapping, a wall arrived in the mid-2000s. Raising the clock
further meant power and heat rising beyond what could be handled — a physical
limit. The decades-long "a faster clock next year" stopped.

The industry's answer was a change of direction — instead of making one worker
faster, *increase the number of workers.* CPUs began to contain two, four,
dozens of engines that independently execute instructions — *cores*. Even
today's phones commonly have about eight. The free lunch was over — the era in
which programs got faster by themselves ended, and the problem of *dividing*
work among many workers became the programmer's (outside this book's scope, but
worth knowing as terrain).

And once there were many workers, chapter 11's cache became the scene of an
unexpected accident.

#qa[
  If each core has its own cache (desk), what happens when two cores touch the
  same data?
][
  The hardware keeps the desks consistent behind the scenes — when one core
  changes a value, the copies on other cores' desks are invalidated, and so on.
  Correctness is preserved this way; the problem is that this tidying is *not
  free*, and that its unit is not one variable but chapter 11's *cache-line box*.
  A strange accident is born here.
]

#idx("false sharing")The accident is called *false sharing*. Two cores touch
*different* variables — they share nothing at all — but if the two variables
happen to sit side by side in the *same cache-line box*, then to the hardware's
eye two cores are fighting over the same box. Every write by one invalidates the
box on the other's desk, the box ping-pongs between the two desks, and both
cores slow down by tens of times. Look at the code and nothing is shared, so the
cause is invisible — a representative modern accident that can only be diagnosed
by knowing chapter 11's fact that memory moves in boxes.

== The same era: C wrote a contract — pre-standard C and C89

While machines were shaking like this, the world of C had turmoil of another
kind.

C had *no official standard* for a long time. K&R's 1978 book "The C Programming
Language" served as the *de facto* standard — not a specification of the
language, but one well-written book standing in for a statute book. As C spread
with Unix into universities and industry, every company built its own compiler,
and dialects grew in every corner the book had not pinned down. The C of that
era looks loose today — a function declaration did not state the types of its
arguments (there were no prototypes), and a name without a stated type quietly
passed as int. Code that worked on one compiler failing on another was routine,
and porting was an adventure every time.

Unable to bear it, the industry formed a standards committee (ANSI X3J11) in
1983, and after six years of heated argument *C89* — the first official C
standard — appeared in 1989. It brought in function *prototypes* (declarations
that state argument types too, chapter 24) so compilers could catch mistakes at
call sites, tightened the loose corners, and above all pinned down *in a
document*, for the first time, "what the programmer is promised and how far the
implementation is free." It is within a few years of chapter 8's IEEE 754
(1985) — an "age of contracts", in which numbers in hardware and syntax in
languages alike, arbitrary from vendor to vendor, began to be governed by
written agreements.

=== The places the contract does not cover — grey areas

Once there is a contract, one question follows at once: *what about the things
the contract does not mention?*

The standard speaks in three ways — *it promises* (this is how it goes
everywhere), *the implementation decides* (it varies, but must be documented), and
*it says nothing at all*. The proper names for the three and the exact
distinctions are chapter 49's business.

And practice holds one more place that has no name at all — *not guaranteed by
the standard's words, yet working on every major implementation, and so widely
used that it became the practice.* This book calls that place, together with the
three above, a *grey area*.

#misconception[
  "Grey area" is not a term of the C standard
][
  *Let this be nailed down: "grey area" is a name this book adopted for
  convenience.* Search the standard as long as you like and the phrase is not
  there. The standard uses three words, and each has an exact definition —
  *implementation-defined*, *unspecified*, and *undefined behavior*. Chapter 49
  treats those three properly.

  Why have another name, then? Because the standard's three leave one place in
  practice with nothing to call it — the place where *the standard promised
  nothing and yet every implementation does effectively the same thing.* This book
  wants one word that covers that too.

  A note on the phrase itself. In ordinary English *a grey area* means "a case the
  rules do not clearly settle" — which is close, but slightly wider than what is
  meant here. This book uses it in one narrow sense: *the standard promises
  nothing, and yet every implementation behaves the same way.* Where the standard
  is merely ambiguous, or where implementations genuinely differ, the exact word is
  used instead.

  So *be careful using the phrase outside this book.* Talking to the standard, or
  to a compiler developer, you must translate it into the exact word — is it
  undefined, unspecified, implementation-defined, or something the standard does
  not address at all? That is also why the text distinguishes "outside the
  contract" (undefined behaviour) from "a grey area".
]

#dtable(
  columns: 2,
  [*What this book calls a grey area*], [*What it is not*],
  [Not guaranteed by the standard's words alone], [Not forbidden either — nothing says "do not"],
  [Yet it really works on the major implementations], [Not luck — there is usually a reason],
  [So widely used that it became the practice], [Still not a contract],
)

Why do such places arise? The standard writes the *lowest common denominator* —
it promises only as much as would hold even on very odd machines. Real
implementations usually give more. And once large codebases build on that extra
room, compilers cannot easily take it back — *break it and half the world stops
running.*

Here are a few grey areas this book will meet, listed in advance. For now the
names are enough.

#dtable(
  columns: 2,
  [*The practice*], [*Where it is treated*],
  [Pushing a pointer to zero with `memset` and calling it null], [Chapters 6, 35, 43],
  [Walking a declared two-dimensional array as if it were flat], [Chapter 38],
  [Subtracting an `offsetof` to recover the enclosing struct (`container_of`)], [Chapter 44],
  [Writing `int rc = setjmp(env);` outside the four contexts the standard fixes], [Chapter 67],
  [Forcing a layout with `#pragma pack`], [Chapter 44],
  [Converting a function pointer to `void *` to print or pass it], [Chapter 54 — where POSIX requires it],
)

#qa[
  So may a grey area be used, or not?
][
  *The judgement is a human one, case by case.* This book does not decide it for
  you — an embedded project aiming at one machine and a library that may be
  compiled anywhere have different answers. Four questions serve as criteria.

  #dtable(
    columns: 2,
    [*The question*], [*What pushes towards avoiding the grey area*],
    [Where will this be compiled?], [In places you do not know],
    [How long will this code live?], [A long time — compilers change meanwhile],
    [What breaks if it breaks?], [A quietly wrong value (crashing would be kinder)],
    [What does the alternative cost?], [The alternative is a few lines],
  )

  *Whichever you choose, one thing must be done — prepare for both outcomes.* A
  grey area is "what works now", not "what will keep working".

  - *If you use it* — make the place *visible*. How much to wrap around it follows
    the ladder below: scale it to the stakes.
  - *If you avoid it* — you usually pay in longer and slower code. Paying that
    price knowingly is not the same as avoiding it blindly.

  #dtable(
    columns: 3,
    [*Rung*], [*What you put around it*], [*Where this much is enough*],
    [1], [One comment — what it leans on, and why it is safe here], [A tool you alone use; code soon thrown away],
    [2], [Documentation — "this module assumes X", where the team reads], [Code several people edit],
    [3], [Let the build say so — `static_assert`, feature-test macros, a compile error], [Where a broken premise goes quietly wrong],
    [4], [A test — one that checks the premise still holds], [Code that will live long, or run on several platforms],
    [5], [Isolation — confine it to one file or function and prepare an alternative], [A port is planned, or a break would bring down something large],
  )

  What decides how far up you climb is the four questions above — *where it is
  compiled, how long it lives, what breaks if it breaks, and what the alternative
  costs.* Adding a test to a script you alone run is too much; leaving one comment
  in a library that ships to several platforms is too little. *Weighing that is
  part of the job.*

  The worst is the third attitude — *using a grey area without knowing it is
  one.* Then you do not even know there is something to prepare for. That is why
  this book takes care to say "outside the contract" and "a grey area" as two
  different things.
]

=== When a standard outside C promises instead

Among the grey areas, some have a different character: *C says nothing, and
another standard makes the promise in its place.*

#dtable(
  columns: 3,
  [*What C leaves open*], [*What promises instead*], [*Example*],
  [The format and rounding of floating point], [IEEE 754 (= ISO/IEC 60559)], [That a `float` is 32 bits and how it rounds (chapters 8, 47)],
  [Conversion between function pointers and `void *`], [POSIX (ISO/IEC 9945)], [`dlsym` returning a function's address as `void *` (chapter 54)],
  [File names, paths, processes], [POSIX], [`open`, `fork`, the path separator (chapter 87)],
  [The grammar of locale names], [POSIX], [The spelling `ko_KR.UTF-8` (chapter 63)],
  [The character set], [Unicode (ISO/IEC 10646)], [When `__STDC_ISO_10646__` is defined (chapter 65)],
)

Do not read this as "C did not settle it, so anything goes". *Another contract is
lying in that place*, and it too was written with care.

#qa[
  How should one think about such places, then?
][
  This book's position, in three sentences.

  *First, where the standard left something open, there is usually a good reason.*
  It is not a gap left by laziness — wanting only promises that hold on any
  machine at all, the committee had to leave out what is true on particular
  machines only. Floating point was not nailed to IEEE 754, and function pointers
  were not equated with data pointers, because machines existed on which that
  would not have worked (chapter 54's platform note is the list).

  *Second, the standard that fills the gap has its own reasons, and the two can
  collide.* POSIX required the conversion between function pointers and `void *`
  so that dynamic libraries could work at all — without it `dlsym` cannot exist. C's
  caution and POSIX's practicality gave different answers in the same place, and
  neither is wrong. What differs is *which contract you are working under.*

  *Third, the programmer's job is therefore not to choose but to make the choice
  plain.* However fiddly it is, you must know which contract you are standing on,
  and once you have chosen you must make that choice *visible*.

  #dtable(
    columns: 2,
    [*How to make it visible*], [*What to write*],
    [Documentation and comments], [One line saying *which contract this code leans on* — "this file assumes POSIX"],
    [Build settings and warnings], [Make a broken premise fail the build or raise a warning (`static_assert`, feature-test macros)],
    [Tests], [One test that checks the premise still holds (a round trip, a size)],
  )

  With those three, *a change of circumstances changes things safely.* When the
  compiler is upgraded, when the code is ported, when a new architecture arrives —
  what has to be re-checked is already written in the code. Without them, you learn
  that a premise broke *after* the accident.

  ```c
  /* This file assumes POSIX: conversion between function pointers and void * (dlsym). */
  static_assert(sizeof(void (*)(void)) == sizeof(void *),
                "a platform where function and data pointers differ in size");
  ```
]

Why does this notion of a contract matter more and more? Because machines, as
this chapter has shown, increasingly overlap and guess and divide, and a
programmer cannot know every detail of that vortex. Between the vortex and the
programmer stands *one more layer* — a layer that takes source code and
translates it into the machine's language, holding the right to rework anything
at all as long as the meaning is kept. The compiler. It is the protagonist of
the next chapter.
