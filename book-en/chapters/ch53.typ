#import "../../book/lib.typ": *

= Handling name collisions — from prefixes to `namespace`

#prereq(
  ([chapter 51, Many files — splitting and linking], [external and internal linkage]),
  ([chapter 52, The world of names], [four name spaces and three axes]),
)

#deepqa[
  Chapter 52 said C has only four name spaces and that you cannot make a new
  one. What happens, then, when two libraries that know nothing of each other
  each export a function called `init`?
][
  If both have external linkage the linker refuses with a *multiple definition*
  --- and that is the lucky case. The unlucky one is worse. If one of them sits
  inside a static library, the linker *takes the first one it finds and moves on
  in silence.* Compiling and linking both succeed, and the wrong function is
  called.

  So the answer to this problem is not "fix it when it collides" but *"make a
  collision impossible in the first place."* This chapter is those methods.
]

#organizer[
#idx("name collision")  Four weapons against name collisions, in order --- do not export, the prefix
  convention, symbol visibility, and cutting down the names themselves. What
  large projects actually chose comes with them. Finally, how C++ solved this
  spot with `namespace`, and what to watch for when the two languages are mixed.
]

#chapter-questions()

== When and where collisions go off

Spellings collide in three places, and each is harder to diagnose than the last.

#dtable(
  columns: 3,
  [*When*], [*What tells you*], [*How easy to handle*],
  [At compile time], [declared twice in one scope --- the compiler refuses at once], [Easy. It is visible right there],
  [At link time], [`multiple definition of 'init'`], [Moderate. The message names the files],
  [*Silently*], [nothing tells you], [Hard. The wrong function runs and you see only symptoms],
)

The third is why this chapter exists. Three places where silent collisions arise.

- *The link order of static libraries.* If `libfoo.a` and `libbar.a` both carry the
  name, the linker takes whichever it met first on the command line and stops.
  Reorder the libraries and the program behaves differently.
- *Weak symbols.* A name the implementation marked "anyone may override this" is
  replaced without a word. Swapping in your own `malloc` is this pattern.
- *Interception at run time.* Unix's `LD_PRELOAD` lets whatever loaded first win.
  A legitimate technique for diagnostic tools, and an accident when the same name
  is used by chance.

#qa[
  Is `multiple definition` actually the welcome error?
][
  Yes. It is *an audible collision.* What is genuinely dangerous is the linker
  picking one side without a word.

  So the direction of the discipline is not "let us fix collisions well" but *"let
  us make sure a collision is heard."* Cut the exported names to a minimum and
  (a) the chance of colliding drops and (b) when one happens the linker makes a
  noise. All four weapons below point that way.
]

== The first weapon — do not export (`static`)

The surest defence is not putting the name outside at all. Add `static` to a
file-scope declaration and it has internal linkage (chapter 51): outside that
translation unit it does not exist.

The discipline reduces to one line --- *make `static` the default and release only
what goes in the header.* There is a bonus: knowing "this name cannot be called
from another file", the compiler can optimise more aggressively (inlining, removing
unused functions).

#antipattern[
  Leaving something global that was never meant for the header
][
  ```c
  /* util.c */
  int helper(int x) { … }         /* not in any header, yet external */
  ```

  Nobody calls it, and still it occupies a name in the linker's yard. The day
  another file happens to make the same name, the problem surfaces. The cost of
  writing `static` is one word.
]

== The second weapon — the prefix convention

For names that must be exported, the fence is built by hand. It is the universal
practice of C libraries, and the larger the project the fewer the exceptions.

#dtable(
  columns: 3,
  [*Project*], [*Prefix*], [*How far it goes*],
  [SQLite], [`sqlite3_`], [functions, types, constants --- even the version number is in the prefix],
  [libcurl], [`curl_`, `CURL`], [lowercase for functions, capitals for constants and types],
  [zlib], [`z`, `inflate`/`deflate`], [old enough that the prefix is short --- which is why it sometimes collides],
  [OpenSSL], [`SSL_`, `EVP_`, `X509_`], [a different prefix per module],
  [GLib, GTK], [`g_`, `gtk_`, `G_`, `GTK_`], [types in camel case, as `GtkWidget`],
  [SDL], [`SDL_`], [functions, types, constants and macros alike],
  [The Linux kernel], [the subsystem's name], [`kmalloc`, `vfs_`, `sock_` --- the layer as the prefix],
)

Four design rules can be read out of it.

+ *Short, but unique.* zlib's `z` is convenient for being short and collides just
  as easily. Three or four characters is a safe choice today.
+ *Put it on types and macros too.* A prefix that covers only functions is half a
  fence --- chapter 52 showed that type names and enumeration constants live in the
  same yard.
+ *Split the layers by case.* Combine the prefix with the habit of lowercase for
  functions and capitals for macros and constants, and a name alone reveals what it
  is.
+ *Write it down.* One line in the conventions document saying "our prefix is this"
  is cheaper than pointing it out a hundred times in review.

#realcase[
  The legacy of the days before prefixes
][
  The standard library is itself a museum of unprefixed names --- `open`, `read`,
  `write`, `time`, `index`, `link`. In the 1970s a program used a handful of
  libraries, so it was not a problem.

  Today's common accident is the result. Make a function called `read` or `time` in
  your own code and you hide the standard one, and other code calling it quietly
  goes elsewhere. That is what chapter 52's table of reserved names is for, and
  "assume the short, common words are already taken" is practice's first rule.
]

== The third weapon — symbol visibility

A prefix is a convention people keep, so it leaks. There is one more layer that
*lets the build enforce it* --- the symbol visibility of a shared library.

#dtable(
  columns: 3,
  [*Platform*], [*Hiding by default*], [*Marking what is exported*],
  [ELF (Linux, BSD)], [`-fvisibility=hidden`], [`__attribute__((visibility("default")))`],
  [macOS], [`-fvisibility=hidden`], [the same attribute, or a list file of exported symbols],
  [Windows], [already hidden by default], [`__declspec(dllexport)` or a `.def` file],
)

The practice is to wrap it in one macro.

```c
#if defined(_WIN32)
#  define MYLIB_API __declspec(dllexport)
#elif defined(__GNUC__)
#  define MYLIB_API __attribute__((visibility("default")))
#else
#  define MYLIB_API
#endif

MYLIB_API int mylib_open(const char *path);
```

Three effects: collisions grow rarer, loading gets faster (fewer symbols to
resolve), and the boundary between the internal functions you may change and the
public ones you may not *is written into the code.*

#qa[
  How do I check what I am actually exporting?
][
  Count them. Pull the defined global symbols out of the object file or library.

  ```sh
  nm -g --defined-only mylib.o      # an object file
  nm -D --defined-only libmy.so     # a shared library (dynamic symbols)
  objdump -T libmy.so               # the same job with another tool
  ```

  Measure this chapter's listing that way and the exported names are just two,
  `main` and `textbuf` --- the inner functions are all `static` and absent from the
  list.

  One good habit: *dump the list of public symbols to a file and compare it in the
  build.* If a new name leaks out unintended, the build says so. It is rung 3 of
  chapter 12's ladder (let the build tell you) applied to names.
]

== The fourth weapon — cut down the names themselves

Better than building a good fence is *having few names to put outside.* There is a
pattern in which a module, instead of exporting twenty functions, exports a single
struct holding function pointers.

#demo("examples/ch53/prefix.c")

The only name this translation unit hands the linker is `textbuf` (the `nm`
measurement above). The calling side writes `textbuf.push(...)` with a dot ---
and the bonus of the pattern is that *it reads like a language that has name
spaces.*

The price is worth stating plainly.

#dtable(
  columns: 2,
  [*What you gain*], [*What you pay*],
  [external names collapse to one], [calls go through a function pointer --- inlining gets hard],
  [the implementation can be swapped whole], [one level of indirect call slower],
  [the caller reads like a name space], [a debugger no longer shows the callee at a glance],
)

So the pattern belongs *at boundaries* --- plugins, swappable back ends, test
doubles. Not in an inner loop where speed matters.

== How C++ solved this spot

C++ put "the user digs a yard" into the language. Here is as much as a C programmer
needs, and accurately.

=== The syntax

```cpp
namespace app {
    int  parse(const char *);
    namespace detail { int helper(int); }   // nested
}
namespace app::io { void flush(); }         // the C++17 shorthand

int x = app::parse("42");
int y = app::detail::helper(1);
```

The yard's name goes in front of the name. It *does the same job* as C's prefix
convention; what differs is that the language enforces it and the tools understand
it.

=== The anonymous namespace — the equivalent of C's `static`

```cpp
namespace { int hidden(int x) { return x; } }   // this translation unit only
```

Measuring shows the character of it.

#dtable(
  columns: 3,
  [*Declaration*], [*Symbol kind (`nm`)*], [*Meaning*],
  [`static int st(int);`], [`t` --- local], [internal linkage, as in C],
  [`hidden` in an anonymous namespace], [`t` --- local], [the same effect; the name prints as `(anonymous namespace)::hidden`],
  [an ordinary `int use(int);`], [`T` --- global], [external linkage],
)

The two have the same effect, but C++ prefers the anonymous namespace --- unlike
`static` it can be applied to *types* as well, and those can be passed as template
arguments.

=== `using namespace` — the convenience and its price

`using namespace std;` *tears down the yard's fence on the spot.* Handy in a short
example; the conventions of practice mostly run like this.

#dtable(
  columns: 2,
  [*Convention*], [*Ground*],
  [Never in a header], [the fence comes down for *every* file that includes it],
  [Narrow even in a source file], [one name at a time, as `using std::string;`],
  [Inside a function], [the effect is confined to that function],
  [`swap` is the exception --- it has its own idiom], [because of ADL, below],
)

=== ADL — looking in the yard of the argument

C++ has *argument-dependent lookup*: the yard where the argument's type lives is
searched as well, automatically.

```cpp
namespace app { struct Buf {}; void print(const Buf &); }
app::Buf b;
print(b);        // app:: was not written, yet app::print is called — ADL
```

Convenient, and a surprising place too. Which function gets called depends on *the
type of the argument*, so in code with many overloads a person struggles to follow
it. C has no such lookup --- one name is one function. Worth remembering as *a spot
where C's simplicity pays.*

=== Name mangling and `extern "C"`

C++ supports overloading, so *the same spelling with different argument types is a
different function.* For that, the name the linker sees must carry type information
--- that is mangling.

#dtable(
  columns: 2,
  [*Source*], [*The name the linker sees (GCC, measured)*],
  [`namespace app { int f(int); }`], [`_ZN3app1fEi`],
  [`namespace app { double f(double); }`], [`_ZN3app1fEd`],
  [`extern "C" int c_f(int);`], [`c_f`],
)

Take `_ZN3app1fEi` apart and the yard name `app` (three characters), the function
name `f` (one) and the argument type `i` are all in there. *The name space is carved
into the symbol.*

So mixing C and C++ means saying "name this function by C's rules", and that is
`extern "C"`. The canonical pattern for a C header:

```c
#ifdef __cplusplus
extern "C" {
#endif

int mylib_open(const char *path);

#ifdef __cplusplus
}
#endif
```

#platform[
  Where a C header breaks in C++ alone
][
  Some headers are fine in C and will not compile in C++. The cause is nearly
  always one thing --- *a C++ keyword used as a name.*

  ```c
  int register_thing(int class, int new);   /* C: fine. C++: an error */
  ```

  Measured, the C compiler says nothing and the C++ compiler refuses with
  `expected primary-expression before 'int'`.

  So when writing a header that *may also be used from C++*, courtesy is to keep
  C++'s reserved words (`class`, `new`, `delete`, `template`, `this`, `namespace`,
  `try`, `catch`, `operator`, `private`, `public`, `virtual` and the rest) out of
  parameter names. Append an underscore (`class_`) or pick another word.

  Words that became keywords in C23 --- `bool`, `true`, `false` --- also turn up as
  variable names in old C headers. The same family of problem.
]

#qa[
  Should C then bring name spaces into the language?
][
  It has been proposed several times and folded each time for the same reason ---
  *the ABI and existing code.* Bringing in name spaces means the yard must be carved
  into the symbol (the mangling above), and that splits forty years of libraries and
  linking conventions. C survives today as "the common denominator any language can
  call" precisely because its symbol names are simple (chapter 92).

  In other words this is *not a deficiency but a trade.* C gave up convenience in
  names and gained simplicity in linking. That simplicity is why Python, Java and
  Rust all speak C's ABI.

  So this chapter's weapons are not stopgaps. They match C's design, and large
  projects have got along on them for decades.
]

== Good habits — a summary

#dtable(
  columns: 2,
  [*Habit*], [*Why*],
  [Make `static` the default], [a name never exported cannot collide],
  [Settle a prefix and write it down], [a fence built by people needs agreement to stand],
  [Prefix types, macros and enumeration constants too], [all three share one yard (ch. 52)],
  [Avoid short, common words], [`read`, `time`, `index` already have owners],
  [Let the build enforce visibility], [tools plug what conventions leak],
  [Dump the public symbol list and compare it], [the moment one leaks, the build says so],
  [Keep C++ keywords out of headers], [you cannot know everywhere they will be used],
  [Turn on `-Wshadow`], [shadowing is not caught by the default warnings (ch. 52)],
)

We know how to govern names. But chapter 52 went past the remark that "macros ignore
all four name spaces" --- meaning a layer that knows nothing of C replaces names.
The next chapter opens that layer head on, and follows the formal steps by which
source code becomes a program.
