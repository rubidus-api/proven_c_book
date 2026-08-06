#import "../../book/lib.typ": *

= Functions as values — the function pointer

#prereq(
  ([chapter 24, Declaring and defining functions], [the name of a function]),
  ([chapter 37, Arrays], [a name decaying into a value]),
)

#deepqa[
  Chapter 37 said an array name decays into the address of its first element, and
  chapter 53 said a variadic function loses type information. Then what does a
  *function name* evaluate to in an expression?
][
  *To a pointer to the function* — the same decay as with arrays exists for
  functions. The exceptions are only when it is the operand of `sizeof` or `&`, and
  since applying `sizeof` to a function is forbidden to begin with, effectively `&`
  alone is the exception. And `&function` gives the same pointer in the end anyway.
  So the strange situation arises in which `f`, `&f` and `*f` are all the same
  value — which is this chapter's first example.
]

#organizer[
  Until now a function has been "a thing you call". In this chapter we handle a
  function *as a value* — put it in a variable, pass it as an argument, lay several
  out in an array. Along the way we see an old rule of C (*a function name decays
  into a pointer*), the strange syntax that rule makes, and how to build object
  orientation by hand with this tool.
]

#chapter-questions()

== The name decays into a pointer

#demo("examples-en/ch54/funcptr.c")

The first two lines of the output are this chapter's heart.

*`add == &add`* — the function name decays into a pointer, and attaching `&` gives
the same pointer. So the two are equal.

*`(*******p)(2,3)` compiles* — the reason any number of stars gives the same result
is this. `*p` dereferences the pointer to obtain a *function designator*, and the
moment that designator is used as a value it *decays back into a pointer*. That
is, each `*` merely goes once round the loop "pointer → function → pointer" and
stays where it was.

The opposite direction does not hold. `&&add` is a syntax error — `&add` is
already a *value* (a pointer), and in C the address of a value cannot be taken (to
take an address there must be a named place, that is, an lvalue). Hence the
asymmetry that `**add` works while `&&add` does not.

#misconception[
  "To call a function pointer you must dereference it, as in `(*p)(x)`"
][
  An old practice. `p(x)` and `(*p)(x)` are entirely the same and the standard
  permits both. The reason some codebases prefer `(*p)(x)` is the documentary
  purpose of *telling the reader that this is a function pointer*, not a
  requirement of the grammar. Either way, be consistent.
]

== The type is the contract

A function pointer's type is settled by the return type and the parameter list.

```c
int  (*p)(int, int);       /* pointer to a function taking two ints, giving an int */
void (*q)(void);           /* pointer to a function with no arguments and no return */
int  (*r[4])(int);         /* an array of four such pointers */
int  (*(*s)(void))(int);   /* hard to read — use a typedef */
```

As the last line shows, declarations quickly turn rough. The practice in the field
is `typedef`.

```c
typedef int (*binop_fn)(int, int);
binop_fn table[] = { add, mul };
```

*Casting to a function pointer of a different type and calling it is outside the
contract.* For example, something stored as `void (*)(void)` must not be called
back as `int (*)(int)`. Only storing and *converting back to the original type* to
call is guaranteed.

#antipattern[
  Matching a comparator signature by casting
][
  ```c
  int cmp_int(const int *a, const int *b);            /* looks convenient, but */
  qsort(v, n, sizeof v[0], (int (*)(const void *, const void *))cmp_int);
  ```
  `qsort` passes two `const void *`, while the real function expects
  `const int *`. Since it is *not guaranteed* that the two types have the same
  representation, this call is outside the contract. The right way is to match the
  signature exactly and cast inside — as the example's `cmp` did.
]

== `void *` and function pointers are different worlds

Chapter 34 taught that `void *` is "a vessel that holds any data pointer". Yet
*function pointers do not go into that vessel.* The standard does not define
conversion between data pointers and function pointers.

The reason lies in history and hardware. On a machine using a *Harvard
architecture*, code and data are in different address spaces — the AVR
microcontroller is representative, and there "address 0" exists separately in the
code region and in the data region. Even the widths of the addresses may differ.
On such a machine, putting the two pointers in the same vessel is impossible to
begin with.

That is why the example printed the two sizes together. On this machine they
happened to be equal, but *there is no guarantee anywhere that they are*.

#realcase[
  The place POSIX parted from the standard — `dlsym`
][
  The POSIX function `dlsym`, which finds a function in a dynamic library, returns
  a `void *`. But what we want is a function pointer. That is, this API demands *a
  conversion standard C does not define*.

  POSIX acknowledged this contradiction and, in its 2008 edition, separately pinned
  down that "implementations shall support this conversion." Compilers still warn,
  so the following idiom has settled in practice.

  ```c
  void (*fn)(void);
  *(void **)&fn = dlsym(handle, "do_work");   /* the idiom that skirts the gap in the standard */
  ```

  It is a rare case of "the platform demanding what the standard forbids", and a
  good specimen of where C's portability splits.
]

== Dispatch tables — an array instead of a `switch`

The example's ④ is that. Pair names with functions and lay them out in an array,
and you choose by *data* instead of by branching. To add an item you mend only the
table, not the code, and combined with chapter 52's X macro you can even generate
the table from a single list.

Grow this pattern and it becomes a state machine, a command interpreter, a plugin
structure. And grow it further — that is the next section's story.

== The virtual function table — object orientation built in C

C has no classes. But *put, as a struct's first member, a pointer to a table of
function pointers* and you obtain polymorphism, the heart of object orientation.

#demo("examples-en/ch54/vtable.c")

The design has four bones.

+ *One table per type* (`static const`). Copying the function pointers into every
  instance makes objects large and the cache worse — so the table is kept
  separately in a single copy, and the object holds only one pointer to it.
+ *The base struct is the first member.* The standard guarantees that "a struct's
  first member begins at the same address as the struct itself", so
  `struct circle *` and `struct shape *` can be safely gone between (chapter 43).
+ *Calls go through the table* — `s->vt->area(s)`. This one line does exactly what
  a C++ virtual function call does.
+ *The object itself is passed as the first argument.* Writing by hand the `this`
  that C++ hides.

The last two lines of the output show the cost. What grows per object is one
pointer (8 bytes) only, and the table exists once per type.

#realcase[
  GTK's GObject — a hand-built object system in real use
][
  The proof that this approach is not a toy is GTK. GTK, the major toolkit of the
  Linux desktop, is written in pure C and beneath it lies an object system called
  *GObject*. Its structure stands on the same bones we have just seen.

  - The first member of the instance struct points at the *class struct* (our
    vtable).
  - Function pointers are laid out in the class struct, and a derived class
    "overrides" some of them by writing its own functions over them.
  - Inheritance is expressed by putting the base struct as the first member, and
    type conversion is wrapped in macros with checks attached (things like
    `GTK_WIDGET(x)`).
  - On top of that ride reference counting, signals (the observer pattern) and a
    property system.

  The same design can be seen elsewhere. The Linux kernel's
  `struct file_operations` — a table holding the `read` and `write` functions that
  differ per file system — is exactly a vtable, and Windows' COM is this very
  convention pinned down at the ABI level.
]

#qa[
  Then what differs from C++'s virtual functions?
][
  The concept is the same; *the degree of automation* differs. In C++ the compiler
  makes the table, plants the pointer, connects it in the constructor, and checks
  type conversions. In C all of that is handwork, so there are many places to slip
  — an object whose table was not connected, a struct that broke the first-member
  rule, code that casts a derived type wrongly.

  What is gained in exchange is clear too. *Everything is visible — what is where.*
  You can count how many tables there are, how many pointers are followed per call,
  how many bytes an object is. It is the place where this book's constant refrain,
  "a language in which cost is visible", appears just the same in object
  orientation.

  One thing more. C++'s virtual table layout is *not settled by the standard* (the
  ABI settles it). That is why, when mixing C and C++, class objects are not passed
  across the boundary and only `extern "C"` functions and plain structs are
  exchanged (chapter 90).
]

#recap[
  Function pointers in summary.

  #dtable(
    columns: 2,
    [*rule*], [*content*],
    [decay], [a function name used as a value becomes a pointer],
    [`f`, `&f`, `*f`], [all the same pointer. `&&f` is a syntax error],
    [call notation], [`p(x)` and `(*p)(x)` are identical],
    [type], [return type + parameters. casting to another type and calling is outside the contract],
    [`void *`], [no guarantee of conversion with function pointers (Harvard architecture)],
    [`dlsym`], [POSIX guarantees it separately. the `*(void **)&fn` idiom],
    [dispatch table], [choosing by data instead of by branching],
    [vtable], [one table per type, one pointer per object. the first-member rule is the ground],
    [in the flesh], [GObject (GTK), the kernel's `file_operations`, COM],
  )
]

We are equipped even to handle functions as values. Yet several times in this
chapter there were places where the declaration itself was hard to read and we
fled to `typedef` — things like `int (*(*s)(void))(int);`. The next chapter pays
that debt: C's most notorious place, *reading declarations*, met head on with two
ways of reading and with `typedef`.
