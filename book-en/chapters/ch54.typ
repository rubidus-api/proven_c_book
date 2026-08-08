#import "../../book/lib.typ": *

= The world of names — four name spaces and three axes

#prereq(
  ([chapter 24, Declaring and defining functions], [scope — where a name is visible]),
  ([chapter 53, Many files — splitting and linking], [linkage — does a name cross the file boundary]),
)

#deepqa[
  Chapter 53 said a name has, besides scope, a second property called
  *linkage*. Then why does the following compile? `struct node` and `node`
  are the same spelling and yet do not collide.

  ```c
  typedef struct node node;
  ```
][
  Because there is a *third* property that is neither scope nor linkage — the
  **name space**. The compiler looks up a name that follows `struct` and a
  name used plainly *in different lists.* Same spelling, different list, no
  collision.

  This chapter takes on how many such lists there are and what they hold, and
  how they differ from scope and linkage.
]

#organizer[
#idx("name space")  You often hear that "C has no name spaces". It is half true — the standard
  defines *four* of them. What C lacks is a way for *you* to make a new one.
  This chapter sets out the four, binds them together with scope and linkage
  into three axes, and covers shadowing and reserved names. The next chapter
  builds on it to answer "so how do collisions get prevented".
]

#chapter-questions()

== The truth behind "C has no name spaces"

The saying is widespread, and it misleads just as widely. The standard's own
sentence says nearly the opposite.

Section 6.2.3 states that if more than one declaration of a particular identifier
is visible at a point in a translation unit, *the syntactic context* tells the uses
apart; thus there are *separate name spaces* for the various categories of
identifiers.

So C *does* have name spaces. What it lacks is the ability to open a new yard, as
`namespace app { … }` does. Blur that distinction and you cannot explain why both
of the following are true.

- `typedef struct node node;` is legal --- different name spaces.
- Two libraries that each export a function called `init` break the link ---
  the *same* name space.

#misconception[
  "C has no concept of a name space at all"
][
  Section 6.2.3 names four of them explicitly. The accurate statement is
  *"C has no user-defined name spaces."*

  The misconception costs something in practice. Believing there are none, you
  cannot explain what happens in the POSIX headers where `struct stat` and the
  function `stat` live side by side, nor why an enumeration constant collides with
  a variable. "There are four, and I cannot make a fifth" is the accurate map.
]

== The four name spaces

#dtable(
  columns: 3,
  [*Name space*], [*What lives here*], [*Which syntactic slot looks it up*],
  [1. Labels], [the targets of `goto`], [after `goto`, and `name:` before a statement],
  [2. Tags], [the name after `struct`, `union`, `enum`], [immediately after those keywords],
  [3. Members], [members of a struct or union --- *one yard per type*], [after `.` and `->`],
  [4. Ordinary identifiers], [variables, functions, `typedef` names, enumeration constants, parameters], [everywhere else],
)

Three points are worth fixing in mind.

- *Members have a yard per type.* The `x` of `struct point` and the `x` of
  `struct vec` have nothing to do with one another. That is why member names can
  stay short however many structs there are.
- *There is one tag yard.* `struct`, `union` and `enum` **share it between the
  three of them.** So if `enum status` exists, `struct status` cannot be made.
- *Enumeration constants live in 4, not with the tags.* This is where practice
  stubs its toe most often.

#demo("examples/ch54/four_spaces.c")

The same spelling `x` can live in all four yards at once and still compile and run,
because the compiler picks which yard to search by *syntactic context* --- after
`goto` a label, after `struct` a tag, after `.` a member, elsewhere an ordinary
identifier.

This is not, of course, an invitation to write that way. *What the language allows*
and *what a person can read* are different ranges, and this listing goes to the
extreme deliberately to show the first.

#qa[
  Which name space do macros live in?
][
  None. A macro is *a name of the preprocessor, not of the language*, and the
  preprocessor knows nothing of C's grammar (chapter 56). So a macro name ignores
  all four yards and replaces tokens wherever they appear.

  ```c
  #define max 100
  struct max { int x; };     // after struct, yet replaced by 100 → error
  ```

  Macros are the one place the name spaces cannot protect, which is exactly why the
  habit of writing macro names *in capitals* became so firmly fixed. A fence the
  grammar cannot build is built by notation instead.
]

== Three axes — scope, linkage, name space

Now put the three side by side. They confuse people because all three attach to the
same name at once while remaining independent of one another.

#dtable(
  columns: 4,
  [*Axis*], [*What it settles*], [*Values*], [*What sets it*],
  [Scope (ch. 24)], [*where* it is visible], [block / file / function / function prototype], [where the declaration was written],
  [Linkage (ch. 53)], [whether it is *the same thing* as that spelling in another translation unit], [external / internal / none], [`static`, `extern`, and where it is declared],
  [Name space (here)], [*which list* it is looked up in], [label / tag / member / ordinary], [the syntactic slot],
)

One line confirms that the three are orthogonal --- a file-scope `struct node` tag
is "file scope + no linkage (tags have none) + tag name space", while its member
`next` is "visible only inside that struct + no linkage + member name space".

#qa[
  Why do tags and members have no linkage?
][
  Linkage is a property of *the linker's joining of names*, and tags and members
  never reach the linker. The name of a type and the names of the slots inside it
  vanish once compilation ends --- what remains in machine code is an offset
  (chapter 46), not a name.

  So two files may each declare `struct point` and the link is fine. In exchange a
  danger appears: *if the two declarations differ, nobody says a word.* That is why
  chapter 53's discipline about headers matters.
]

== A habit born of tags living apart

That tags and ordinary identifiers are different yards has shaped how C code looks.

#demo("examples/ch54/tag_typedef.c")

=== `typedef struct node node;`

The commonest idiom. The tag `node` and the typedef name `node` are different
yards, so the same spelling serves both, and the caller need not write `struct`
every time. Notice too that a pointer to itself *requires* the tag --- at the point
`struct node *next;` is written, the typedef name is not yet complete.

=== And yet there are conventions that forbid `typedef`

The Linux kernel's coding style is the well-known one. Its ground is *concealment
of information* --- `node n;` alone does not say whether that is a struct, a
pointer or an integer, whereas `struct node n;` says so on sight. Where large code
is read by many, that information is worth more than a few keystrokes.

#dtable(
  columns: 3,
  [*Policy*], [*Ground*], [*Where it is used*],
  [Tag and typedef share a spelling], [the calling side gets shorter], [public library APIs (SQLite, SDL, …)],
  [No `typedef` at all], [seeing `struct` reveals the nature], [the Linux kernel and kernel-like projects],
  [`typedef` only for opaque types], [hide what is hidden, open what is open], [handle-passing APIs (`FILE *` is the archetype)],
)

Which of the three is right is not settled. *Settling on one and keeping it* is.

== Enumeration constants are ordinary identifiers

Write `enum color { red, green, blue };` and `red`, `green` and `blue` go not into
the tag yard but into yard 4 --- *the same yard as variables and functions.*

```c
enum color { red, green, blue };
int red;                 // error: 'red' redeclared as different kind of symbol
```

Hence practice's convention of prefixing enumeration constants --- `STATUS_OK`,
`GTK_ALIGN_FILL`, `SDL_QUIT`. The names look long, but in a world with one yard
that prefix is the fence.

#platform[
  C++'s `enum class`
][
  C++11 parted ways here. Declare `enum class color { red };` and the constants do
  not leak into the surrounding yard; they are written `color::red` only --- "one
  yard per enumeration".

  C23 introduced syntax for an enumeration's *underlying type*
  (`enum color : unsigned char { … }`) but left the yard the constants live in
  alone. The two features sound alike and solve different problems --- one is about
  size, the other about names.
]

== Shadowing

Declare the same spelling again in an inner scope and the outer one is *shadowed*.
It is legal, occasionally useful, and often the site of an accident.

```c
int count = 1;                    // global
static int f(int count) {         // the parameter shadows the global
    { int x = count; { int x = 2; return x; } }   // inner x shadows outer x
}
```

The trouble is that *the warning is not on by default.*

#dtable(
  columns: 2,
  [*Option*], [*Measured (GCC 14)*],
  [`-Wall -Wextra`], [catches not one shadowing (only the unused variable)],
  [`-Wshadow`], [catches all three --- the parameter over the global, and both nested locals],
)

#qa[
  Should `-Wshadow` then always be on?
][
  For new code, yes. Turn it on for the first time in old code and warnings pour
  out; there are compromises for that moment.

  - GCC and Clang's `-Wshadow=local` (only a local shadowing a local) or
    `-Wshadow=compatible-local` (only when the types match too).
  - `-Wshadow=global` to catch only what shadows a global.

  The root cure is elsewhere --- *have fewer globals.* With no global to shadow
  there is no shadowing accident. The next chapter's "export only what you must" is
  the same cure with another face.
]

#antipattern[
  Re-declaring the loop variable inside
][
  ```c
  for (int i = 0; i < n; i++) {
      for (int i = 0; i < m; i++) { /* the outer i is lost */ }
  }
  ```

  It compiles. But there is no way to refer to the outer `i` from the inner block,
  and the day somebody lifts the inner loop's body into a function, the meaning
  changes quietly. This is the pattern `-Wshadow` is best at catching.
]

== Names you must not use — the reserved yard

In a world with only four name spaces, the standard library and the implementation
*reserved space in advance.* Trespass and it may work today and break in the next
version.

#dtable(
  columns: 3,
  [*What is reserved*], [*Where*], [*Example*],
  [underscore + capital, and two underscores], [*for any use*, always], [`_Value`, `__x`, `_Atomic`],
  [one underscore + lowercase], [file-scope ordinary identifiers and tags], [`_helper` (not as a global)],
  [`str`, `mem`, `wcs` + lowercase], [taken by the `<string.h>` family], [`strdup`, `memcpy2`],
  [`is`, `to` + lowercase], [the `<ctype.h>` family], [`isodd`, `tolower2`],
  [`E` + capital or digit], [`<errno.h>`], [`EMYERROR`],
  [`LC_` + capitals], [`<locale.h>`], [`LC_MINE`],
  [`SIG`, `SIG_` + capitals], [`<signal.h>`], [`SIGMINE`],
  [`PRI`, `SCN` + lowercase], [`<inttypes.h>`], [`PRIxmine`],
  [the `_t` suffix], [reserved by POSIX (not by standard C)], [`mytype_t` --- a grey area (ch. 12)],
)

The ground is §7.1.3, and the fuller story returns in chapter 80.

#realcase[
  Why the `_t` suffix is a grey area
][
  Since standard types end in `_t` --- `size_t`, `uint32_t` --- you want to put it
  on your own. But *standard C did not reserve `_t`, and POSIX did.*

  So the situation splits. In a pure standard-C program `mytype_t` is legal. Let
  that same code meet a POSIX system's headers and it may collide the day POSIX
  starts using that name --- which has genuinely happened more than once.

  Chapter 12's ladder makes the judgement simple. For code only you use, one comment
  suffices; for a library shipping to several platforms, put a prefix in front and
  keep it *inside your own yard*, as `prov_str_t` does. That is why most large
  projects chose the latter.
]

We know the rules for names. Yet keep every rule and two libraries exporting the
same spelling still collide --- there is no way to dig a new yard in C. That is the
next chapter's problem: how collisions are prevented, and how C++ solved this spot.
