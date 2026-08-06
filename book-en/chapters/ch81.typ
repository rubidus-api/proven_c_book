#import "../../book/lib.typ": *

= Errors are values

#prereq(
  ([chapter 48, Errors and contracts], [errors as values]),
  ([chapter 79, The five bugs shipped for fifty years], [the unchecked return value]),
)

#deepqa[
  Chapter 48 said C's ways of reporting an error are only three — the return value,
  global state, and stopping the program — and that of these the return value is the
  most honest. Then how does one say both "it failed" and "here is the result" with a
  single return value?
][
  There are three ways. Mix an *impossible value* into the result's place (null,
  `-1` — that trap seen in chapter 79), take the result out as an *output parameter*
  and use the return value for status alone, or *hold both in one struct* and return
  them together. proven uses the second and third together — and the third is
  possible because, as learned in chapter 43, a C struct can be returned by value.
]

#organizer[
  We see the answer to chapter 79's second bug — unconfirmed failure. The way of
  returning failure as a value, the bundle holding a value and an error together,
  and the device that makes the compiler protest if an error is thrown away. The
  discipline set up in chapter 48, "errors are values", hardens here into a type.
]

#chapter-questions()

== Two shapes of return

The rule is simple. *A function that can fail must return the failure as a value.*

- If there is no result to return, it returns a single `proven_err_t`.
- If there is a result to return, it returns an `{err, value}` bundle — with a name
  per type, such as `proven_result_u8str_t` or `proven_result_size_t`.

`proven_err_t` is an enumeration and success is `PROVEN_OK` (0). The check is always
the single `proven_is_ok(err)` — writing `err == 0` would work too, but using the name
lets the code survive a later change of representation.

=== Every error code

Failures have a name per kind, and there are sixteen in all. They are not to be
memorised; it is enough to know *what branches exist* — most code divides only success
from failure, and looks at the branch only when attempting recovery.

#dtable(
  columns: 3,
  [*code*], [*what happened*], [*mainly where*],
  [`PROVEN_OK`], [success (0)], [—],
  [`ERR_NOMEM`], [the allocator could not hand out memory], [`_create`, `_grow`],
  [`ERR_OUT_OF_BOUNDS`], [outside the vessel — refused rather than truncated], [`append`, `slice`, array indexing],
  [`ERR_INVALID_ENCODING`], [the UTF-8/UTF-16 was broken], [string conversion, `hex`/`base64`],
  [`ERR_INVALID_ARG`], [an argument is outside the contract (null, 0, an unusable allocator)], [almost every entry point],
  [`ERR_IO`], [the outside world failed], [files and streams],
  [`ERR_NOT_FOUND`], [what was sought is not there], [map lookup, opening a file],
  [`ERR_INVALID_STATE`], [it cannot be done in the present state], [a closed stream, a destroyed object],
  [`ERR_NEED_MORE`], [more input is needed before judging], [parsers and decoders],
  [`ERR_OVERFLOW`], [a size calculation overflowed], [`create`, container growth],
  [`ERR_UNSUPPORTED`], [this environment does not have that facility], [OS features under freestanding],
  [`ERR_AGAIN`], [not now — try again], [non-blocking I/O],
  [`ERR_EOF`], [the end was reached], [reading],
  [`ERR_BUSY`], [somebody else is using it], [locks, the job queue],
  [`ERR_PERMISSION`], [there is no permission], [files],
  [`ERR_INVALID_FORMAT`], [the format was wrong], [parsing, format strings],
)

#demo("examples-en/ch81/codes.c")

The latter part of the example shows this table in the flesh. Try to put twelve bytes
into an eight-byte vessel and `OUT_OF_BOUNDS` comes — *and the original is left
untouched* (the length is still 0). Give an unusable allocator and it is caught as
`INVALID_ARG` before anything is made. Slicing outside the range too is a refusal, not
"as much as there is".

Two things are worth taking from here. First, *`ERR_INVALID_ARG` is usually a bug in my
own code* — not a failure of the outside world but a contract violation, so it is to be
mended rather than recovered from. Second, `ERR_EOF` and `ERR_AGAIN` are *part of the
normal flow*. In a reading loop EOF is not an error but the ending condition
(chapter 87).

=== The kinds of result bundle

A function with a value to return has one bundle per type. The naming rule being the
same, the list need not be memorised — inside a `proven_result_XXX_t` there are always
just `err` and `value`.

#dtable(
  columns: 3,
  [*bundle*], [*the type of `value`*], [*where it is returned*],
  [`proven_result_size_t`], [`proven_size_t`], [lengths, counts, bytes written],
  [`proven_result_mem_mut_t`], [`proven_mem_mut_t`], [allocators (chapters 82 and 83)],
  [`proven_result_mem_view_t`], [`proven_mem_view_t`], [slicing (chapter 82)],
  [`proven_result_u8str_t`], [`proven_u8str_t`], [making a string (chapter 84)],
  [`proven_result_buf_t`], [`proven_buf_t`], [making a buffer],
  [`proven_result_cstr_t`], [`const char *`], [exporting as a C string (chapter 84)],
  [`proven_fmt_result_t`], [(amount written and amount needed)], [formatting (chapter 85)],
)

Only the last row is of a different grain. For formatting, "success or failure" is not
enough — if it was truncated you must know *how much more was needed* — so beside `err`
it carries two numbers as well (we look at it closely in chapter 85).

#demo("examples-en/ch81/errval.c")

This example contains all of this chapter's syntax. `make_greeting` sends the result
out through an output parameter (`out`) and used the return value for status alone —
on failure it passes it up as it is. And `proven_u8str_create` returns a bundle, so
`made.value` is taken out *only after checking*. The order must not be reversed.

#antipattern[
  Taking out `value` before checking
][
  ```c
  proven_u8str_t s = proven_u8str_create(alloc, 64).value;   /* dangerous */
  ```
  It finishes in one line and looks clean, but what comes into your hand on failure
  is *a meaningless value*. The bundle's contract is "`value` has meaning only when
  `err` is `PROVEN_OK`", so this code has skipped the contract. That on failure a
  struct filled with zeros usually arrives and it does not die immediately is rather
  the danger — the accident is put off until much later (that pattern from
  chapter 79).
]

The output of the second call compresses this part's theme. On trying to put
`"Hello, world"` into a capacity of 8 bytes, the library, *instead of putting in as
much as fits and declaring success*, wrote nothing and returned a failure. It is the
exact opposite choice from `snprintf`'s quiet truncation seen in chapter 79.

== Throw it away and the compiler protests

Returning the error as a value is not enough by itself. As seen in chapter 79, a
return value *can be thrown away*. So functions for which failure is meaningful have
C23's #idx("nodiscard")`[[nodiscard]]` attached (we saw the name in chapter 48).
Throw the result away and the compiler really says this.

```text
warning: ignoring return value of ‘proven_u8str_append’,
         declared with attribute ‘nodiscard’ [-Wunused-result]
    5 |     proven_u8str_append(&s, proven_u8str_view_from_cstr("hi"));
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
note: declared here
  123 | [[nodiscard]] proven_err_t proven_u8str_append(...);
```

If the `-Werror` recommended in chapter 17 is turned on, this is not a warning but a
*build failure*. What was "checking is optional" has become "it does not compile
unless you check".

Of course there are times when you really do want to ignore it. Then `(void)` is put
in front.

```c
(void)proven_u8str_append(&s, view);   /* ignored knowingly */
```

#qa[
  If it can be ignored with `(void)`, is it not compulsory after all?
][
  Because the purpose of the compulsion is not "to prevent ignoring" but *"to make
  ignoring visible"*. An error thrown away with no mark is invisible in code review,
  while a line with `(void)` attached becomes a declaration that "this failure is
  deliberately ignored". The very fact that it must be typed is the heart of it — it
  cannot be done by accident, only on purpose.
]

#qa[
  But chapter 80's `proven_println` had no such mark. Why is screen output alone an
  exception?
][
  Because contracts have grades too. The failure of a write going to the console has
  conventionally been ignored (have you ever seen code that checks `printf`'s return
  value?), and making every line of output carry a `(void)` would bury the code in
  noise. So this library placed output in the grade that *returns the error but does
  not compel a check*. Conversely, the functions on the *input* side do have the mark
  — ignore the failure of a read and you treat "data not read" as though it had been
  read, replaying chapter 79's second bug exactly. Where to attach the mark is itself
  a design judgement.
]

== What remains after a failure — failure atomicity

There is a question that naturally arises after receiving an error. *What state is
the object the failed function was touching in now?*

#idx("failure atomicity")The library's answer is *failure atomicity* — unless the
documentation says otherwise, a failed operation leaves the target in the state it
was in before being touched. If memory runs short while growing an array the existing
elements are still alive, and if there is not enough room while appending to a string
the original content stands. The second call of the example just now is that case —
it failed, but no half-written string was left.

Why does this matter? Without failure atomicity a caller can do nothing after a
failure *but throw the object away*. With it, "give up this addition and carry on
with what has been gathered so far" becomes possible.

#misconception[
  "If it fails we will end the program anyway, so what does the state matter"
][
  For a short-running command-line tool that may be so. But long-running programs —
  servers, editors, games, firmware — must not die on one failure. If the whole server
  went down because handling one request failed for lack of memory, that would be the
  greater accident. Failure atomicity is the minimal condition that makes possible the
  recovery of "throw away only this request and take the next".
]

== Raising a failure upward — together with the cleanup

Receiving errors as values raises one practical problem at once. *If it fails in the
middle, who gives back what has been taken so far?* In a language with exceptions the
stack unwinds and destructors handle it, but C has no such device (chapter 70). So an
idiom is needed.

#demo("examples-en/ch81/cleanup.c")

This example deliberately inserts a failing allocator (once the *budget* runs out it
necessarily gives `NOMEM`) and runs all three cases — failure from the first
allocation, one taken and failure at the second, and everything succeeding. The middle
case is the heart of it. `x` has already been taken while `y` failed, so simply
returning here is *a leak*.

The pattern comes to three.

+ *Mark what you hold with a flag* — one boolean such as `has_x`. If the resources are
  several, so are the flags.
+ *On failure everything gathers at one place* — `goto done`. That use chapter 70
  called "disciplined `goto`".
+ *Clean up in reverse order of taking* — what was taken later is given back first.

It is worth noticing too that after ownership passes with `*out = y;` on the success
path, `y` is not destroyed thereafter. *You must be able to point at the place where
ownership passes with a single line of code* — a function that cannot is usually one of
blurred design.

#qa[
  Is deliberately making a failing allocator of any use in practice too?
][
  Of great use. The out-of-memory path is almost never executed in a real program, so
  in most codebases it is *the least tested path*. And a leak or double free there is
  the hardest of all to diagnose.

  As chapter 83 will show, an allocator is simply a value, so a shell that "fails from
  the nth call" can be made in ten lines, as in the example, and inserted. Raise n from
  1 and run the tests and you can pass through *every failure point* once, and running
  it with ASan or Valgrind (chapter 17) makes that path's leaks show themselves plainly.
  It is the place where the decision that the library does not call `malloc` directly
  comes back as testability.
]

== When there is nobody to return to — the panic

To return an error as a value there must be *somebody to return it to*. But in a
place where the contract itself is broken there is no such somebody — if, for
example, a null arrived in a place where there is no reason whatever to pass a null,
that is not a failure but means *the program's logic is already wrong*.

For such places the library has a panic path. It is the same spirit as chapter 48's
`assert`, differing in that the way it is handled can be swapped out so as to be
usable in embedded work too (chapter 88).

There are only two doors.

```c
void proven_panic(const char *msg);                       /* raise a panic */
void proven_set_panic_handler(proven_panic_handler_t h);  /* swap the handler */
```

The default handler *does not return* — it stops the program on the spot (the
implementation is `__builtin_trap()`). And this swapping is what pays in embedded work.
On a board with no console there is nowhere to print a message, so a handler is
registered that lights an LED, kicks the watchdog, or reboots.

```c
static void my_panic(const char *msg) {
    (void)msg;
    board_led_on(LED_FAULT);
    for (;;) { }          /* it does not go back */
}
/* at the program's starting place */
proven_set_panic_handler(my_panic);
```

*A handler must not return.* If it returns, the validity of what an `_or_panic`
function gave back is not guaranteed — a panic is the declaration that "from here the
program's premises are broken". The exception is test code deliberately using a
returning handler to confirm the panic path, and even then the value after it is not
used.

The places where the library itself calls a panic can be counted on the fingers — the
functions with `_or_panic` in the name (chapter 83's arena allocation is
representative) and a few places where the contract is plainly already broken. Everything
else is returned as a value.

The distinction is best remembered like this.

#recap[
  #dtable(
  columns: 3,
    [*situation*], [*example*], [*the library's handling*],
    [failure of the outside world], [out of memory, file not found, out of room], [return the error as a value],
    [the caller's contract violation], [a null that must not be, reusing a destroyed object], [panic (or undefined)],
    [failure that may be ignored], [console output failure], [return the error but do not compel],
)
]

#realcase[
  Other languages that chose errors as values
][
  This design is not C's invention alone but a current common to recent systems
  languages. Go has functions return a result and an error side by side, and Rust
  wraps success and failure in the single `Result` type and warns if it is ignored.
  Both are languages that decided not to use exceptions, and the reason is the same —
  *the error paths must be visible in the shape of the code*. Exceptions are
  convenient but erase from the signature "which failure jumps where from here".
  Three different languages, in effect, found the answer to the second row of
  chapter 79's table from the same direction.
]

#qa[
  What is the price of this way?
][
  `if`s multiply. There being no device like exceptions to sweep a deep failure up in
  one go, code that checks and passes upward attaches at every place a failure is
  met. That is half the reason the `make_greeting` of the example just now runs to
  some twenty lines. In exchange one thing is gained — *where and what can fail is
  visible in the code as it stands.* That there is no hidden failure is the thing
  this library sells.
]

Knowing the shape of errors, we now go down to what those errors protect — memory
itself. The next chapter is bytes and views, and size calculation that does not
overflow.
