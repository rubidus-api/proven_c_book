#import "../../book/lib.typ": *

= Deciding — if and switch

#organizer[
  The program meets a fork. `if`, which takes a different road according to a
  condition, and `switch`, which spreads several branches at once — we learn both
  branching devices. This is the chapter in which chapter 4's model of
  computation ("if the result is zero, jump over there") finally appears with the
  face of C syntax.
]

#deepqa[
  Chapter 12 foreshadowed the instinct that "an `if` is not free" — a mispredicted
  branch pays the penalty of flushing the pipeline. Should a programmer then
  avoid branches?
][
  No — branches are the skeleton of a program, and the predictor is right well
  over 90% of the time in everyday code. The use of that instinct is not "do not
  use branches" but *knowing that the cost of a branch is not constant* — a
  regular fork is cheap, a random one expensive (the sorted-array episode). It is
  knowledge to be taken out only in the very small amount of code where
  performance matters; the branching learned today may be used freely.
]

== if — when the condition is true

The syntax is as it reads:

```c
if (condition) {
    statements when the condition is true
} else {
    statements when it is false   /* the else part may be omitted */
}
```

In the condition's place goes an expression that becomes a bool (chapter 29),
and each branch takes a block (chapter 19). When there are several branches you
build a ladder with `else if` — the demonstration is that shape. Notice too that
chapter 25's input practice (fgets + sscanf) appears again.

#demo("examples-en/ch30/grade.c", stdin: true)

The ladder is checked *in order from the top*, and only the first branch that
becomes true is executed — that 87 stops at "우" and does not fall further down
is that guarantee. So the order of the ladder is the logic itself: narrow
conditions above, broad conditions below, is the standard.

#qa[
  I hear the braces may be omitted when a branch has only one statement — may
  they be?
][
  Grammatically they may — and this book recommends not doing it. A brace-less
  branch takes "only the next single statement" into the branch, and adding a
  line later while forgetting the missing braces is a classic path to accident.
  It really happened in Apple's SSL library, where this pattern pushed
  verification code outside the branch and the security check was skipped
  entirely (the "goto fail" incident, 2014) — the day it was expensively proved
  that indentation is visible only to human eyes and not to the compiler
  (chapter 19). Always braces on a branch — the rule every example in this book
  follows.
]

== switch — a board that forks by value

When the fork is not "which range" but "which *value*", there is a dedicated
device — `switch`.

#demo("examples-en/ch30/season.c", stdin: true)

How to read it — when `switch (month)` steps onto the board holding one value,
execution *jumps* to the `case` label matching that value. And here is switch's
most important property: a `case` is not a fence but a *label*, so once jumped to
it *keeps flowing downward*, past the next `case`. This flow is called
*fall-through*, and where you want to stop you write `break` (leave the board)
explicitly.

Fall-through cuts both ways. It is ideal for binding several values to one answer
as in the demonstration (December, January and February into a single winter),
but *forget* the `break` and it becomes the classic accident of flowing into an
unintended branch — so compiler warnings and C23's explicit notation
(`[[fallthrough]]` — documentation saying "this flow is intended") guard this
place. `default` is the branch for when no label matches — always keeping it, as
*a net that catches unexpected values* ("there is no such month"), is the
practice.

#realcase[
  A curious example the standard gives — a declaration nobody reaches
][
  Push the property that a `case` label is not a fence to its extreme and strange code
  becomes possible. The following is one the C standard gives directly as an example in
  its `switch` clause.

  ```c
  switch (expr) {
      int i = 4;          /* ← nobody comes here */
      f(i);               /* ← nor here */
  case 0:
      i = 17;
      /* falls through */
  default:
      printf("%d\n", i);
  }
  ```

  On entering a `switch`, execution *jumps straight to the matching label*. So the two
  lines at the head of the block are executed for no value at all. And yet the variable
  `i` *exists* — once the block has been entered, that block's local variables get their
  places (chapter 39's automatic storage duration).

  The result is curious. If `expr` is 0 it jumps to `case 0:`, `i = 17` runs and 17 is
  printed; but *if it is not 0* it jumps straight to `default:` and `i` is read
  *never having been initialised* — the reading of an indeterminate value learned in
  chapter 46. And that with the initialising line sitting there in plain sight.

  This example is in the standard not to say "write it this way" but to pin down *the
  consequence of a `case` being merely a label*. The rules in practice are two — *do not
  put a declaration at the head of a `switch` block*, and *if a variable must be declared
  inside a `case`, make a block with braces* (otherwise the compiler may refuse it as an
  initialisation jumped over by a case label).
]

#qa[
  An `if` ladder can do it all, so why is `switch` needed at all?
][
  The difference between them is not expressive power but *conveying intent*.
  switch pins down in syntax the intent of "matching one value against several
  constants", so the structure is visible both to the reader and to the compiler
  — the compiler may use that structure to produce fast code such as a jump table
  (a shape chapter 13's editor likes), and the warning features can point out "a
  case missing from an enumeration". Fork on a single value, use switch; on
  ranges or compound conditions, an if ladder — fitting the tool to the intent.
  And in the next chapter we finally meet the legendary code that abused (?) this
  fall-through property to its limit.
]

We have learned the fork — but a program's real power comes not from forking but
from *repeating*. The repetition of which chapter 4 said "even simple steps,
#idx("loop")taken billions of times", is taken into our hands in the next
chapter.
