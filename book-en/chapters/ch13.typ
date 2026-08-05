#import "../../book/lib.typ": *

= Compiler optimisation - the abstract machine

#organizer[
  You will learn that a compiler does not translate source code "as written" —
  that as long as the meaning is kept it rearranges, deletes and rewrites at
  will. You will meet the basis of that freedom, the notion of the observable
#idx("abstract machine")  result, and the abstract machine, a virtual machine
  living in the standard document. We also see how C99 and C11 refined this
  contract.
]

#deepqa[
  At the end of chapter 12 the compiler was called "a layer holding the right to
  rework anything as long as the meaning is kept." But chapter 5 called C "an
  honest nickname for the machine" — was the C code I write not in direct
  correspondence with machine instructions?
][
  In those days it largely was; today it is not. Today's compiler is not a
  literal translator but an *editor* — it rewrites whole sentences as long as
  the meaning survives. How and why early C's sense of "direct correspondence"
  collapsed is this chapter, and that collapse is the final piece that made C an
  abstract language.
]

== The editor's workshop — what a compiler does

A few real examples of editing give the feel. Compilers routinely do things
like this.

*They calculate in advance.* If the source says `seconds = 24 * 60 * 60`, no
multiply instruction is generated; 86400 is simply written in. There is no
reason to multiply at run time.

*They delete dead work.* A value computed and then used by nobody, a fork that
cannot be reached — gone entirely. Code present in the source may not exist in
the executable.

*They change the order.* If two calculations are unrelated, they are rearranged
so the pipeline (chapter 12) fills well. The order of lines in the source stops
being a promise about the order of execution.

*They remove trips to memory.* Instead of reading and writing one variable to
memory on every turn of a loop, they keep it in a register (chapter 11) and put
it down once at the end. The source may say a thousand memory accesses where
there are really two.

When these edits pile on one another, the machine instructions in the executable
and the source code become separate documents that do not correspond sentence by
#idx("compiler optimisation")sentence. That is why, when you look at an
optimised program in a debugger, you meet notices like "this line was optimised
out."

#qa[
  What is the basis for rewriting at will? How far can it be rewritten and still
  count as "keeping the meaning"?
][
  Drawing exactly that boundary is the standard's job, and the boundary's name is
  the *observable result*. Roughly: as long as what the program exchanges with
  the outside world (what it printed, what input it read, specially protected
  accesses) is as promised, everything about how that result is produced is the
  compiler's discretion. How many times an intermediate calculation happens,
  whether a variable really exists in memory, in what order the work is done — as
  long as it is not observed from outside, it does not matter. Look the same on
  the outside and the inside is free — that is the sentence of the contract.
]

== The abstract machine — the virtual machine inside the contract

Then who sets the standard for "looking the same"? It cannot be a real machine —
there are thousands of kinds, each different. So the standard defined a *virtual
machine* inside the document. A machine existing only on paper, which pins down
that the meaning of a C program is "this is how it executes on this virtual
machine" — the *abstract machine*.

The relationship of three layers now comes into focus.

- The *programmer* writes code against the abstract machine. The meaning of the
  code is determined by execution on the abstract machine.
- The *compiler*, keeping only the observable results of that meaning, freely
  invents the fastest code for the real machine.
- The *real machine*, however many layers of cache and stages of pipeline
  (chapters 11 and 12), does not expose that vortex on the outside.

This is the answer to chapter 5's remark that C's reputation as "an honest
nickname for the machine" is only half true today. What C now serves as a
nickname for is not the real machine but the *abstract machine*. The
correspondence with the real machine is a translation the compiler invents as it
goes, guaranteed only in appearance.

== Visible edits — when a value is read and when it is written

The editor's favourite place to work is *trips to memory*. As chapter 11 taught,
memory is slow and registers are immediate, so the compiler holds values in
registers where it can. Two scenes give the feel.

*Scene 1 — repeated reads become one.* Consider a loop like this:

```c
for (int i = 0; i < n; i += 1) {
    total += table[i] * scale;   /* scale does not change in the loop */
}
```

As written, `scale` must be read from memory every turn, but the compiler judges
that "no code inside this loop changes scale" and *reads it once before the loop
and keeps it in a register* (loop-invariant code motion). A thousand reads become
one. `total` likewise lives in a register and is put down in memory once, after
the loop.

*Scene 2 — deferring writes and changing order.* For code assigning to two
unrelated variables, the compiler may change the order or defer a write so the
pipeline (chapter 12) fills well. The line numbers of the source are not a
promise about the moment of execution.

Both edits keep the *observable result* the same, so both are inside the
contract. The trouble arises from the fact that "observable" is the standard's
criterion, not the programmer's expectation.

#realcase[
  The loop that never turns — believing memory literally
][
  Consider a classic accident of embedded programming. Suppose a hardware status
  register appears at some address in memory (that world of chapter 6), and the
  code "wait until ready" is written like this:

  ```c
  int flag = *status;                     /* the device changes this value */
  while (flag == 0) { flag = *status; }   /* wait until ready */
  ```

  The programmer's picture is "read memory again every turn." But in the
  compiler's eye there is no code inside this loop that changes `*status` — so
  the edit of scene 1 applies: read the value *once* into a register and look
  only at that register thereafter. However much the device changes memory, the
  program spins forever seeing the old value. An infinite loop.

  The reason lies in the contract. The standard's abstract machine does not know
  that "something outside this program may change memory" — if that is the case,
#idx("volatile")  *the programmer must say so*, and the notation for it is
#idx("multicore")  `volatile` ("really perform this access every time"). The
  same pattern recurs in the multicore world: poll a value another core changes
  through an ordinary variable and it freezes for the same reason (there the
  right answer is not volatile but C11's atomic types — see below). The lesson is
  one: *the compiler does not execute what is written in the source. It produces
  only the results the contract guarantees.*
]

== Refining the contract — C99, C11

If C89 was the first edition of the contract (chapter 12), the standards since
have been revisions honing its clauses. The more aggressively machines and
compilers evolved, the more sharply "how far may it be rewritten?" had to be
drawn. We note only the two large revisions.

*C99* (1999) shaped the language towards giving the editor more grounds for
discretion — most notably, the rule that names of different types may be
#idx("strict aliasing")*assumed* not to refer to the same storage (strict
aliasing) was written into the text. That assumption is what lets the editor
treat two things as unrelated and rearrange and cache them. The price is a duty
on the programmer's side — code that breaks the assumption (reading the same
storage through the eyes of a different type) becomes a violation of the
contract (revisited with the safe methods in chapter 43).

*C11* (2011) is the response to the multicore era of chapter 12 — a *memory
model* pinning down as a clause "when a write by one core becomes visible to
another", and the atomic types (`_Atomic`) used for that communication, entered
the standard. Until then, C on multiple cores was, quite literally, a partnership
without a contract, leaning on practice outside the standard.

== A false signal — the ghost summoned by breaking strict aliasing

Strict aliasing is the most delicate clause in this chapter, so let us look at
one pattern. To restate the rule — *the compiler may assume that two pointers of
different types do not refer to the same storage* (the char family is the
exception, chapter 35).

The problem is that this assumption reads as a *signal* in the programmer's
code. Suppose you used this old technique to look at the bits of a floating-point
number:

```c
float f = 1.0f;
uint32_t bits = *(uint32_t *)&f;   /* contract violation: a float through uint32's eyes */
```

The programmer's intent is "let us read the same four bytes through different
eyes", but the signal the compiler received is the opposite — *"this `float*` and
that `uint32_t*` refer to different storage (you may assume so)."* The editor can
then confidently keep the write to f in a register and move the read of bits
earlier — and the result is that "the old bits, from before the write" are read:
a ghost that cannot be explained from the source alone. Worse, this ghost appears
*only when optimisation is on*, and comes and goes with the compiler version
(the classic identity of "it works in the debug build but is wrong only in
release").

There are two correct methods — copy the bytes with `memcpy` (modern compilers
recognise this copy and optimise it away for free), or use the union we learn in
the next part (chapter 43 does this demonstration properly). The point is not the
technique but the perspective: *a contract violation is not "slightly dangerous
code" but the act of telling the compiler something untrue as if it were true.*
That is why the result is not "slightly odd" but "anything at all" — the
standard's name for this state is *undefined behaviour* (UB), and chapter 46 of
this book is its head-on treatment.

#realcase[
  The vanished eraser — security code deleted by optimisation
][
  "Look the same outside and the inside is free" has caused real accidents.
  Security programs have a practice of overwriting a password's memory with zeros
  after use — so that no secret is left lying in memory. But in the editor's eye
  this erasure is *a write nobody reads again*: dead work. It contributes nothing
  to the observable result — so it may be deleted. Several compilers really did
  silently delete this erasure, and software shipped with passwords left in
  memory became the subject of security advisories. The side that ignored the
  contract was not the compiler but the person who had not read it — afterwards,
  standards and platforms provided devices for saying "do not delete this write"
  (`memset_s` and the like). It is an event that shows how heavy a single clause
  about observable results is in practice.
]

#qa[
  If optimisation is this dangerous, may we not simply switch it off?
][
  Debug builds during development really do lower the optimisation to keep the
  correspondence between source and execution alive. But switching it off for
  release is not the answer — much of modern software's speed comes from the
  editor's skill, and there is no reason to give up several-fold performance. The
  right road is one: know the contract and write inside it. The criterion must be
  "correct on the abstract machine", not "it ran on my computer" — what happens to
  code outside the contract is faced head on in chapter 46 (undefined behaviour).
]

The materials are now all assembled. Memory is a ladder (chapter 11), execution
is overlap and guesswork (chapter 12), and above that vortex stands an editor
who guarantees only appearances (chapter 13). Put the three together and one
conclusion follows — what a C programmer deals with is not the real machine. The
next chapter completes this part's thesis and closes with a panorama of which
machine's language C finally is — including a contrast between CPU and GPU.
