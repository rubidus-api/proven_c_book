#import "../../book/lib.typ": *

= Lifetime and storage duration

#prereq(
  ([chapter 24, Declaring and defining functions], [while a function runs]),
  ([chapter 2, The regions of memory], [the stack and the static region]),
)

#deepqa[
  Chapter 24 taught scope — the range in which a name is *visible*. But
  chapter 40 used the phrase "the return ledger of a function call". Are a name
  ceasing to be visible and that memory *vanishing* the same thing?
][
  They are different, and that distinction is this chapter's skeleton. *Scope* is
  a translation-time notion (where that name may be used) and *lifetime* is a
  run-time notion (from when to when that memory is valid). Usually they travel
  together, but the moment they diverge is an accident — the name has gone and
  the address remains (see the misconception box below).
]

#organizer[
  We draw the map of memory — where a variable lives, when it is born and when it
#idx("stack")  dies. Automatic and static lifetime, the ledger called the stack,
  and the accident to be most careful of in this part (keeping the address of
  something that has vanished).
]

#chapter-questions()

== The standard's four axes — storage duration, scope, linkage, storage class

Let us set the terms out formally here. The C standard defines *four mutually
independent properties* for names and objects, and lumping them together
guarantees confusion later (especially over static's two faces).

#idx("storage duration")#idx("scope")#idx("linkage")
#dtable(
  columns: 3,
  [*standard term*], [*what it fixes*], [*a notion of when*],
  [*storage duration*], [*from when to when* the object exists], [run time],
  [*scope*], [*where* that name may be used], [translation time],
  [*linkage*], [whether it is *the same thing* as that name elsewhere], [translation and link time],
  [*storage-class specifier*], [*how* the three above are written], [syntax],
)

The last line matters. `static`, `extern`, `auto`, `register`, `typedef`, and
C11's `_Thread_local` (C23's `thread_local`) are words occupying *one slot*
syntactically, and which of them you write fixes the three properties above.
There is only one such slot per declaration, so `static extern int x;` is a
syntax error.

=== Storage duration — four

#dtable(
  columns: 3,
  [*storage duration*], [*born when, dies when*], [*how it is made*],
  [automatic], [on entering a block ~ on leaving it], [an ordinary declaration inside a block],
  [static], [before the program starts ~ when it ends], [a file-scope declaration, or `static`],
  [thread], [when that thread starts ~ when it ends], [`thread_local` (C11)],
  [allocated], [`malloc` ~ `free`], [chapter 43],
)

The standard's word is not *dynamic* but *allocated storage duration*. What is
commonly called "dynamic allocation" is this, and this book follows the common
usage while recording the standard term here.

=== Scope — four

The range in which a name is visible. The C standard divides it into four.

#dtable(
  columns: 3,
  [*scope*], [*visible how far*], [*example*],
  [block], [from the declaration to the end of that block], [a local variable inside a function],
  [file], [from the declaration to the end of that translation unit], [a declaration outside functions],
  [function], [the whole of that function], [*label names only* (the targets of `goto`)],
  [function prototype], [inside the prototype's parentheses], [parameter names written in a prototype],
)

The third will look unfamiliar: it is the special case that a label is visible
throughout the function wherever inside it it is written. The fourth fixes how
far a name written only in a prototype, as in `void f(int count);`, lives — it
disappears outside the parentheses, so a prototype's parameter names are
effectively *comments*.

*The inner hides the outer.* When the same name overlaps, the inner block's wins.

```c
int n = 1;                  /* file scope */
void f(void) {
    int n = 2;              /* block scope — hides the outer n */
    { int n = 3; use(n); }  /* here it is 3 */
    use(n);                 /* here it is 2 */
}
```

=== Linkage — three

It fixes whether the same name appearing in several places *refers to one and the
same object*. It is the groundwork of chapter 51 (several files).

#dtable(
  columns: 3,
  [*linkage*], [*meaning*], [*how it comes about*],
  [external], [the same thing *across* translation units], [the default at file scope, `extern`],
  [internal], [the same thing *only within this translation unit*], [`static` at file scope],
  [none], [separate for each declaration], [ordinary variables in a block, parameters, `typedef` names],
)

#antipattern[
  Reading static's two faces as the same thing
][
  The same word does entirely different jobs depending on *where it is written*.
  Seen through the standard's terms there is no room for confusion.

  ```c
  static int counter;        /* file scope: makes the linkage *internal* (the duration was static anyway) */

  void f(void) {
      static int calls;      /* block scope: makes the storage duration *static* (there is no linkage) */
  }
  ```

  The first `static` *does not change the lifetime* — a file-scope variable has
  static storage duration regardless. What it changes is the *linkage*, and it
  means "this name cannot be used outside this file." The second `static` *has
  nothing to do with linkage* — a local name has none to begin with. What it
  changes is the *storage duration*.

  Memorise it in one sentence: *`static` at file scope hides; `static` at block
  scope keeps alive.*
]

=== The remaining storage-class specifiers

- *`extern`* — a declaration saying "this name is defined somewhere else." Being
  an announcement rather than a definition, it takes no memory (chapter 16's
  linker joins the real thing).
- *`auto`* — the old word stating automatic storage duration. Being the default
  anyway, nobody wrote it, and *C23 recycled the slot for type inference* — write
  `auto x = 1 + 2;` and the type comes from the initialiser.
- *`register`* — the old request "in a register if possible" (chapter 10). It has
  no effect on today's optimisation, but one rule survives: *the address of a
  `register` variable cannot be taken* (`&x` becomes an error).
- *`typedef`* — syntactically it goes in this slot but does something entirely
  different. Instead of a variable it makes a *type name* (treated in
  chapter 55).
- *`thread_local`* — makes an object that is separate per thread (chapter 66). It
  is the one exception that may be written together with static storage duration
  (`static thread_local`).

#qa[
  And what is the difference between a "declaration" and a "definition"?
][
  A *definition* is a declaration that makes the real thing. For a variable it
  takes memory; for a function it writes the body. A mere *declaration* is only an
  announcement that "a name of this type exists somewhere."

  ```c
  extern int total;      /* declaration — takes no memory */
  int total = 0;         /* definition — the real thing appears here */
  ```

  The rule is one: *a definition once in the whole program, declarations as often
  as needed.* Break it and you get the linker errors seen in chapter 16
  (`undefined reference` when there is no definition, `multiple definition` when
  there are two or more). The practice of putting declarations in a header and the
  definition in one source file comes from here (chapter 51).
]

== Two lifetimes

#idx("storage duration")A local variable in C has, by default, *automatic
storage duration* — born on entering a block, dead on leaving. That parameters
and local variables are born anew on each function call was the ground on which
chapter 32's recursion stood.

Attach `static` and it becomes *static storage duration* — born once before the
program starts and living until it ends (initialised exactly once too). The
demonstration contrasts the two.

#demo("examples-en/ch41/life.c")

`next_ticket`'s `issued` keeps its value between calls and grows 1, 2, 3, while
`fresh_count`'s `n` is born anew on each call and is always 1. Both are
"variables inside a function" and yet their lifetimes differ. (Note in addition
that the demonstration split the calls into separate statements — exactly
chapter 32's rule that piling side-effecting calls into one expression leaves the
evaluation order unspecified.)

== The map of memory — the regions with our own eyes

At this point let us see in one picture how a program's memory is actually laid
out. Below are addresses printed directly on this book's verification machine.

#demo("examples-en/ch41/regions.c")

The way to read it is *the order, not the values*. From low addresses they line
up like this.

#dtable(
  columns: 3,
  [*region*], [*what lives there*], [*lifetime*],
  [code], [the machine instructions of functions], [the whole program (read-only)],
  [read-only data], [string literals, `const` data], [the whole program (writing collapses)],
  [`data`], [globals and `static`s with an initial value], [the whole program],
  [`bss`], [globals and `static`s with no (= zero) initial value], [the whole program],
  [heap], [what `malloc` gave (chapter 43)], [until freed],
  [stack], [local variables, parameters, return addresses], [until the function ends],
)

The example confirmed three things. *`data` and `bss` sit side by side*, *the
heap grows upward above them*, and *the stack grows downward from a far distant
high address* (the example's last three lines stack one more frame and measure
that direction).

#qa[
  What is that strange name `bss`? And why is it separated from `data`?
][
  The name is an abbreviation of a 1950s assembler instruction, `Block Started by Symbol` — the meaning was forgotten and only the name crossed half a century.

  The reason for separating them is practical. Consider a large global whose
  *initial value is 0*, such as `int table[1000000];`. Put it in `data` and a
  million zeros go inside the executable, making the file 4 MB bigger. Put it in
  `bss` and only *one number saying "fill this much with zeros"* is written in the
  file, with the actual filling happening when the program starts. So variables in
  `bss` get C's promise that "an uninitialised one is 0" for free — that zero was
  filled in by the operating system (or, in embedded work, the startup code).
]

#platform[
  How large is the stack — Linux and Windows
][
  *The C standard has neither the word "stack" nor any promise about its size.* It
  fixes only automatic storage duration and leaves where and how to place it to
  the implementation. So the size is decided by *the operating system and the
  tools*.

  - *Linux* — the main thread's default limit is usually *8 MiB* (check and change
    it with `ulimit -s`; this book's verification machine was 8388608 bytes too).
    It can be raised if needed, and the stack made for each thread is set
    separately with `pthread_attr_setstacksize`.
  - *Windows* — the default is *1 MiB*. Moreover only the first 4 KiB of it is
    actually committed, growing as it is used. Change it with the linker option
    `/STACK:reserve[,commit]` when building the executable, and for a thread
    specify it as an argument to `CreateThread`.

  There is a place where the difference shows in practice. Code that ran fine on
  Linux dying of stack overflow on Windows — *the same code, a container eight
  times narrower*. A large local array (`char buf[2*1024*1024];`) or deep
  recursion are the candidates. The fuller map, and the circumstances of embedded
  work, are treated in chapter 70.
]

== The stack — the ledger of calls

The place where automatic variables live has a name — the *stack*. Each time a
function is called, a bundle of slots for that call (a stack frame) is laid on
top, and when the function ends it is lifted off whole. A frame contains, along
with local variables and parameters, *the address to return to* (the return
address) — the identity of the "return ledger" whose name was brushed past in
chapter 40.

With the picture in place two things are explained at once. First, why
chapter 32's recursion piles up in layers — because one frame is laid on per
call. And recurse too deeply and the stack space runs out and the program
collapses (*stack overflow* — the representative symptom of infinite recursion).
Second, why chapter 40's boundary-violation attack is so dangerous — overflow an
array on the stack and you can overwrite *the return address of that same frame*,
whereupon the function, on finishing, "returns" to a place the attacker chose.
One array's boundary is connected to control of the program.

#misconception[
  "The address of a variable inside a function can still be used after the
  function ends"
][
  The commonest accident right after learning pointers, and the frightening part
  is that *it appears to work for a while*. When a function ends the frame is
  lifted, but the bits in that place are not immediately erased, so following a
  dead variable's address still reads the old value for a time. Then, the moment
  another function uses that place as its frame, the value flips — becoming a bug
  that goes off later, somewhere unrelated. Such an address is called a *dangling
  pointer*, and the rule is one: *the address of a local variable must not
  outlive its function.* To send a function's result out to live longer, there are
  three ways: use static lifetime, use the next chapter's dynamic memory, or fill
  a container the caller provided (chapter 34's `&` idiom). Chapter 17's ASan is
  also the representative tool for catching this accident at run time.
]

#qa[
  Are global variables — declared outside functions — of static lifetime too?
][
  They are. A variable outside functions has static storage duration and lives for
  the whole program. Separately from lifetime, though, a question of *visibility*
  attaches — whether to make it visible in several files or keep it to this one is
  the subject of chapter 51 (linkage). And the practical advice is an old one:
  *keep mutable global state to a minimum.* A value that can change anywhere is
  hard to trace, and in chapter 12's multicore world it is a source of accidents.
  A `static` inside a function has the same property in miniature (the
  demonstration's `issued`), so convenient though it is, the practice is not to
  overuse it.
]

Two places on the map of memory — the stack (automatic) and the static region —
are learned. The remaining place is this part's last: memory whose size is
settled at run time and which stays alive as long as you wish — chapter 42's
dynamic memory.
