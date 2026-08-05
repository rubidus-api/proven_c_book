#import "../../book/lib.typ": *

= The meaning of a function — copying values and side effects

#prereq(
  ([chapter 24, Declaring and defining functions], [declaring and defining a function]),
  ([chapter 20, Expressions], [how a value is handed over]),
)

#deepqa[
  Chapter 24 said a parameter is "a variable declaration that receives the
  material." Then if that parameter is *changed* inside the function — does the
  variable passed in as material change too?
][
  It does not — and this is the single rule that fixes the meaning of a C
  function: *values cross over by being copied.* A parameter is a separate
  variable that received a copy of the material's value, and nothing done inside
  the function touches the original. A demonstration is quicker than words — here
  it is.
]

#organizer[
  Part VI's finish. *How* values cross over in a function call (copying),
#idx("side effect")  the formal rules of side effects and evaluation order
  (collecting chapter 20's seed), recursion as a new kind of repetition, and the
  conditional operator `?:`. The seed planted at the end — the perspective of the
  contract — blooms in Part IX.
]

#chapter-questions()

== Copying values — the original is safe

#demo("examples-en/ch32/copy.c")

Inside `try_change` the `x` was changed to 999, but the `n` outside is still 1.
That is the exact meaning of the call `try_change(n)` — n's *value* 1 was copied
and became the first value of a *different variable* called x, and thereafter the
two are strangers. Add this copying rule to chapter 24's partition of scope and a
function becomes a complete isolation room: material comes in as a value, the
result goes out as `return`'s value, and there is no other channel.

#misconception[
  "You pass a variable to a function"
][
  Natural in everyday speech, but false in C's semantics — and the moment you
  imagine it that way, the demonstration above becomes a riddle. What crosses
  over is not the variable but *the variable's value* (a copy). A natural
  objection arises: "then is it simply impossible for a function to change a
  variable outside?" — there is a proper method that makes it possible: copying
  and passing, as a value, *the variable's address* instead of the variable. A
  function that received an address goes to that address and touches the
  original. In chapter 25 the `&` of `sscanf(line, "%d", &n)` was doing exactly
  this — the copy-by-value rule intact (an address is a value too — chapter 5!),
  with the effect of "modifying the original." The syntax of that proper method
  is chapter 33's pointers.
]

== "Call by value" and "call by reference" — pinning the terms down

There is a subject to which every primer devotes considerable space: *call by
value* and *call by reference*. The conclusion first.

*C has no call by reference. Argument passing in C is always a copy of a value.*

Passing a pointer is no exception. What `f(&n)` does is not "pass n" but *copy
and pass the value that is n's address*. The parameter inside the function is a
separate variable holding that address, and assigning a different address to that
variable has no effect whatever on the caller's side. It is only when the address
is *followed* (dereferenced) and written through that the caller's object
changes. What changes is not the argument but *what the argument pointed at*.

#misconception[
  "Passing a pointer is call by reference"
][
  Many textbooks use this phrasing, and it is not accurate. *Call by reference* is
  the term used when a language has the feature of "binding the caller's variable
  itself to the parameter" — C++'s references (`void f(int &x)`) or Pascal's `var`
  parameters. In such a language, writing merely `x = 5` inside the function
  changes the caller's variable.

  C has no such syntax. With `void f(int *p)` you must *write the dereference*,
  `*p = 5`, and that star is precisely the evidence that "this is a value (an
  address) and I am now following it." So the accurate phrasing is *"passing a
  pointer by value to achieve the effect of a reference"*, not "call by
  reference." Even in English this distinction is a long-running argument, but in
  the standard's terms there is nothing to argue about — the C standard has no
  concept of a reference at all.
]

Get the terms right and there is genuinely less to be confused about. Three
frequent questions unravel with the same single rule.

- *Is an array not copied when passed?* — the array's name decays into the
  address of its first element (chapter 36) and *that address is copied* across.
  Not an exception to the rule but an application of it.
- *Is a struct copied whole when passed?* — yes (chapter 41). Which is why the
  practice of passing a pointer arose for large ones.
- *How do I let a function change the caller's pointer itself?* — pass the
  pointer's address (`int **`). An answer that follows naturally once you know the
  copy-by-value rule.

== Side effects and evaluation order — collecting the seed

Chapter 20 planted only the seed that "the order in time inside one statement may
differ." Now that all the material is gathered, let us set it out formally.

A *side effect* is anything an expression's evaluation does to change the state
of the world *besides* producing a value — and we already know three: output
(chapter 22), assignment and `+=`/`++` (chapters 20 and 31), and input
(chapter 25). A function call *carries* side effects if its body does such
things.

The *rules of evaluation order* summarise into two sentences. First, within one
statement (strictly, one full expression) the order of evaluating subexpressions
is mostly *unspecified* — if two function calls are in one expression, which is
called first is not in the contract (chapter 20's tiger and lion). Second, *at
the moment the end of a statement (the semicolon) is reached, all side effects
that statement caused are guaranteed complete* — this guarantee point is called,
#idx("sequence point")in the traditional term, a *sequence point*. There are a
few places besides the semicolon, chief among them the `&&` and `||` met in
chapter 29 (right side after the left's evaluation completes) and the boundary of
a function call (body entered after the materials are evaluated). Modern
standards (C11 onwards) refined the same concept more precisely as the relation
"sequenced before", but the classic term is still current.

From these rules come a practical rule and a trap. The rule is exactly
chapter 20's — *split statements when the order of side effects matters.* The
trap is new, because we have learned `++`. Code that *changes the same variable
twice in one expression, or changes it while separately reading it*:

```c
i = i++ + 1;      /* the same i changed in two places — outside the contract */
printf("%d %d\n", i, i++);   /* changed while separately read — outside the contract */
```

Such expressions go beyond unspecified (one of several orders happens) to
outright *undefined behaviour* — the standard guarantees nothing about the result
(chapter 46). There is no rule to memorise, only a pattern to remember: *change
one variable only once in one statement.* That chapter 17's UBSan and the
compiler warnings catch this pattern well is a reassuring backstop.

#qa[
  If copying is the rule, is a large struct copied whole every time it is passed
  — the cost would not be trivial.
][
  Yes, semantically it is a copy — and that cost is the reason for the practice of
  passing pointers (treated with structs in chapter 41). But there are two
  provisos. First, *copying a small value is effectively free* — as chapter 11
  taught, arguments usually cross in registers, so passing a few integers involves
  not even a trip to memory. Second, chapter 13's editor intervenes — if the
  observable result is the same it may omit the actual copy. *The meaning is a
  copy; the implementation is the compiler's discretion* — this distinction is the
  perspective to fix before worrying about performance.
]

== Recursion — a function that calls itself

If a function can call a function — can it call itself? It can.
#idx("recursion")That is *recursion*.

#demo("examples-en/ch32/fact.c")

`fact(5)` is `5 * fact(4)`, inside which is `4 * fact(3)`… reach the floor
(`n <= 1`) and it returns 1, and the layered calls come back in turn carrying
values. The reason this is possible is this chapter's rule itself — the parameter
`n` is *copied anew* for each call, so the five layers of fact are five isolation
rooms each with its own n (5, 4, 3, 2, 1). Without the isolation made by value
copying and scope, recursion does not stand.

Recursion is the natural expression for work that splits into "the same problem,
smaller" (searching tree structures is the representative — chapter 41 onwards),
and every recursion can also be written as a loop (recall the loop version of
factorial in chapter 31). Which is better is decided by the shape of the problem
— and recursion brings with it the physical problem of space for the "layered
calls", whose identity (the stack) is met in chapter 39.

== The last operator, and the seed of the contract

The last item on this part's allotment — the *conditional operator* `?:`.
`condition ? value1 : value2` is an *expression* that becomes value1 if the
condition is true and value2 if false. It writes "choose and hold" on one line,
as in `int big = a > b ? a : b;` — useful where an if, being a statement, cannot
go (initialisation and the like). Overused it becomes hard to read, so this book
takes "one layer at most" as its practice.

Three properties are enough to know.

*First, the branch not chosen is not evaluated.* The same guarantee as
chapter 29's short-circuiting — which is why `p != nullptr ? p->x : 0` is safe.

*Second, the result has a single settled type.* This is where beginners most
often stumble. If the two sides differ in type, the usual arithmetic conversions
(chapter 28) apply and give *one common type*, and that type is settled at
compile time regardless of the condition's value.

#demo("examples-en/ch32/ternary.c")

In `1 ? i : d` the condition is true so `i` (int) was chosen, and yet the
result's size is 8 bytes and its value is `7.0`. `int` met `double` and was
unified into `double`. Mix signs and the same rule appears more fiercely — the
last line, in which `-1` becomes `4294967295`, is the evidence, and gcc really
does point at this place like so.

```text
error: operand of ‘?:’ changes signedness from ‘int’ to ‘unsigned int’
       due to unsignedness of other operand [-Werror=sign-compare]
```

*Third, therefore it cannot be used to "return different types by condition."*
Code like `cond ? 3 : "three"` does not compile at all, and combinations that do
compile, like `cond ? 1 : 2.0`, do so *after both sides are converted to the same
type*. The principle that an expression's value has one static type (chapter 23)
is kept here too. If you need a choice that splits types, there are only two ways
— make a type holding "one of several types" with chapter 43's tagged union, or
choose the branch at compile time with chapter 50's `_Generic`.

There is one more convenience rule on the pointer side. If one side is a null
pointer constant the result takes the other side's pointer type — the example's
last line is that case.

== Operators at a glance — what we have met so far

Closing Part VI, let us gather the operators in one place. This book has taken
them out little by little where needed (starting from chapter 20's minimal set),
so this is the first sight of the whole map. Precedence and associativity are in
appendix A.

#dtable(
  columns: 3,
  [*kind*], [*operators*], [*one-line note*],
  [arithmetic], [`+ - * / %`], [`%` only on integers. division truncates toward zero (chapter 27)],
  [increment], [`++ --`], [prefix after changing, postfix the value before (chapter 31)],
  [comparison], [`< <= > >= == !=`], [result is `bool`. do not mix `==` with `=` (chapter 29)],
  [logical], [`&& || !`], [carry the short-circuit guarantee (chapter 29)],
  [bitwise], [`& | ^ ~`], [use on unsigned types (chapter 27)],
  [shift], [`<< >>`], [shifting at least the width is outside the contract (chapter 7)],
  [assignment], [`=` and compound `+= -= *= /= %= &= |= ^= <<= >>=`], [compound assignment evaluates the left side only once],
  [conditional], [`?: `], [a fork in an expression's place. the type is unified (this chapter)],
  [memory], [`& * [] . ->`], [address, dereference, subscript, member (chapters 30, 33 and 41)],
  [size and alignment], [`sizeof alignof`], [they ask a type, not a value. mostly not evaluated],
  [conversion], [`(type)`], [explicit conversion. only when needed (chapters 24 and 28)],
  [comma], [`,`], [evaluates the left and discards it; the right becomes the value],
)

Two items from the table deserve separate mention.

*Compound assignment* is not a mere abbreviation. `a[f(i)] += 1` differs from
`a[f(i)] = a[f(i)] + 1` in evaluating the left side *only once* — meaning `f` is
not called twice.

*The comma operator* evaluates the left, discards it, and takes the right as the
result. Moving two things together in the head of a for, as in
`for (i = 0, j = n; i < j; i++, j--)`, is nearly its only legitimate use;
elsewhere it harms readability. Worth remembering too that it *looks the same* as
the argument separator of a function call — the comma in `f(a, b)` is not an
operator but part of the grammar.

Finally, one seed for what follows. `fact` in fact stands on an *implicit
promise* — "n must be zero or more (a negative never reaches the floor), and at
13 or above the int container overflows (chapter 26)." The conditions a function
places on its material (preconditions) and what it promises about the result
(postconditions) — begin to look at functions through this lens and each one
appears as a little *contract*. That perspective is the root of error handling
(chapter 45) and of proven's design philosophy (chapter 38), and it is faced head
on in Part IX.

Part VI is over — we forged values (chapters 26–28), governed flow
(chapters 29–31), and gained the meaning of functions (chapter 32). The next part
is this book's second mountain, *memory* — back to chapter 5's locker corridor,
this time walking it in C's syntax. Pointers are waiting.
