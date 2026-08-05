#import "../../book/lib.typ": *

= The machinery of speed — the birth of the standard

#prereq(
  ([chapter 4, A simple model of the machine], [machines differ from one another]),
  ([chapter 11, Memory divides], [the layers of memory]),
)

#organizer[
  If the cache reduced waiting, this time it is the machinery that overlaps
#idx("pipeline")  execution itself — pipelining, branch prediction, and the
  multicore turn forced by the limits of the clock. We also look at the strange
  accident that arrives with several cores (false sharing). Alongside, we see how
  C came to write its first contract (C89) in the same era of turmoil.
]

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

Why does this notion of a contract matter more and more? Because machines, as
this chapter has shown, increasingly overlap and guess and divide, and a
programmer cannot know every detail of that vortex. Between the vortex and the
programmer stands *one more layer* — a layer that takes source code and
translates it into the machine's language, holding the right to rework anything
at all as long as the meaning is kept. The compiler. It is the protagonist of
the next chapter.
