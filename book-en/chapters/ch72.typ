#import "../../book/lib.typ": *

= Non-local jumps — `<setjmp.h>`

#prereq(
  ([chapter 71, Signals], [the thing that cuts into the flow]),
  ([chapter 39, The stack and calls], [how call frames are stacked and cleared]),
  ([chapter 48, How to report failure], [returning failure as a value]),
)

#deepqa[
  Chapter 39 said that calling a function stacks a frame, and that `return`
  clears that frame and goes back *one step at a time*. Then how, in C, does one
  return from ten levels deep to the top *in a single move*?
][
  The two words of `<setjmp.h>` do exactly that. `setjmp` records "here, now",
  and `longjmp` revives that record and *rewinds the stack whole*. Ten levels or
  a hundred, it comes back at once.

  What is rewound, though, is only *the stack pointer and a few registers*. No
  one closes the files opened in between, frees the memory taken, or releases the
  locks held — half of this chapter is about that price.
]

#organizer[
#idx("non-local jump")  A close reading of one header, `<setjmp.h>`. Why it exists (the makeshift of a
  language without exceptions), the exact shape and return values of the two
  words, the eight slots of `jmp_buf` and which registers come back, the
  `volatile` rule that follows from them (measured on a real build), how real
  software such as libjpeg and PostgreSQL copes with the memory left behind by a
  jump, and today's alternatives.
]

#chapter-questions()

== Why it exists — the makeshift of a language without exceptions

When trouble arises deep down, a language with exceptions escapes to the upper
floors with one `throw`. C has none. So an error must be *carried up one layer at
a time as a return value*, and every function in between must take that value and
pass it on (chapter 48).

`setjmp`/`longjmp` is the device that skips that chain. It was already in Unix V7
in the 1970s, and its reason for being was the same as now — *getting out of deep
recursion in one move.*

The standard's footnote states the purpose narrowly: these functions "are useful
for dealing with unusual conditions encountered in a low-level function of a
program." That is, the original place of the device is *an escape hatch for
exceptional situations, not a substitute for exceptions.*

#qa[
  Is this then C's exception handling?
][
  No. Three differences are decisive.

  *First, nothing is cleaned up.* A C++ exception rewinds the stack calling
  destructors (stack unwinding). `longjmp` simply jumps — open files, taken
  memory, locked mutexes all stay as they were.

  *Second, there is no type.* The only thing thrown is one `int`. To carry what
  went wrong, that value must serve as an index into something written elsewhere.

  *Third, the region is limited.* The function that called `setjmp` must *still be
  alive*. There is no jumping into the place of a function that has already
  returned — doing so is undefined behaviour.

  So the conclusion of this chapter, stated in advance: *new code mostly does not
  use it. But you must be able to read it* — because widely used libraries are
  built on it.
]

== The two words — the exact shape

#dtable(
  columns: 2,
  keycol: false,
  [*Declaration*], [*What it does*],
  [`int setjmp(jmp_buf env);`], [Saves the present calling environment in `env`. *It is a macro*],
  [`[[noreturn]] void longjmp(jmp_buf env, int val);`], [Goes back to the place saved in `env`. Does not return],
)

=== `setjmp` — the macro that seems to return twice

`setjmp` is a *macro*, not a function (the standard allows a function to be
provided as well, but suppressing the macro and calling the function directly is
undefined behaviour).

The rule for its return value is simple, and it is nearly the whole header.

#dtable(
  columns: 2,
  [*How it was reached*], [*What `setjmp` returns*],
  [Called directly (the moment the place is saved)], [0],
  [Returned to by `longjmp(env, val)`], [`val` — except that 0 becomes 1],
)

That is why `longjmp(env, 0)` cannot produce 0. Zero already means *"saving right
now"*, so the standard turns it into 1.

#demo("examples-en/ch72/jmp_basic.c")

The demonstration follows that flow exactly. The first time it yields 0 and goes
down into `deep`; at depth 3 the `longjmp(env, 42)` brings us *straight back to
`main`'s `setjmp`* and 42 appears. The intervening `deep(2)` and `deep(1)` never
get to print their "leaving normally" line — those frames simply vanished.

The last test is `longjmp(env, 0)`. We gave 0, and 1 came back.

=== Where `setjmp` may appear — four contexts only

The standard (§7.13.1.1) restricts the context *by enumeration*. Using it outside
these four is undefined behaviour.

#dtable(
  columns: 2,
  keycol: false,
  [*Permitted context*], [*Example*],
  [The *entire* controlling expression of `if`, `switch`, `while`, …], [`if (setjmp(env)) { ... }`],
  [One operand of a comparison with an integer constant expression (the whole being a controlling expression)], [`if (setjmp(env) == 0)`],
  [The operand of a single `!` (likewise)], [`if (!setjmp(env))`],
  [An entire expression statement (a cast to `void` is allowed)], [`setjmp(env);`],
)

So `int rc = setjmp(env);` is *not one of the standard's contexts.* It does work
on many implementations and this book's demonstration writes it that way, but
where portability matters, `switch (setjmp(env))` or `if (setjmp(env) == 0)` is
the safer form.

#qa[
  Why does such an odd restriction exist?
][
  Because `setjmp` is a thing *called once and returned from twice*. To a
  compiler this is not an ordinary call — if its return value sat in the middle of
  a complicated expression, there would be no way to decide how to revive the
  half-computed state (temporaries spread across registers).

  So the standard narrowed the context down to *places where no temporary can be
  alive*: one controlling expression, one comparison against a constant, and an
  expression statement that discards the value. The restriction looks strange, but
  it comes from implementability.
]

=== `longjmp` — three things to check before jumping

`longjmp(env, val)` does not return (C23 marks it `[[noreturn]]`). Three
conditions must hold before jumping, and breaking any one is undefined behaviour.

#dtable(
  columns: 2,
  [*Condition*], [*If broken*],
  [There really was a `setjmp` on that `env`], [Jumping to a place never saved],
  [The function that called `setjmp` is *still alive*], [Jumping into a vanished frame — usually instant death],
  [It is the same thread], [Jumping into another thread's stack],
)

The second is the accident that happens most often. Thinking "I will make an error
handler and call it from anywhere", one calls `longjmp` after the function that
called `setjmp` has already returned — and by then another function's frame has
moved into that place.

== What `jmp_buf` really is

`jmp_buf` is *an array type*. The standard nails that down for a practical reason
— being an array, passing it to a function decays it to the address of its first
element (chapter 37), so `setjmp(env)` can *modify the original* without writing
`&env`.

What is inside? The standard says only "information sufficient for `longjmp` to
restore the calling environment". In practice it is roughly these.

#dtable(
  columns: 2,
  [*What is saved*], [*Why*],
  [The stack pointer], [The place to rewind to must be known],
  [The program counter (return address)], [Where to go back to],
  [Callee-saved registers], [The values the calling convention promised to preserve],
  [(Sometimes) the signal mask], [POSIX's `sigsetjmp`, optionally],
)

The size the demonstration printed (200 bytes on this implementation) is the sum
of those contents. But *looking inside or editing it by hand is outside the
contract* — treat it as opaque data.

The standard is also explicit about what is *not* saved: *the state of the
floating-point environment, of open files, or of any other component of the
abstract machine.* That one sentence produces every pitfall in the next section.

=== Looking inside the eight slots

On this implementation (glibc, x86-64) the 200 bytes of a `jmp_buf` divide thus.

#dtable(
  columns: 2,
  [*Part*], [*Size*],
  [Eight register slots (`long [8]`)], [64 bytes],
  [Whether the signal mask was saved (`int` + padding)], [8 bytes],
  [Room for the signal mask], [128 bytes],
)

Print the eight slots and their order appears — RBX, RBP, R12, R13, R14, R15, the
stack pointer, the return address. That is exactly the list the x86-64 SysV
calling convention marks *callee-saved* (chapter 39's calling convention).

Two things stand out.

*First, three of the slots hold scrambled values.* Print the slots for RBP, the
stack pointer and the return address and they do not look like stack addresses.
glibc stores them mixed with a per-thread guard value — a device against
overwriting a `jmp_buf` to jump to an address of the attacker's choosing. It
confirms the rule that the inside of a `jmp_buf` is not to be edited by hand.

*Second, registers not on that list are not saved.* RAX, RCX, RDX, RSI, RDI and
R8--R11 are the ones the convention marks "the caller's business" (caller-saved),
so `setjmp` does not keep them.

=== And that is the `volatile` rule

Read those two facts from a local variable's point of view and the next section's
rule follows by itself. A compiler puts a local in one of three places.

#dtable(
  columns: 3,
  [*Where the variable was*], [After `longjmp`], [*Why*],
  [In memory on the stack], [The changed value, intact], [`longjmp` restores only the stack *pointer*. It does not touch the contents],
  [In a callee-saved register], [The old value from `setjmp`], [That slot is restored wholesale from the `jmp_buf`],
  [In a caller-saved register], [Anything at all], [It is neither saved nor restored],
)

Turn optimisation off and the compiler mostly puts locals on the stack, so only
the first row happens — which is why nothing *appears* wrong at `-O0`. Turn it on
and heavily used variables move into registers, and the second and third rows
appear.

It is also why the standard writes *"indeterminate representation"* rather than
"becomes the old value". The result depends on where the variable was, so the
standard promises none of them.

== What happens to values on return — the `volatile` rule

The most practical rule in this chapter. The standard (§7.13.2.1) settles it thus.

*All objects have the values they had at the time `longjmp` was called, with one
exception — objects of automatic storage duration local to the function
containing the `setjmp`, that are not `volatile` and have been changed since the
`setjmp`, have indeterminate representations.*

#demo("examples-en/ch72/jmp_volatile.c")

Three variables split exactly along that rule.

#dtable(
  columns: 3,
  [*Variable*], [*Storage and qualifier*], [After `longjmp`],
  [`plain`], [automatic, non-`volatile`, changed in between], [*No guarantee*],
  [`guarded`], [automatic, `volatile`], [Guaranteed],
  [`statik`], [static], [Guaranteed],
)

*This rule really does bite.* Build with optimisation off and all three show 2;
build the same code with `-O2` and `plain` *comes back as 1* — the compiler had
kept that variable in a register, and `longjmp` restored the registers to their
values at `setjmp`. This book ran both builds and confirmed the difference, and
the example reports which build it is by looking at `__OPTIMIZE__`.

#misconception[
  "`volatile` is for hardware registers and nothing else"
][
  This is `volatile`'s second legitimate use (the first was chapter 71's
  `volatile sig_atomic_t`). Set the three side by side and the purpose is clear.

  #dtable(
    columns: 2,
    [*Place*], [*What it prevents*],
    [MMIO and hardware registers], [Optimisations that erase or merge reads and writes],
    [Flags exchanged with a signal handler], [Optimisations that skip the read in a loop],
    [Locals that cross a `longjmp`], [Optimisations that keep the value only in a register],
  )

  All three prevent "the compiler bypassing memory". Conversely, *`volatile` is
  useless for synchronization between threads* — that place belongs to
  `<stdatomic.h>` (chapter 74). Miss this distinction and `volatile` becomes a
  misunderstood charm against all evils.
]

== Nobody cleans up the resources

`longjmp` only rolls back the stack pointer. Whatever was taken in between stays
taken.

#antipattern("Code that leaves resources where it jumped over")[
  ```c
  void work(void) {
      FILE *f = fopen("data", "r");     /* opened */
      char *buf = malloc(1024);          /* taken */
      parse(f, buf);                     /* if a longjmp happens here… */
      free(buf);                         /* does not run — a leak */
      fclose(f);                         /* does not run — the file leaks too */
  }
  ```
  If a `longjmp` happens inside `parse`, `work`'s frame simply vanishes. Neither
  `free` nor `fclose` runs. In a long-running program this pattern shows up as a
  small leak per request.
]

So code that uses this device keeps one discipline without exception — *gather
the places that take and release resources inside the function that called
`setjmp`.* Having jumped back, that function can clean up itself.

=== The quieter accident — a pointer reverts to its old value

The previous counter-example was a `free` *that never ran*. There is a quieter
one. The cleanup code runs perfectly well, but *the pointer saying what to free*
falls foul of the previous section's rule and reverts to its old value.

#demo("examples-en/ch72/jmp_leak.c")

At `-O0` both survive, but build the same code at `-O1` or above and the
non-`volatile` one reverts to the null it held at `setjmp` (we checked). Then
`free(risky)` frees null, does nothing, and 64 bytes leak for good — clear the
static copy and ASan reports it as "Direct leak of 64 byte(s)".

*There is cleanup code, and it still leaks.* Reading the code will not show it,
and it appears only in the optimised build that ships. Of the accidents in this
pattern it is the hardest to diagnose.

There is an opposite direction too. If the old value is not null but *an address
already freed*, it is a double free.

#antipattern("Code that reverts to an old address and frees it twice")[
  ```c
  char *p = malloc(64);
  if (setjmp(env) == 0) {
      free(p);            /* freed once */
      p = malloc(128);    /* and taken afresh */
      work(p);            /* a longjmp happens here */
  }
  free(p);                /* if p reverted, this frees what was already freed */
  ```
  Chapter 42's double free, reproduced exactly. The allocator's ledger is wrecked,
  so every allocation after it is contaminated.
]

The discipline is one line — *do not keep a pointer that must survive a `longjmp`
in an automatic local.* Give it `volatile`, or put it in a static variable or a
context struct. The `load_image` of the next demonstration does exactly that.

#demo("examples-en/ch72/jmp_error.c")

The demonstration's `load_image` is that structure. The `malloc` happens *before*
the `setjmp`, and the `free` sits once, in a place both the success and the
failure path pass through. The deep `parse_header` and `parse_size` take no
resources and only jump.

== What is actually built on this

The device is not recommended for new code, but *what is already built* is
plentiful.

#dtable(
  columns: 3,
  [*What*], [*How it uses it*], [*Why it did so*],
  [libjpeg], [A `jmp_buf` inside `struct jpeg_error_mgr`; on error it jumps from `error_exit`], [Decoding is deep recursion, so a return-value chain would be far too long],
  [libpng], [The same pattern through the `png_jmpbuf(png_ptr)` macro], [The same reason — imitating exceptions in a C API],
  [The Lua interpreter], [`LUAI_THROW`/`LUAI_TRY` are implemented with `longjmp` in a C build], [A script's `error()` must be carried into the host language],
  [Some coroutine implementations], [Save the context with `setjmp` and swap stacks], [The only road to imitating context switching with the standard alone],
  [Test frameworks], [On a failed assertion, abandon that test and move to the next], [One test's failure must not stop the whole run],
)

#realcase("libjpeg's error manager — how it came to look like this")[
  Code using libjpeg almost always begins like this.

  ```c
  struct my_error_mgr { struct jpeg_error_mgr pub; jmp_buf setjmp_buffer; };

  static void my_error_exit(j_common_ptr cinfo) {
      struct my_error_mgr *err = (struct my_error_mgr *)cinfo->err;
      longjmp(err->setjmp_buffer, 1);
  }
  ```

  The library's default behaviour was *to `exit` on error*. That amounts to a
  library killing the application — one broken image and the whole editor
  terminates. So libjpeg left open a hook for "the function to call on error", and
  from inside that hook the only way back was `longjmp`.

  What this case shows is the order of the design. *A deep call chain + a C API +
  a library that must not kill the process* — when those three coincide, there was
  hardly any choice besides `setjmp`/`longjmp`. Designed afresh today one would
  take chapter 48's "failure as a value", but within the constraints of the 1990s
  it was a reasonable decision.
]

=== How do they cope with memory — the answer is always the same

Read the previous table again, this time asking only "who frees the memory taken
before the jump?"

#dtable(
  columns: 3,
  [*What*], [*Who frees it*], [*The device*],
  [libjpeg], [One call to `jpeg_destroy_*`], [The library ties every allocation into pools of its own memory manager],
  [libpng], [One `png_destroy_read_struct`], [The same way],
  [Lua], [The garbage collector], [Every allocation is registered in the interpreter's state],
  [PostgreSQL], [Resetting a memory context where the error is caught], [An arena per query and per transaction],
  [Test frameworks], [Wholesale, when a test ends], [An arena per test, or separate processes altogether],
)

*The common thread is visible — they are all arenas.* The flaw that `longjmp`
cleans up nothing was worked around by a structure in which *no individual `free`
is needed*: tie everything taken into one bundle, and at the place you jump back
to, throw away that one bundle.

So one more conclusion attaches. *Use `setjmp` without an arena and a leak is a
matter of time.* Part XII's arena is also the precondition that makes this
pattern legitimate.

#realcase("PostgreSQL's `PG_TRY`/`PG_CATCH`")[
  PostgreSQL builds something very like exceptions out of this device.
  `PG_TRY` and `PG_CATCH` are macros, and inside them are `sigsetjmp` and a global
  exception stack. Reporting an error deep down jumps to the nearest `PG_CATCH`.

  But a single query takes thousands of pieces of memory. How is that coped with?
  *Memory contexts.* Every allocation belongs to some context, and resetting that
  context where the error is caught makes thousands of pieces vanish at once. An
  arena used as if it were a language feature.

  And the previous section's rule is written into that source as a convention —
  *a local variable modified inside a `PG_TRY` block and used in the `PG_CATCH`
  must be declared `volatile`.* It is precisely the list of what a large C program
  needs in order to use this device safely: one arena, one `volatile` convention,
  and macros around the jump so that people never write `longjmp` themselves.
]

#qa[
  What happens to `alloca` and variable-length arrays?
][
  Those alone are cleaned up automatically. They were taken from the stack, so
  when the stack pointer rewinds they go with it — the VLAs of the skipped frames
  do not leak.

  The standard does nail down one thing, though. *A `longjmp` that would leave the
  scope of an identifier with a variably modified type is undefined behaviour* —
  the implementation loses its chance to tidy that place. So the rule becomes "do
  not jump across a block in which a VLA is alive."

  From the memory point of view it comes to this — *what was taken from the stack
  is cleaned up for free; what was taken from the heap is cleaned up by nobody.*
  That one line is why all the libraries above went to arenas.
]

== POSIX's `sigsetjmp`/`siglongjmp`

Unix-like systems have one more pair.

```c
int  sigsetjmp(sigjmp_buf env, int savemask);
[[noreturn]] void siglongjmp(sigjmp_buf env, int val);
```

The difference is whether the *signal mask* (chapter 71) is saved along with the
rest. If `savemask` is non-zero the current mask is saved too, and `siglongjmp`
restores it.

Why is that needed? Consider code that escapes from a signal handler with
`longjmp`. While the handler runs that signal is blocked, and jumping out with the
standard `longjmp` may *leave it blocked* — after which that signal never arrives
again. `sigsetjmp` closes that hole.

#misconception[
  "Escaping from a signal handler with `longjmp` is fine"
][
  A widely used but dangerous pattern. Two things compound.

  *First, the mask problem* — exactly as above. On Unix the `sigsetjmp` pair must
  be used.

  *Second, and more fundamentally, you do not know where it was cut.* A signal
  arrives in the middle of `malloc` or `printf` too (chapter 71). Jump out from
  there and that data structure stays half-rewritten. Call `malloc` again after
  jumping out and it collapses then.

  So the places where the pattern is even defensible are narrow — *finish
  immediately after jumping* (`_Exit`), or take a path that uses the standard
  library no further. In a long-running program the pattern of "set a timeout with
  a signal and escape with `longjmp`" runs well on the surface and collapses
  rarely.
]

== Its place today — when to use it, what to use instead

#dtable(
  columns: 2,
  [*What you want to do*], [*The recommended way*],
  [Carry a deep failure upward], [Return it as a value (chapter 48). Tedious for the middle layers, but safer],
  [Tidy several failure paths inside one function], [Gather them with a single `goto cleanup` — the Linux kernel's practice],
  [Not forget to release resources], [By *structure*, not by attributes — take and release in the same function],
  [A deep escape in a parser or interpreter], [One of the few legitimate places for `setjmp`, if the resource discipline is kept],
  [Escape across threads], [Impossible — `longjmp` works only within one thread],
)

The `goto cleanup` pattern is worth writing out again, because most of what people
reach for `setjmp` to do is in fact covered by it.

```c
int work(void) {
    int rc = -1;
    FILE *f = fopen("data", "r");
    if (!f) goto out;
    char *buf = malloc(1024);
    if (!buf) goto close;
    if (parse(f, buf) != 0) goto free_buf;
    rc = 0;
free_buf: free(buf);
close:    fclose(f);
out:      return rc;
}
```

*Within one function* this is better. The flow is visible, the order of cleanup is
written in the code, and the compiler checks it. `setjmp` is needed only when
*several functions must be skipped over*.

#recap[
  #dtable(
    columns: 2,
    [*What to remember*], [*The point*],
    [What it is], [An escape hatch that rewinds the stack whole — not an exception],
    [`setjmp`], [A macro. 0 when called directly, `val` when jumped to (0 becomes 1)],
    [Context restriction], [Controlling expression, comparison with a constant, `!`, expression statement — four only],
    [`longjmp`'s premises], [The saving function is still alive, and it is the same thread],
    [`jmp_buf`], [An array type. Eight callee-saved slots plus room for a signal mask],
    [Registers], [Only callee-saved ones are restored → stack locals live, register locals revert],
    [Pointers], [`free` through a reverted pointer means a leak or a double free — measured],
    [The `volatile` rule], [Automatic, non-`volatile`, changed in between → no guarantee (measured)],
    [Resources], [Nobody cleans up — keep taking and releasing in one function],
    [Today's choice], [Mostly return values and `goto cleanup`. Legitimate only in deep parsers],
  )
]

We have seen the two devices that cut a flow and jump — the signal that comes from
outside, and the non-local jump that leaps from within. The next chapter is what
recent standards added, and the long argument over "safe" functions.
