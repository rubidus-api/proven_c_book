#import "../../book/lib.typ": *

= Errors are values

#organizer[
  We see the answer to chapter 69's second bug — unconfirmed failure. The way of
  returning failure as a value, the bundle holding a value and an error together,
  and the device that makes the compiler protest if an error is thrown away. The
  discipline set up in chapter 45, "errors are values", hardens here into a type.
]

#deepqa[
  Chapter 45 said C's ways of reporting an error are only three — the return value,
  global state, and stopping the program — and that of these the return value is the
  most honest. Then how does one say both "it failed" and "here is the result" with a
  single return value?
][
  There are three ways. Mix an *impossible value* into the result's place (null,
  `-1` — that trap seen in chapter 69), take the result out as an *output parameter*
  and use the return value for status alone, or *hold both in one struct* and return
  them together. proven uses the second and third together — and the third is
  possible because, as learned in chapter 41, a C struct can be returned by value.
]

== Two shapes of return

The rule is simple. *A function that can fail must return the failure as a value.*

- If there is no result to return, it returns a single `proven_err_t`.
- If there is a result to return, it returns an `{err, value}` bundle — with a name
  per type, such as `proven_result_u8str_t` or `proven_result_size_t`.

`proven_err_t` is an enumeration and success is `PROVEN_OK` (0). Failures have a
name per kind — `PROVEN_ERR_NOMEM` (out of memory),
`PROVEN_ERR_OUT_OF_BOUNDS` (out of room), `PROVEN_ERR_NOT_FOUND`,
`PROVEN_ERR_INVALID_ARG`, `PROVEN_ERR_IO`, `PROVEN_ERR_EOF` and so on. The check is
always the single `proven_is_ok(err)`.

#demo("examples/ch71/errval.c")

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
  chapter 69).
]

The output of the second call compresses this part's theme. On trying to put
`"Hello, world"` into a capacity of 8 bytes, the library, *instead of putting in as
much as fits and declaring success*, wrote nothing and returned a failure. It is the
exact opposite choice from `snprintf`'s quiet truncation seen in chapter 69.

== Throw it away and the compiler protests

Returning the error as a value is not enough by itself. As seen in chapter 69, a
return value *can be thrown away*. So functions for which failure is meaningful have
C23's #idx("nodiscard")`[[nodiscard]]` attached (we saw the name in chapter 45).
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
  But chapter 70's `proven_println` had no such mark. Why is screen output alone an
  exception?
][
  Because contracts have grades too. The failure of a write going to the console has
  conventionally been ignored (have you ever seen code that checks `printf`'s return
  value?), and making every line of output carry a `(void)` would bury the code in
  noise. So this library placed output in the grade that *returns the error but does
  not compel a check*. Conversely, the functions on the *input* side do have the mark
  — ignore the failure of a read and you treat "data not read" as though it had been
  read, replaying chapter 69's second bug exactly. Where to attach the mark is itself
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

== When there is nobody to return to — the panic

To return an error as a value there must be *somebody to return it to*. But in a
place where the contract itself is broken there is no such somebody — if, for
example, a null arrived in a place where there is no reason whatever to pass a null,
that is not a failure but means *the program's logic is already wrong*.

For such places the library has a panic path. It is the same spirit as chapter 45's
`assert`, differing in that the way it is handled can be swapped out so as to be
usable in embedded work too (chapter 78). The distinction is best remembered like
this.

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
  chapter 69's table from the same direction.
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
