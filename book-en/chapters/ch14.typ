#import "../../book/lib.typ": *

= And so C is an abstract language

#prereq(
  ([chapter 13, Compiler optimisation], [what a compiler keeps is only the observable behaviour]),
  ([chapter 4, A simple model of the machine], [the simple machine model]),
)

#deepqa[
  Chapter 13 said "what a C programmer deals with is not the real machine."
  Then was it wasted labour to look so closely at the real machine — clocks,
  lockers, words — all through chapters 4 and 5?
][
  The opposite. Those pictures are the *skeleton* of the abstract machine. The
  standard's abstract machine was not invented out of thin air: it distilled the
  common skeleton of real machines — memory as a sequence of bytes, addresses,
  sequential execution — and wrote it into the contract. You must know the real
  machine to understand why the abstract machine has that shape, and you must
  know the abstract machine not to be shaken by the real machine's vortex
  (chapters 11–13). The two pictures are not rivals but a pair.
]

#organizer[
  The conclusion of Part II. The scattered pieces — memory that became a
  ladder, execution that overlaps, the compiler that acts as an editor — are
  gathered into one thesis: C is not a nickname for machine code but the
  language of an abstract machine, and must therefore be approached abstractly.
  We see why the increasingly strict rules about pointers (provenance) follow
  from that, and close the part by contrasting CPU and GPU to ask which
  machine's language C really is.
]

#chapter-questions()

== Completing the thesis — putting three pieces together

Reduced to one line each, the pieces this part has stacked up:

- Chapter 11 — memory is not one lump but a *ladder* from register to warehouse,
  and the source code does not say which rung your variable is on right now.
- Chapter 12 — execution is not one step at a time but *overlap and guesswork*,
  and the real progress of instructions differs from the order of lines.
- Chapter 13 — above that stands an *editor* that rewrites the code wholesale,
  keeping only the apparent results.

The thought that each line of source is "what the machine will do, as written"
therefore collapses on three counts. What remains solid is one thing: the
*contract*. What does my code mean on the virtual machine (the abstract
machine) defined by that contract, the standard? That alone is the real meaning
of a C program, and everything else — cache hits, pipelines, reordering,
register allocation — is the inner business of realising that meaning quickly.

*And so C is an abstract language, and must be approached abstractly.* Not the
habit of guessing "how will the machine run this code", but the habit of asking
"what does this code mean on the contract" — that habit is what this book tries
to cultivate in every chapter that follows.

#misconception[
  "C is a language for manipulating hardware directly"
][
  The most famous sentence about C, mixing half history with half
  misunderstanding. Historically it was close to fact (chapters 4 and 5), and
  even today C is the language that *takes you nearest* to the hardware. But
  between today's C code and the hardware stand an editor (the compiler) and a
  contract (the standard), and code written with a naive "direct manipulation"
  instinct — a shortcut outside the contract, say — breaks in the face of the
  editor's discretion (chapter 13's vanished eraser, and chapter 48). The
  accurate sentence is this: C is not a language that manipulates hardware
  directly but *a language that lets you meet the hardware through the contract
  called the abstract machine*.
]

== The current direction — pointers acquire an origin

The representative trend showing where this perspective is heading is the
refinement of the rules about pointers.

Chapter 5 said "an address is a number too." In the naive picture a pointer is
just a number — the same number is the same place, and if the number is right
you should be able to touch that place however you obtained it. But in the
editor's world this picture cannot hold. The compiler's optimisation stands on
tracing "where did this pointer *come from*" — the assumption that a pointer
derived from this variable cannot touch that one (chapter 13's strict aliasing
is one branch of it) is what makes reordering and caching possible.

#idx("provenance")So the modern standards discussion is refining pointers from
"a number" into "a number + a *tag of origin*." The same bit pattern may be
usable in different places depending on where it came from — this notion of
origin is called *provenance*, and the C committee has been working to write it
into the text. The direction is consistent: the rules for pointers grow more
explicit and stricter, that is, the clauses of the contract grow more precise.
At an introductory stage there is no need to know the details of the clauses —
plant only the instinct that "a pointer is not just a number; it has a
lineage." The practical rules come in chapter 36, and the consequences of
violating the contract in chapter 48.

#qa[
  Does chapter 6's tagged pointer — the trick of tucking a tag into the free
  bits of an address — collide with this rule?
][
  That is exactly where it can collide. Manipulating a pointer like a number to
  insert and strip a tag was self-evident under "number = pointer", but in a
  world of origin tags it becomes work that must follow *a procedure that does
  not lose the lineage* (there are legitimate routes that go through integer
  conversion). The lower-level the technique, the more the contract has to be
  read closely — that is modern C.
]

== A closing panorama — which machine's language is C?

Before closing the part, let us widen the view once. What we have been calling
"the machine" is in fact not one kind.

Out of the same transistors you can build opposite machines. One *finishes a
single job as fast as possible* — deep pipelines, large caches, elaborate
branch prediction (chapters 11 and 12) are all devices for reducing *latency*.
That is the CPU we have been drawing. The other *does the same job to a
mountain of data at once* — it strips away prediction and deep pipelines and
plants thousands of simple calculators in their place. The machine of
*throughput*: the GPU. It was born to spray the same calculation over millions
of pixels on a screen, and that constitution turned out to match matrix
arithmetic — the heart of artificial intelligence — making it the protagonist
of the age.

As a restaurant metaphor, the CPU is one or two master chefs who turn out any
order immediately, and the GPU is a canteen line producing ten thousand plates
of the same dish at once. Asking which is better does not parse — the orders
are different.

C's relation to this becomes the last sentence of the chapter. Even the GPU's
world has programming languages (CUDA, OpenCL) built as dialects of C — chapter
1's story that C is the lingua franca of the systems world repeats here too.
But to be precise, *C's abstract machine is an image of the CPU*. Sequential
execution, a single memory, addresses — the contract's virtual machine is
modelled on the CPU's skeleton, and the GPU's different constitution is handled
through dialects and extensions of that contract. To learn C is to learn, along
with its language, the most fundamental image among computing's many machines:
the sequential machine that has come down to us from von Neumann.

== Closing Part II

The groundwork is done. We started from the bit (chapter 4), walked the locker
corridor (chapters 5 and 6), learned how to hold integers and decimals and
letters and flows (chapters 7–10), and saw how machines grew complicated and what
contract stands on top of them (chapters 11–14). All of it is capital that the
deeper exchanges will keep summoning.

Now we really can begin. In the next part we finally write the first program —
hello world is printed on the page, and the journey of reading that one piece
is the whole of the rest of the book.
