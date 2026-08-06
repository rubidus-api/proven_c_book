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
