#import "../../book/lib.typ": *

= The three faces of `main` — entry point and exit status

#organizer[
#idx("main")  We face head on the `main` that chapter 14 passed over as merely
  "the agreed starting point". The *three forms* the standard permits, the exact
  contract of the command-line arguments `argc` and `argv`, and where the returned
#idx("exit status")  value goes and what it becomes — including the conventions
  of Linux, Windows and embedded targets. Why `void main()` is wrong is settled
  here too.
]

#deepqa[
  Chapter 3 said a process leaves one number, the *exit status*, as it ends, and
  chapter 14's hello world ended with `return 0;`. So who receives that 0, and
  what happens if a nonzero value is returned?
][
  The receiver is *whoever ran this program* — the terminal's shell, a build tool,
  a script, or another program that launched this one as a child. They judge "did
  it end well?" from that one number and decide what to do next. So `main`'s return
  value is not decoration but *the program's last conversation with the outside
  world*.
]

== The three forms the standard permits

The C standard pins `main`'s definition down like this — the return type must be
`int`, and the parameters must be *none*, or *two*, or *some other
implementation-defined manner*. Hence three faces.

#dtable(
  columns: 3,
  [*form*], [*when to use it*], [*status*],
  [`int main(void)`], [when command-line arguments are not used], [standard],
  [`int main(int argc, char *argv[])`], [when arguments are taken], [standard],
  [other forms], [extended arguments such as `envp`], [*implementation-defined*],
)

The second form's `char *argv[]` may equally be written `char **argv` (exactly
chapter 37's rule that array parameters decay into pointers). The names are free
too — `argc` and `argv` are only convention.

The representative third form is
`int main(int argc, char *argv[], char *envp[])`. Unix-family systems and Windows
commonly support it, but it is *not standard*, and the portable road to reading
environment variables is `getenv` (chapter 55).

#antipattern[
  `void main()`
][
  ```c
  void main(void) { … }        /* not standard */
  ```
  A notation often seen in old textbooks and Turbo C-era code. The standard pinned
  the return type to `int`, so this notation is *outside the contract in a hosted
  environment*. Many compilers warn about it and some treat it as an error.

  Only two exceptional circumstances need be known. First, in a *freestanding
  implementation* (chapter 53) the name and form of the starting function are the
  implementation's to decide, so there really are embedded compilers that
  officially support `void main(void)` — there being nowhere to receive the return
  value, they remove that code to save size. Second, that is *that compiler's
  promise*, not the standard's. In hosted code it is always `int`.
]

== The contract of `argc` and `argv`

#demo("examples-en/ch47/entry.c")

What the standard promises is as follows, and these promises form the skeleton of
argument-handling code.

+ `argc` is *zero or more*. There may be no arguments at all.
+ `argv[0]` is *the program name* — though if the name cannot be known it may be
  an empty string. So code that computes a path trusting `argv[0]` unconditionally
  is dangerous.
+ `argv[1]` through `argv[argc-1]` are the actual arguments.
+ *`argv[argc]` is necessarily a null pointer.* The example confirmed it. That is
  why traversal taking null as the end marker, as in
  `for (char **p = argv; *p; p++)`, works.
+ These strings *may be modified* and are valid while the program runs.

#qa[
  Must the code that interprets arguments be written by hand? Things like `-v`
  and `--help`.
][
  Two things must be divided first — *receiving the arguments* and *interpreting
  them* are different jobs.

  *Receiving them is in the standard.* The `argc` and `argv` just seen are that, and
  on any platform the arguments come in through these two parameters. Nothing is
  lacking in that place.

  *It is the parser that is absent.* Rules for *interpreting* — "`-v` means verbose,
  `--out FILE` is an option with a value attached, the rest are file names" — are not
  provided by standard C at all. It means there is no function like `getopt` in
  `<stdlib.h>`. So that work divides into three roads.

  *Write it yourself* — for a short program this is enough. Scan `argv`,
  distinguish with `strcmp`, and for arguments with an attached value read the next
  slot. To convert to a number use `strtol`, not `atoi` (chapter 55).

  *Platform tools* — the Unix family has POSIX's `getopt` (`<unistd.h>`) and GNU's
  `getopt_long` (which handles the `--name` form), and glibc has `argp`, which even
  builds the help text. All of them belong to the platform, not the standard.
  Windows' C runtime has no `getopt`, so porting projects mostly put one `getopt`
  implementation into the repository or use a parser of their own.

  *A library* — as the scale grows (subcommands, generated help, merging with a
  configuration file), use a library dedicated to argument parsing.

  Whichever road, keep one rule: *arguments are input from outside.* Chapter 39's
  rules for handling input — do not trust lengths, check the failure of numeric
  conversion, do not concatenate paths blindly — apply just the same.
]

#platform[
  How the arguments reach the program — Unix and Windows
][
  That `argv` is standard does not mean *the way it is made* is the same. The two
  worlds are opposites.

  *The Unix family* — the side launching the program passes *an array of strings* in
  the first place (chapter 3's `execve`). The shell handles quotes and wildcards
  first and cuts them into pieces, so the `argv` a program receives is already
  divided. The kernel carries that array over to the new process as it is.

  *Windows* — `CreateProcess` passes *one string* (chapter 3). That is, the dividing
  is the receiving side's part. So the C runtime, in its startup code, cuts that one
  line by rule and makes the `argv` it hands to `main` — `argc` and `argv` arriving as
  the standard says is because the runtime does that work for you.

  This difference leaves two things in practice. First, on Windows the *original
  command line* can be seen directly and cut by hand if needed — `GetCommandLineW`
  returns that one line and `CommandLineToArgvW` cuts it by the standard rules. To
  receive Unicode arguments intact, using `wmain` (or those two functions) is the
  practice. Second, *the cutting rules differ by platform* — the handling of quotes
  and backslashes especially. Hence the advice, when launching another program and
  building its arguments, to use APIs that *pass the arguments as an array rather than
  joining a string by hand* (`posix_spawn`, and `CreateProcess`'s argument-assembly
  rules) — the same grain as chapter 58's `system` counterexample.
]

== The value returned — three notations, one meaning

There are three ways to end `main`, and all three mean the same thing.

```c
return 0;              /* explicit */
return EXIT_SUCCESS;   /* the name from <stdlib.h>. its value is 0 */
}                      /* just ending — since C99 the same as return 0; */
```

The last is a special case introduced in C99. *For `main` alone*, ending without a
return value counts as having returned 0 (in other functions, not returning a
value and then using it is outside the contract). The standard belatedly ratified
what was common in C89-era code.

To report failure, use `EXIT_FAILURE`. As the example confirmed, the common value
is 1, but *the standard does not promise it is 1* — it is only "a nonzero value
meaning failure." Returning any other number is *implementation-defined*.

#dtable(
  columns: 3,
  [*way of ending*], [*what it does*], [*caution*],
  [`return n;` (in main)], [the same as `exit(n)`], [all the cleanup procedures run],
  [`exit(n)`], [end the program from anywhere], [runs `atexit` functions, flushes streams],
  [`quick_exit(n)`], [quick termination (C11)], [only `at_quick_exit` runs],
  [`_Exit(n)`], [immediate termination], [it does *not* clean up],
  [`abort()`], [abnormal termination], [no cleanup. a core dump may be left],
)

The example's `atexit` shows that cleanup procedure — the registered function ran
after `main` ended. Flushing open streams (chapter 55's buffers) is included in
it. So *ending with `_Exit` or `abort` can lose output.*

#misconception[
  "Any number at all can be returned as the exit status"
][
  You may return it, but *there is no guarantee the receiver sees it as it is.*
  Unix-family systems use only *the low eight bits* when conveying a child's exit
  status. So the status of a program that ended with `return 300;`, seen from the
  shell, is not 300 but 44 (300 − 256).

  ```text
  $ ./ex ; echo $?
  44
  ```

  Windows conveys a 32-bit exit code as it is, so it has no such truncation. The
  portable rule is one — *0 for success, 1 for failure, and otherwise only small
  numbers in the range 0–125.* Do not send large or negative numbers out as a
  status.
]

== Conventions — who reads that number, and how

#platform[
  Linux and the Unix family
][
  The shell keeps the last command's status in `$?`. The conventions are:

  #dtable(
    columns: 2,
    [*value*], [*meaning*],
    [0], [success],
    [1], [a general failure],
    [2], [a usage error (the convention of many tools)],
    [126], [found but not executable, for permission and similar reasons (shell)],
    [127], [command not found (shell)],
    [128 + N], [killed by signal N (the shell's notation)],
  )

  Thanks to these conventions a script can decide its flow from the status alone,
  as in `if ./program; then …`, and `make` and CI notice failure. The BSD family
  has a finer convention in `<sysexits.h>` (`EX_USAGE` 64 and so on), but it is not
  widely used.

  Caution: values of 128 and above are confusable with being killed by a signal, so
  it is better for a program not to return them itself.
]

#platform[
  Windows
][
  The same number is called the *exit code*, read as `%ERRORLEVEL%` in a batch
  file and `$LASTEXITCODE` in PowerShell. To receive a child's code from a program
  you use `GetExitCodeProcess`.

  Two differences. *It is not truncated to the low eight bits* (the full 32 bits),
  and certain values can be read as overlapping with system error codes. An
  ordinary program is still safest using 0 and 1.
]

#platform[
  Embedded — there is nowhere to return to
][
  On a machine with no operating system there is nobody to receive a status when
  `main` ends. So `main` in this world usually *does not end*.

  ```c
  int main(void)
  {
      init();
      for (;;) {            /* turns forever */
          poll();
      }
  }
  ```

  And if it does end, what happens is also the implementation's business — the
  startup code may hold it in an infinite loop, reset the chip, or let it wander
  anywhere. So embedded coding conventions often explicitly require that "`main`
  does not return." That the entry point may not even be called `main` is exactly
  as chapter 53 showed — the function the reset vector points at is the starting
  point, and that function copies `.data`, fills `.bss` with zeros and then calls
  `main` (chapter 67).
]

#realcase[
  The tool ecosystem built by one exit status
][
  It is interesting to see how much the single convention "0 is success" supports.
  The shell's `&&` and `||`, `make` stopping when one rule fails, CI judging a build
  failure, a container deciding a restart from a process's exit code, a test runner
  counting passes and failures — all of it reads this one number.

  The Unix philosophy of "joining small programs with text streams", seen in
  chapter 9, in fact had one more channel. Data flows through streams, and
  *success and failure flow through the exit status.* So making a program that
  behaves like a tool requires two things: results to standard output, error
  messages to standard error, and *success or failure always carried in the exit
  status.*
]

#qa[
  If `main` is a special function, may it be called recursively or have its
  address taken?
][
  In hosted C, `main` is grammatically an ordinary function, so both calling it and
  taking its address are syntactically possible. But *the norm is not to* — C++
  forbids it outright, and in C too it is a place entangled with startup code and
  library initialisation, with nothing to gain. It is better for the reader as
  well to treat the program's start as happening once.

  Know one thing instead. That `main` is the starting point is a statement from
  *the C program's point of view*. In reality, startup code (something like `crt0`)
  runs before it, preparing the static region and gathering the arguments before
  calling `main` (chapter 67). `main` is not "the first code that runs" but "the
  first of the code we write that runs."
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [three forms], [`int main(void)`, `int main(int, char *[])`, implementation-defined],
    [`void main()`], [not standard (embedded extensions are another matter)],
    [`argv[0]`], [the program name — it may be empty],
    [`argv[argc]`], [necessarily null. the end marker for traversal],
    [ways of ending], [`return`/`exit` (they clean up) / `_Exit`, `abort` (they do not)],
    [the C99 special case], [`main` returns 0 even when it ends without a value],
    [the range of values], [small numbers 0–125. Unix conveys only the low eight bits],
    [conventions], [0 success, 1 failure, 2 usage error (Unix)],
    [embedded], [`main` usually does not return],
  )
]

We have seen the program's beginning and end. Now we move on to the story of a
program *divided across several files* — the place where chapter 16's linker
appears again.
