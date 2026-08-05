#import "../../book/lib.typ": *

= Assignment and side effects

#prereq(
  ([chapter 23, Declaring a variable], [putting a value under a name]),
  ([chapter 32, The meaning of a function], [side effects and evaluation order]),
  ([chapter 28, Implicit conversions], [when values of different types meet]),
)

#deepqa[
  Chapter 32 said that calls with side effects should be given their own
  statements. But assignment is itself a side effect — `x = 1;` changes the object
  x. Why, then, have we used assignment so freely all along?
][
  Because each statement had *one* side effect. `x = 1;` changes one object, and
  there is a sequence point at the end of the statement, so by the time the next
  statement begins it is over. The danger starts when *two or more side effects*
  are packed into one expression — as in `a[i] = i++`. This chapter draws that
  line exactly.
]

#organizer[
#idx("assignment")  The last chapter of this part is the operator used most often
  and understood most shallowly: *assignment*. That it is an expression, that the
  left side is evaluated too, that compound assignment evaluates the left side
  only once, and where the contract ends. The vocabulary built here carries into
  chapter 45's gathering of the operators and chapter 48's undefined behaviour.
]

#chapter-questions()

== Assignment is an expression, not a statement

`x = 1` looks like a command, but it is an *expression* — it yields a value. That
value is "the assigned value, converted to the type of the left side", and so it
can be placed inside another expression.

#demo("examples-en/ch33/assign.c")

The first two lines of the output confirm it. In `x = (y = 7) + 1` the expression
`(y = 7)` has the value 7, and adding 1 makes x 8. `p = q = r = 5` makes all three
5 by the same principle — assignment is *right-associative*, so it groups as
`p = (q = (r = 5))` and the value of the inner assignment is handed outward.

One thing should be nailed down. *The result of an assignment is not an lvalue.*
`(a = b) = c` does not compile. This differs from C++, and it is enough to
remember that "assignment yields a value, but not a place".

#misconception[
  "`if (x = 0)` tests whether x is 0"
][
  This is the oldest typo accident in C. `x = 0` is an *expression* whose value is
  0, so the `if` always sees false. Far from being tested, x is *changed* to 0.
  Comparison is `==`.

  There are three defences. First, turn compiler warnings on — gcc's
  `-Wparentheses` points at an assignment in a condition (if it was intended,
  write `if ((x = f()))` with an extra pair of parentheses to say "on purpose").
  Second, there is the old practice of putting the constant on the left
  (`if (0 == x)`, the so-called Yoda condition), but it reads badly and this book
  does not recommend it. Third, and most reliable, is *the habit of not assigning
  in a condition*.
]

== Assignment does two things

Split what `E1 = E2` does and it comes to this.

+ *It computes a value* — `E2` is evaluated and converted to the type of `E1`.
+ *It changes an object* — that value is written into the place `E1` denotes.
  This is the *side effect*.

Keeping the two apart is the key to this chapter. The *value* of the expression
can be used at once, but *when the side effect actually happens* is another
question. The standard says only that the side effect must be complete by the
next sequence point; when within that span it happens is not settled.

#qa[
  Why is `x = x + 1` safe, then? Does it not read x and change x as well?
][
  Because the reading and the changing are one each. Written exactly, the rule is:
  *within one sequence point, modifying the same object twice, or modifying it
  and reading its value for a purpose other than determining the new value, is
  outside the contract.*

  In `x = x + 1`, x is read *in order to determine the new value*, which is
  allowed. In `a[i] = i++`, by contrast, the `i` on the left is read not to
  determine i's new value but *to determine where to write* — and that is outside
  the contract.
]

== The left side is evaluated too

This is what beginners miss most often. The left of `=` is not a "value" but *a
computation that decides where to write*, and that computation runs.

The second block of the example confirms it. Running
`a[where()] = what();` calls *both* `where()` and `what()`. The left computes "how
to find that place", the right computes "what to write".

Which of the two is computed first? *The standard does not settle it
(unspecified).* A compiler may compute the right first or the left first. If both
computations have side effects, the result can differ with that order — the trap
of the next section.

#platform("compilers really do differ")[
  The same code being evaluated in different orders by gcc and clang does happen.
  Change the optimisation level and it can differ within one compiler. This is a
  prime example of a place where "on my machine it comes out this way" is not
  evidence — *unspecified* means both roads are correct, not that one was chosen
  and will be kept.
]

== Compound assignment — the left side is evaluated once

`E1 op= E2` is *not the same as* `E1 = E1 op E2`. The standard says the only
difference is that `E1` is evaluated once — and that one line of difference
matters in practice.

Look at the third block of the example. In `a[where()] += 1;`, `where()` is called
*once*. Spelled out as `a[where()] = a[where()] + 1;` it would have been called
twice, reading one slot and writing another.

#dtable(
  columns: 3,
  [*shape*], [*left-side evaluations*], [*note*],
  [`a[f()] = a[f()] + 1`], [2], [with a side effect in `f`, it reads one slot and writes another],
  [`a[f()] += 1`], [1], [the recommended shape],
  [`*p++ += 1`], [1], [still hard to read — better split up],
)

There are ten compound assignments — `+= -= *= /= %= <<= >>= &= ^= |=`. Do not
forget that the grey zones of each operation come along unchanged: `x /= 0` is
still outside the contract, and so is `x <<= 40` (chapter 27).

#qa[
  Are `x += 1`, `x++` and `++x` not the same thing in the end?
][
  The *effect on the object* is the same. What differs is the *value of the
  expression* — `++x` and `x += 1` yield the value after the change, `x++` the
  value before it. In a statement that discards the value (`x++;`) all three are
  identical, so in practice it is a matter of taste.

  One practical difference exists. `x += n` can add any value, and on a pointer it
  moves by elements (chapter 37). And a habit inherited from C++ — "use `++x` when
  the value is not used" — is widespread, though in C there is no performance
  difference.
]

== Where the contract ends

Now the famous expressions can be judged. The code below *carries no output* —
printing the result of an expression outside the contract would leave the false
knowledge "on this compiler it comes out like this" (chapter 48's principle).

```c
int i = 0, a[4] = {0};

i = i++;              /* outside the contract — i is modified twice */
a[i] = i++;           /* outside — modified, and read to decide the place */
i = ++i + i++;        /* outside — modified twice */
a[i++] = i;           /* outside — the same reason */
printf("%d %d", i++, i++);  /* outside — no sequence point between arguments */
```

These, by contrast, are fine.

```c
i = i + 1;            /* the read is to determine the new value */
a[i] = i;             /* one object changed, a[i]; i is only read */
i++, i++;             /* the comma *operator* has a sequence point */
x = (i++) && (i++);   /* so does && */
f(i++);               /* one argument cannot overlap */
```

#antipattern("gathering side effects into one expression")[
  ```c
  a[i] = ++i + i++;        /* outside the contract. Asking "what value" is the wrong question */
  ```
  Meeting such code, read it not as "what is the result" but as *"this code has no
  meaning"*. Outside the contract does not mean the value is strange; it means the
  compiler may generate anything at all and owes you no diagnostic (chapter 48).

  The safe shape splits the statements.
  ```c
  int t = i + 1;            /* whatever was intended, settle the value first */
  i = t + 1;                /* then change the object */
  a[i] = t;
  ```
]

#realcase("compilers know about this place")[
  gcc and clang catch the common cases with `-Wsequence-point` and
  `-Wunsequenced`. Obvious ones such as `i = i++;` are usually caught, but cases
  that pass through a function call or reach the same object through a pointer are
  missed — the problem is hard to decide statically. So tools are an aid; knowing
  the rule comes first.
]

== The conversion hidden in an assignment

An assignment puts the right-hand value in *converted to the left-hand type*.
Chapter 28's conversion rules apply here quietly, and quietly is the danger.

The fourth block of the example shows it. In `c = (char)321` the value is cut to
65, and the value of the assignment expression is that 65 — *not what was put in
but what went in*. That `(int)d` is 3 after `d = 3.9` is the same story
(truncation toward zero, chapter 27).

#dtable(
  columns: 3,
  [*assignment*], [*what happens*], [*verdict*],
  [`char c = 300;`], [narrowed in the way the implementation settles], [*implementation-defined* — the value does not fit],
  [`int n = 3.9;`], [truncated toward zero to 3], [fine, but confirm the intent],
  [`unsigned u = -1;`], [wraps to the maximum], [fine (unsigned is modular)],
  [`int n = 3e30;`], [a real that does not fit an `int` → *outside the contract*], [UB],
  [`float f = 0.1;`], [narrowed from double to single], [fine; precision is lost (chapter 46)],
)

Turning on `-Wconversion` makes the compiler point at such places. It is a noisy
option, hard to switch on across a whole project, but *keeping it on for new code*
is a good habit.

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [assignment is an expression], [it yields a value — but not an lvalue],
    [right-associative], [`a = b = c` is `a = (b = c)`],
    [the left side is evaluated], [in `a[f()] = g()`, `f` is certainly called],
    [order of the two sides], [*unspecified* — the compiler decides],
    [compound assignment], [evaluates the left side *once*],
    [where the contract ends], [modifying the same object twice within one sequence point, or modifying it and reading it for another purpose],
    [the hidden conversion], [the right side goes in converted to the left type],
  )
]

Part VI is over — we forged values (chapters 26–28), governed flow
(chapters 29–31), and gained the meaning of functions and of assignment
(chapters 32–33). The next part is this book's second mountain, *memory*: back to
chapter 5's locker corridor, this time walking it in C's syntax.
