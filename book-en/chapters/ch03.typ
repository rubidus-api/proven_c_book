#import "../../book/lib.typ": *

= Programs and processes — what it is to be run

#prereq(
  ([chapter 2, The regions of memory], [the map that divides memory into code, static, stack and heap]),
)

#deepqa[
  Chapter 2 said a program's memory divides into code, static, stack and heap. But
  when, and by whom, are those regions prepared?
][
  At *the moment the program is run*, mostly by the operating system. The executable
  file lying on the disk has no memory yet — it is merely a blueprint saying "the code
  is like this, and the static region needs this much". Reading that blueprint,
  spreading out the real memory, setting up the workbench, and leaping to the first
  instruction — that is running, and one set brought to life that way is called a
  *process*.
]

#organizer[
#idx("process")  We divide the *program* that exists as a file from the *process*
  that has been run and is alive. What the operating system gives a process (an
  address space of its own, open files, an exit status), and how a process is born —
#idx("fork")  the Unix family's `fork` and Windows' way — seen briefly. *What
  appears here is not the C standard.* It is also the chapter that first draws the
  boundary between what the standard promises and what the operating system gives.
]

#chapter-questions()

== A program is a noun, a process is a verb

The distinction is simple.

- *Program* — a file lying on the disk. Dead. It can be copied and moved.
- *Process* — one set run and alive. It has memory, has a position it has reached,
  has open files, ends one day and leaves an *exit status*.

Run the same program three times and three processes arise. The three share the same
code but *their memory is each their own* — change a value on one side and the other
does not know.

#dtable(
  columns: 3,
  [*what the OS gives a process*], [*what it is*], [*in this book*],
  [address space], [a memory map of its own (chapter 2's four regions)], [chapter 71],
  [execution position], [which instruction it is in the middle of], [—],
  [the list of open files], [the passages including standard input and output], [chapters 10 and 58],
  [command-line arguments and environment], [the values handed over at running], [chapter 50],
  [exit status], [the one number it leaves as it ends], [chapters 15 and 50],
)

Two things in this list connect straight to later chapters. First, the *standard
input, output and error* we shall see in chapter 10 are passages the operating system
opened for the process in advance — it is not the program that opens them. Second, the
`0` that chapter 15's hello world returns as it finishes is exactly the *exit status*,
and the side receiving that number is whoever ran this process.

#qa[
  "Memory of its own" sounds odd — is there not only one memory attached to the
  machine?
][
  Physically there is one, but the operating system shows each process *a different
  map*. The addresses a process sees are not physical addresses but numbers of that
  process alone, and hardware moves them to the real places (virtual memory). So even
  if two processes use identical addresses they touch different places, and if one
  collapses the other is unharmed.

  There is a world without this isolation too — on a small chip running with no
  operating system there is no concept of a process at all, and one piece of code uses
  the whole machine (chapter 71). So this chapter's story is not the story of
  "everywhere C runs" but the story of *when it runs on an operating system*.
]

== How a process is born

From here it differs by operating system. We learn the faces of two branches.

#platform[
  The Unix family — `fork` and `exec`
][
  The Unix family (Linux, macOS, BSD) divides the making of a process into *two
  steps*. This design is the root of the shell and of pipelines (chapter 10).

  - *`fork()`* — it *duplicates* the present process. The duplicated side (the child)
    starts with the same code, the same memory contents and the same open files as the
    parent. The strange thing is that this function *returns twice* — to the parent it
    returns the child's number (its PID), to the child 0. From that return value each
    knows who it is.
  - *The `exec` family* — it *replaces the present process's contents with another
    program*. The memory is turned wholly into the new program's and it does not return
    (if it succeeds).
  - *The `wait` family* — the parent waits for the child to end and receives the *exit
    status*.

  ```c
  pid_t pid = fork();
  if (pid == 0) {               /* the child */
      execl("/bin/ls", "ls", (char *)0);
      _exit(127);               /* it comes here only if exec failed */
  } else if (pid > 0) {         /* the parent */
      int status;
      waitpid(pid, &status, 0);
  }
  ```

  Type one command in a terminal and the shell does exactly this — duplicates itself,
  replaces the duplicated side with that command, and waits for it to end. The
  redirection and pipes seen in chapter 10 happen in between as well: *after
  duplicating and before replacing*, the child's input and output passages are changed.
  Dividing the design into two steps is what made that place.

  Duplication does not mean copying the memory whole. Today's implementations *put it
  off until writing* (copy-on-write) — parent and child share the same place, and only
  the part one of them writes to is copied at that moment.
]

#platform[
  Windows — `CreateProcess`
][
  Windows has no `fork`. Instead it is *one step* — `CreateProcess` instructs in one
  go, "start this program as a new process". There being no duplication step, the
  child does not inherit the parent's memory, and what the parent wishes to hand down
  (open passages, the environment, the working directory) is stated in the arguments.

  The difference in character between the two ways shows itself in porting. The Unix
  pattern of "duplicate, mend a little, then replace" does not carry over to Windows as
  it is and must be rewritten in the form of *writing what is needed into the
  arguments*. On the POSIX side too there is `posix_spawn`, which makes this pattern
  one step.
]

#misconception[
  "Surely this sort of thing is in the C standard too"
][
  It is not. *Neither `fork` nor `exec` nor `CreateProcess` is C standard.* What the C
  standard says about processes is surprisingly little — about as much as that the
  program starts, `main` runs, and it ends leaving an exit status. How to make a new
  process, run another program, or wait for a child is not in the standard (there is
  `system` alone, settled only as "it executes a command string", and what that means
  is left to the implementation).

  Why so stingy — because C *must run on machines with no operating system too*
  (chapter 57's freestanding implementation). C must exist on chips that have no
  processes, so processes remained outside the standard, in the operating system's
  territory.

  This sense of the boundary matters through the whole book. *What is in the standard
  works everywhere; what is outside it works only on that operating system.* It is also
  why this book puts the stories tied to an operating system into separate "platform
  note" boxes.
]

#qa[
  Then what is the reason to know about processes now?
][
  Because three things become needed at once.

  First, where chapter 10's *streams* come from — standard input and output are
  passages already open when the process is born. Second, to whom the *exit status* `0`
  that chapter 15's hello world leaves goes. Third, that chapter 2's *four regions* are
  separate per process — saying a global variable is "one throughout the program" means
  *one within one process*.

  The deeper stories — programs running along several strands (threads), communication
  between processes, signals — are beyond this book's scope, but the pieces of them the
  C standard treats are met in their places (signals in chapter 65, threads and atomic
  operations in chapter 68).
]

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [program / process], [a file / one set run and alive],
    [what the OS gives], [an address space, open passages, an exit status],
    [memory is each its own], [run the same program several times and they do not know each other],
    [Unix], [two steps, `fork` (duplicate) + `exec` (replace)],
    [Windows], [one step, `CreateProcess`],
    [outside the standard], [processes are not the C standard's territory — the OS gives them],
  )
]

Part I is over. What kind of language C is and why we learn it now (chapter 1), how a
program divides its memory (chapter 2), and what it is to be run (chapter 3) — the
ground is laid.

The next part goes one layer below that ground. It begins by drawing the computer as
three parts, climbs the ladder of representation from bits to letters, and sees why
machines became so complicated.
