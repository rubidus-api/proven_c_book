#import "../../book/lib.typ": *

= The structure of a program

#prereq(
  ([chapter 15, Hello world], [what hello world already contained]),
  ([chapter 13, Compiler optimisation], [the contract of the abstract machine]),
)

#organizer[
  Part IV has one goal — to read one piece of chapter 15's hello world
  *completely*. The first step is the skeleton of a program: comments,
  statements, blocks, and why the `#include` line is not a statement. By the
  end of this chapter the outward appearance of C source is no longer strange.
]

#deepqa[
  Chapter 4 said the machine is a repetition of "fetching the next thing to do
  from memory and doing it." Then what, on the source-code side, corresponds to
  one of those things to do?
][
  The *statement*. The body of a C program is a list of statements, and
  execution is working through that list from top to bottom, one statement at a
  time — chapter 4's model of computation reflected directly in source code. Of
  course, as chapter 13 taught, the compiler may rearrange the inner business;
  but the *observable result* is guaranteed to be the same as "one statement at
  a time, top to bottom." So the reader may safely read with this picture.
]

#chapter-questions()

== Comments — writing only humans read

Before looking at the skeleton, let us clear away what is *not* skeleton. Source
code may contain text the compiler entirely ignores — *comments*. There are two
notations:

```c
// this notation makes the rest of the line a comment
/* this notation makes everything between the opening and closing a comment —
   it can span several lines */
```

To the machine a comment does not exist; to a human it is documentation. The
examples in this book carry their explanations as comments — read them along
with the code.

== Statements — one step, and the semicolon

A *statement* is one step of a program. The `printf(...)` line and the
`return 0` line of chapter 15 are each one statement. And in C the end of a
statement is marked by a *semicolon* `;` — the full stop of our writing.

One important rule here. *In C a line break is not the end of a statement.*
Where a statement ends is decided solely by the semicolon; line breaks and
indentation are entirely tidiness for human eyes. A language like this is called
*free-form*.

#qa[
  Why not take the end of a line as the end of a statement? Typing a semicolon
  every time is a nuisance.
][
  Because of long statements. When one statement grows long and you want to
  write it over several lines, a language in which "end of line = end of
  statement" needs an exception device such as a continuation marker. C took the
  other side — mark the end explicitly with a semicolon and leave line breaks
  entirely free. Remember chapter 10's punched cards and the flavour deepens:
  passing through an era whose physical constraint was "one card = one line",
  languages evolved towards making meaning independent of the line as a form.
]

#misconception[
  "Writing one statement per line is a rule of C"
][
  Plausible — well-written code mostly looks like that. But that is *practice*,
  not a rule. Grammatically you may crowd several statements onto one line, or
  spread one statement over five. To the compiler they are all the same program.
  And yet the practice of "one statement per line, indent inside a block" has
  survived half a century for a simple reason — code is read far more often by
  humans than by machines. This book's examples follow that practice too:
  distinguishing the freedom the grammar allows from the restraint the practice
  advises is itself part of studying C.
]

== Blocks — a bundle of statements

Several statements wrapped in braces `{ }` are a *block*. The fence after `main`
in chapter 15 was exactly that — the list of things `main` does, bound into one.
A block is a grammatical bracket meaning "from here to here is one bundle", and
nearly every structure we meet from now on (conditions, loops, functions) uses
blocks as a component. For now "the packaging unit of a statement list" is
enough.

== Lines that are not statements — the world of `#`

Now the last secret of chapter 15's first line. There is *no* semicolon at the
end of `#include <stdio.h>`. If it were a statement there ought to be a full
stop — and the reason there is none is simple. *It is not a statement.*

Recall the relay of chapter 16. A line beginning with `#` is speech addressed to
the first runner, the *preprocessor*, and the preprocessor is a text tool that
works not by C's grammar but by *lines*. So the grammar of a `#` line is
entirely different from C's statement rules — the end of the line, not a
semicolon, ends the directive, and by convention they live gathered at the top
of the file. There are in fact *two languages* inside one source file (the
language of preprocessing directives, and C). Strange, but for us who know the
history the reason is visible — the preprocessor was born as a separate worker
and has remained one.

== Putting it together — a two-statement program

Here is a variation containing all this chapter's material. Only one thing
changed from chapter 15's hello world — there are now two statements.

#demo("examples-en/ch19/two.c")

Notice that the output is one line. There are two statements and two steps —
but since the first statement has no `\n`, on the band of output (chapter 10)
the letters simply join up. *What breaks a line is not a statement but `\n`* —
the boundaries of statements and the line boundaries of output are unrelated.
That is the lesson of this demonstration.

The skeleton of hello world's six lines is now readable — `#include` (a
preprocessing directive), `main` and its fence (a block), statements and
semicolons, and comments. What remains is the substance *inside* the fence:
what exactly happens inside the statement `printf("...")`. The next chapter
enters that inner world — the things that become values, *expressions*.
