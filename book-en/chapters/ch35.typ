#import "../../book/lib.typ": *

= Null — the three siblings, formally

#prereq(
  ([chapter 6, Special knowledge about addresses], [what is special about address zero]),
  ([chapter 34, Objects, addresses, pointers], [a pointer is a value that points at something]),
)

#deepqa[
  Chapter 6 showed the history that "a null pointer's internal representation
  need not be 0" (the Prime 50, the CDC Cyber 180). Then does the comparison
  `p == 0` in source code — break on such machines?
][
  It does not — that is the standard's promise. The integer constant 0 in the
  source is read in a pointer context as a *null pointer constant*, and the
  compiler translates it so that the comparison is against that machine's actual
  null representation. The compiler upholds the separation between the symbol
  (the 0 in the source) and the representation (the bits in the machine). What
  does break is another kind of code — code that assumes *at the bit level* that
  "the representation is 0." We see that trap shortly.
]

#organizer[
  The three nulls whose faces we learned in chapter 6 — the null pointer,
  `NULL`/`nullptr`, and the NUL character — get formal treatment in syntax and
  practical rules. How to handle "empty" is the first chapter of the pointer
  safety code.
]

#chapter-questions()

== nullptr — the name of emptiness

A pointer variable must be made to point at something the moment it is declared,
or, if there is no target yet, must state that it is *empty* (chapter 23's
initialisation rule is even more vital for pointers — following a rubbish address
being the worst accident). Two notations for emptiness are current — the
traditional `NULL` macro and the keyword `nullptr` brought in by C23. This book
uses `nullptr`: `NULL` historically had definitions that differed by
implementation with subtle traps (variadic arguments and so on), while `nullptr`
is a modern notation clear down to its type.

#demo("examples-en/ch35/null.c")

The rule is exactly this demonstration — *check whether a pointer is empty before
using it.* Dereferencing an empty pointer (`*p`) is outside the contract
(undefined behaviour), and as chapter 6 taught, in a protected-mode environment
the program usually collapses on the spot — and that dying loudly is the kinder
outcome is also exactly chapter 6's story.

#realcase[
  The billion-dollar mistake — an apology from null's inventor
][
  The very value "empty" has a history of controversy. Tony Hoare, who first
  introduced the null reference into an ALGOL-family language in 1965 (the same
  Hoare of the quicksort family of algorithms, chapter 30), called it in a 2009
  lecture *"my billion-dollar mistake"* — a self-indictment that "I could not
  resist the temptation of it being easy to implement, and the cost of the
  collapses and vulnerabilities null dereferences have caused over the following
  forty years must exceed a billion dollars." That modern languages (Rust, Swift,
  Kotlin) evolved to force a check on "may be empty" through the type system is a
  direct descendant of that reflection. C has no such enforcement — checking is
  the business of culture and discipline, which is why this book pins the check
  down as idiom.
]

== How far may 0 be used — the exact definition of a null pointer constant

Chapter 6 said "the 0 in the source is a symbol". The standard fixes exactly
what qualifies as that symbol. A *null pointer constant* is one of two things:

- an *integer constant expression* with the value 0
- such an expression cast to `void *`

C23 added the keyword `nullptr` to those. So the five spellings below all mean
the same thing.

#dtable(
  columns: 3,
  [*Spelling*], [*A null pointer constant?*], [*Note*],
  [`0`], [Yes], [The oldest spelling],
  [`0L`, `0u`, `'\0'`], [Yes], [Any integer constant expression with value 0],
  [`(void *)0`], [Yes], [`NULL` is commonly defined this way],
  [`NULL`], [Yes], [A macro the implementation defines as one of the above],
  [`nullptr`], [Yes], [C23. Its type, `nullptr_t`, is unambiguous],
  [`zero` in `const int zero = 0;`], [*No*], [A variable, not a constant expression — a compile error],
  [An `int` variable that became 0 at run time], [*No*], [Putting an integer into a pointer is another matter],
)

The last two rows are the point. What qualifies is not *being zero* but *being a
constant expression*. So passing a variable whose value is zero, as in
`int *p = zero;`, is not a null assignment but "an integer into a pointer" —
something else entirely, and today's compilers reject it.

=== The bits it becomes are another matter

The same spelling does not mean the same representation. Chapter 6's thread is
picked up here.

#demo("examples-en/ch35/nullrep.c")

The demonstration puts the two layers side by side. All five spellings become
the same value (`a == b && b == c`), and every comparison is true — *the
spelling layer is what the standard promises.* The printed bytes, on the other
hand, are merely *what this implementation chose*. Run the same program on
chapter 6's Prime 50 or CDC Cyber 180 and the comparisons would still be true
while the bytes would not be zero.

#dtable(
  columns: 3,
  [*What you do*], [*Which layer*], [*Does it promise null?*],
  [`p = nullptr;` / `p = 0;`], [Spelling], [★ Yes],
  [`p == nullptr` / `!p`], [Spelling], [★ Yes],
  [`struct s x = { 0 };`], [Value], [★ Yes — pointer members null, floating members 0.0],
  [`memset(&x, 0, sizeof x)`], [Representation], [No],
  [`calloc(n, size)`], [Representation], [No],
  [`memcmp(&p, zeros, sizeof p)`], [Representation], [Not a way to ask whether it is null],
)

The third and fourth parts of the demonstration show those rows in the flesh. A
struct made with `{ 0 }` has *the standard's* promise that its pointer members
are null and its floating members 0.0, while `memset` and `calloc` merely happen
to give the same result on this machine.

#platform[
  How C++ differs — the same 0, other rules
][
  This is the famous place where C and C++ part. The root is one thing: *C++ has
  no implicit conversion from `void *` to another pointer type.*

  So C++'s `NULL` cannot be defined as `((void *)0)`; that would make
  `int *p = NULL;` an error outright. Checked on this machine, C's `NULL` is
  `((void *)0)` while g++'s is the compiler extension `__null`. Traditional C++
  implementations simply defined it as `0` or `0L`.

  Using `0` as null creates an accident peculiar to C++: *overloading.*

  ```cpp
  void f(int);
  void f(char *);
  f(NULL);      // if NULL is 0 -> f(int) is quietly chosen
  ```

  Run it and, with `NULL` defined as `0`, `f(int)` wins; with g++'s `__null` the
  call is *ambiguous* and fails to compile. Either way it is not the `f(char *)`
  that was meant.

  #dtable(
    columns: 3,
    [*What*], [*C*], [*C++*],
    [`(void *)0` into another pointer], [Fine], [Error — a cast is required],
    [The definition of `NULL`], [Usually `((void *)0)`], [`0`, `0L` or `__null`],
    [A `const int` variable holding 0], [Not null (not a constant expression)], [Null in C++03, *an error from C++11*],
    [A dedicated keyword], [C23's `nullptr`], [C++11's `nullptr` (type `std::nullptr_t`)],
  )

  The third row's history is interesting. C++03 said "an integral constant
  expression rvalue of integer type that evaluates to zero", so the `zero` of
  `const int zero = 0;` was a null pointer constant; C++11 narrowed it to "an
  integer literal with value zero, or a prvalue of type `std::nullptr_t`". This
  book checked that the same code compiles under `-std=c++03` and fails under
  `-std=c++11`.

  The conclusion is the same in both languages — *use `nullptr`.* C++11 brought
  it in first and C23 followed, and one word removes both the overloading
  accident and the ambiguity of "is this a zero or a pointer?".
]

== The trap — code that assumes the representation is 0

Let us collect the foreshadowing of the opening exchange. These two pieces of
code do not mean the same thing:

```c
int *arr[8];
for (int i = 0; i < 8; i += 1) { arr[i] = nullptr; }  /* correct */
/* memset(arr, 0, sizeof arr);     — fill the bits with 0: a different meaning! */
```

The first line says "hold a null pointer" (the world of symbols — whatever the
representation, the compiler handles it). The commented memset says "fill every
bit with 0" (the world of representation). On today's mainstream machines, where
the null representation is all zeros, the results coincide by accident, but as
chapter 6's history shows, that accident is not a contract. You will often see
the memset style in practice (and on mainstream platforms it does work), but with
*the eye of portability* it is a grey zone, and using it knowingly differs from
using it unknowingly — this book's choice is the explicit `nullptr` assignment.

#qa[
  When does the NUL character (`'\0'`) get its formal treatment — one of the
  three siblings is still outstanding.
][
  Three chapters later. The NUL character works not in the world of pointers but
  in the world of *strings* (as the end marker), so chapter 39, which treats
  strings properly, is its stage. While waiting, the distinction once more —
  `nullptr` is "nowhere to point" (a pointer value), `'\0'` is "the text ends
  here" (a character value, one byte in size). Strangers living in different
  worlds.
]

We can handle emptiness. The next chapter takes the remaining rules that bind
pointers — the constraint chapter 6's alignment places on pointer casts, and a
practical feel for the provenance (the tag of origin) foreshadowed in chapter 14.
