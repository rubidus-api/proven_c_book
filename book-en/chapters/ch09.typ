#import "../../book/lib.typ": *

= Memory divides — registers, caches, a ladder of layers

#organizer[
  A chapter that begins with a confession — how bold a simplification the
#idx("register")  three-part picture of chapter 2 really was, and a repair of
  that picture. The registers that were inside the CPU all along, the large
  memory outside, and the multi-layered cache that pushed into the widening
#idx("cache")  speed gap between them — you will learn that memory is not one
  thing but a ladder.
]

#deepqa[
  Chapter 2 said "the CPU fetches the next thing to do from memory and does it."
  So does the calculation itself happen on memory — does the sum of slot 100 and
  slot 104 go straight into slot 108?
][
  No — and that question opens this chapter. A CPU does not calculate directly
  on memory. Calculation happens in a separate space *inside* the CPU. That
  space was not drawn in chapter 2's picture at all. Now it is time to be
  honest.
]

== A confession — that picture was too simple

The CPU-memory-clock model of chapter 2 is excellent as a first step of
learning, but for today's computers it is *excessively simplified*. To what
degree? If that picture were accurate, today's CPU would spend most of its time
*waiting blankly*. Clocks got hundreds of thousands of times faster over half a
century while memory did not keep up. And yet real computers do not sit blank.
What machinery stepped into the gap is the story of this chapter and the next.
First, let us fill in what was missing from the picture from the start.

== Registers — the CPU's own hand, there all along

Inside the CPU are small storage places called *registers*. They were there
from the beginning — chapter 2's picture merely omitted them. They number only
a few dozen and each is one word in size (chapter 3). In exchange their speed
is *immediate*: read and written within the beat of the clock. Call them coins
held in the hand — nearer than the pocket (memory), and taking no time to pull
out.

The important point is this. *All calculation happens on registers.* To add two
numbers you first bring them from memory into registers (load), add register to
register, and send the result back to memory (store). The real route of a
calculation that looks like "slot 100 + slot 104" is always
[memory → register → calculate → memory].

#qa[
  If they are that important, why are there only a few dozen? Why not make
  plenty?
][
  Because fast and many are a trade-off. Registers are immediate because they
  sit right beside the CPU's calculation circuits, built in the most expensive
  way. Increase the number and the distance grows and selection time is added,
  so "immediate" collapses. The design therefore settled on "a little of
  something very fast" — and this trade-off is the principle that governs the
  whole chapter: *the faster the memory the smaller it is, and the larger the
  memory the slower.*
]

The surface of C carries a trace of this stratum. C has a keyword `register` —
an old-days request that "this variable be kept in a register if possible."
Today compilers place things far better than people do, so the request has
become decoration; but the question of who puts a variable in a register and
when it goes back to memory is very much alive, and becomes the core material
of chapter 11 (compiler optimisation).

== The widening gap — between register and memory

Stand the two kinds of memory side by side and the picture sharpens.

- *Registers*: a few dozen × one word. Immediate.
- *Memory (DRAM)*: tens of billions of slots. On today's CPUs the answer comes
  back after *hundreds of clock beats*.

It was not always so. When C was born (chapter 2) the strides of CPU and memory
roughly matched. Over the following decades the CPU's clock accelerated
headlong while memory evolved towards capacity and did not keep pace. The gap
widened year by year to hundreds of times — the relation of a coin in the hand
to a warehouse an hour's round trip away. If calculation is immediate but
fetching the material takes hundreds of beats, the machine spends longer waiting
than working.

#misconception[
  "Memory access costs the same wherever you read"
][
  A natural misconception planted by the simple picture, and the first illusion
  to break when trying to understand a program's speed. In reality, touching
  something near what you just touched and touching a distant place for the
  first time differ in cost by tens to hundreds of times — why, is exactly the
  cache of the next section. This is why two programs doing the same work can
  differ several-fold purely in *what order they touch memory*.
]

== The cache — a middle layer wedged into the gap

Engineering's answer to a hundredfold gap was the *cache*. The idea is exactly
the trick of someone working in a library. If the round trip to the stacks
(memory) is slow — *leave the book you just fetched on your desk for a while.*
When you want it again and it is on the desk (a hit) it is immediate; when it is
not (a miss) you make the trip to the stacks just then.

This works because of programs' habits. Programs do not touch memory at random —
they *touch again soon what they just touched*
#idx("locality") (temporal locality) and *go on to touch the neighbours of what
they touched* (spatial locality). Think of a loop touching the same variable
repeatedly, and of an array being scanned from the front. Thanks to these
habits, a single small desk removes most of the trips to the stacks.

There is one more trick that exploits spatial locality. The cache does *not*
fetch things from memory one slot at a time. It carries the requested slot along
with its neighbours — today usually 64 bytes, sixty-four lockers as one box — in
#idx("cache line")a single trip. This unit of carriage is the *cache line*. It
is why scanning an array in order costs one trip to the stacks at the first slot
and the next sixty-three are free, and conversely why a program that touches far
apart is slow, carrying a new box every time. This "box" returns in the next
chapter as the protagonist of an unexpected accident (false sharing).

== The cache divides — a ladder of layers

The same trade-off as with registers applies to caches — to be fast is to be
small, to be large is to be slow. So the cache too divided into *layers*: the
small, fastest L1 right beside the CPU, the intermediate L2 behind it, the large
and relatively slow L3 shared by several cores — and beyond that, memory. The
rough feel in numbers is as follows (it differs by machine).

#dtable(
  columns: 4,
  [*layer*], [*order of size*], [*rough wait*], [*metaphor*],
  [register], [a few dozen], [0 (immediate)], [coins in the hand],
  [L1 cache], [tens of KiB], [a few beats], [on the desk],
  [L2 cache], [hundreds of KiB–MiB], [a dozen-odd beats], [the bookshelf],
  [L3 cache], [a few–tens of MiB], [tens of beats], [stacks on this floor],
  [memory (DRAM)], [tens of GiB], [hundreds of beats], [the warehouse],
)

There is one way to read it — *each step down is a large slowdown.* Memory is
not "one lump" but this whole ladder, and a program's speed is determined in
large part by "on which rung of the ladder the thing you need is found."

#realcase[
  Same work, five times the time — scanning a two-dimensional table
][
  Think of the simple job of summing a large two-dimensional table (say
  thousands by thousands of numbers). Scan along rows — in the order they lie in
  memory — and each cache-line box is used thriftily. Scan along columns —
  jumping far every time — and boxes are carried and discarded over and over.
  The work done (the number of additions) is exactly the same, and yet measured
  the difference runs from several-fold to tens of times. The gap produced by the
  order of two lines of code cannot be explained without knowing that memory is a
  ladder — and you will see such a measurement as an example in this book
  (chapter 30).
]

#qa[
  Can a programmer not control the cache directly — an instruction like "keep
  this in L1"?
][
  In general, no. The cache is run automatically by hardware, and C has no
  standard syntax for touching it directly. What the programmer does is not
  control but *cooperation* — keeping data together with its neighbours and
  touching it in order. The code of someone who knows memory is a ladder
  naturally comes out that way. And that alone produces the several-fold
  difference we just saw.
]

The first half of the repair is done — memory is not one thing but a ladder from
register to warehouse. But that is only half. The CPU was not content with
reducing waiting; it also evolved towards *overlapping execution itself* —
stacking instructions like an assembly line, guessing at forks in advance, and
finally increasing the number of workers (cores). That story, and how C came to
write its first contract (C89) in the middle of that turmoil, is the next
chapter.
