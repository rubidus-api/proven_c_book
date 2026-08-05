#import "../../book/lib.typ": *

= Repetition — loops and invariants

#organizer[
  We take a program's real power — repetition — into our hands. The three
  siblings of the loop (`while`, `for`, `do-while`), new operators (`++`, `+=`),
#idx("Duff's device")  and the frame of thought for reading and writing loops
  correctly (the invariant). And we finally meet the reunion booked in
  chapter 12 — Duff's device.
]

#deepqa[
  Chapter 4 said "even simple steps, taken billions of times a second, can build
  anything however complicated." But our programs up to chapter 30 flow from top
  to bottom once and end — where do the billions of steps come from?
][
  From a device that *executes the same statements again*. Going *back* to a
  block while a condition is true — the loop. If branching was the device that
  splits flow, the loop is the device that winds it up, and the moment we have
  both, C becomes a language that can write down everything computable
  (theoretically so as well — all chapter 4's model of computation demands is
  "sequence, branch, repetition").
]

== The three siblings of the loop

*`while`* — the most primitive form. "While the condition is true, repeat the
block":

```c
while (condition) {
    statements to repeat
}
```

It checks the condition *first*, so if it is false from the start the loop never
turns.

*`for`* — the form that gathers the loop's housekeeping (start, condition,
update) onto one line. Seen in a demonstration — the sum from 1 to 100:

#demo("examples-en/ch31/sum.c")

Read `for (int i = 1; i <= 100; i += 1)` in three slots — *start* (`int i = 1`:
make the loop variable), *condition* (`i <= 100`: checked before each turn), and
*update* (`i += 1`: executed at the end of each turn). The new operator `+=` is
an abbreviation of chapter 23's assignment, the same as `i = i + 1`, and the
still shorter `i++` (the increment operator) does the same job — idiomatically
you will see the `for (...; i++)` shape most often. (`++` has a prefix form and a
postfix form, and subtle circumstances inside expressions — treated together with
the next chapter's story of sequence points.)

*`do-while`* — the form that executes the block *first* and checks the condition
afterwards (`do { ... } while (condition);`). It is used for "work that must
happen at least once" (showing a menu first and then asking whether to repeat),
and is the least frequently seen of the three.

#mathbox[
  The loop invariant — the eye that proves a repetition
][
  There is a tool for reading a loop "correctly" — the *invariant*: a proposition
  that always holds at the same point of every turn. For the demonstration's
  loop, on every entry to the body

  $ "sum" = 1 + 2 + dots.c + (i - 1) $

  holds (on the first turn, the empty sum = 0). The body does `sum += i` and the
  equation's right side grows to $dots.c + i$; the update raises $i$ and it takes
  the same shape again — the invariant is *maintained*. At the moment the loop
  ends $i = 101$, so substituting into the invariant gives sum = 1 + ... + 100.
  That is, [invariant + termination condition = a proof of the loop's
  correctness]. Even if you do not write it out every time, the habit of asking
  "what fact does not change in this loop?" becomes the eye that catches most
  loop bugs — turning one too few or one too many, the off-by-one. It is also the
  first exercise in the perspective of code as contract (chapter 45).
]

#qa[
  What if the condition never becomes false?
][
  An infinite loop — the program turns there forever. Usually it is the result of
  a bug in which the update was forgotten, but a deliberate infinite loop
  (`while (true)`) is a respectable idiom too — it is the skeleton of programs for
  which "not ending is normal", such as servers, and you leave it from inside on
  a condition with `break` (leave the loop — the same word as switch's break).
  What is dangerous is not the infinite loop itself but the *unintended* one.
]

== The reunion — Duff's device

Time to meet the legend booked in chapter 12. We now know both switch's
fall-through (chapter 30) and the loop (this chapter).

In 1983 Tom Duff of Lucasfilm was struggling with a loop copying data to a device
that was too slow. To reduce the housekeeping cost of each turn (checking the
condition, updating) he processed eight at a time (unrolling) — and solved the
handling of the remainder, when the count was not a multiple of eight, in a way
nobody had imagined:

```c
switch (count % 8) {
case 0: do { *to = *from++;
case 7:      *to = *from++;
case 6:      *to = *from++;
case 5:      *to = *from++;
case 4:      *to = *from++;
case 3:      *to = *from++;
case 2:      *to = *from++;
case 1:      *to = *from++;
        } while ((count -= 8) > 0);
}
```

(The `*to` and `*from++` parts are chapter 33's pointer syntax, so for now look
only at the outward shape — the point is the structure.) How to read it: the
switch jumps *into the middle of a loop*. Remember chapter 30's fact that a
`case` is only a label — a label stamped *inside* the body of a `do-while` is not
a grammatical violation. The first entry jumps to the point that processes just
the remainder, and thereafter the do-while turns the whole eight-line body.
Remainder handling and unrolling became one body.

Duff himself left the remark that on discovering it he felt "a mixture of pride
and revulsion", and the code stands as a monument, on the boundary of legality
and grotesquerie, to the flexibility (or perhaps the looseness) of C's grammar.
And today's lesson is exactly as chapter 12 foretold — *this acrobatics is no
longer a human's job.* A modern compiler unrolls an ordinarily written loop by
itself (chapter 13's editor). Admire Duff's device as a masterpiece in the
museum, and write our own loops plainly and readably like the demonstration's
`for` — that is modern C.

With repetition the tools of flow are complete — sequence (chapter 19), branch
(chapter 30), repetition (chapter 31). The next chapter closes Part VI by digging
into the *meaning* of that device the function — how values cross over, what
exactly a side effect is, and the formal answer to the seed planted in
chapter 20 (the order of evaluation).
