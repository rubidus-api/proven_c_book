#import "../../book/lib.typ": *

= Representing integers — sign, overflow, shift

#prereq(
  ([chapter 4, A simple model of the machine], [what the machine handles is a bundle of bits of fixed width]),
  ([chapter 5, Words and addresses], [words and bytes]),
)

#deepqa[
  Chapter 5 said that several bytes are joined to hold a large number, and we
  even saw endianness (the order of storing). But every number so far has been
  zero or above. Negative numbers — where among the bits do you put the minus
  sign?
][
  There is nowhere to put it — bits have only 0 and 1, no minus sign
  (chapter 4). So negative numbers too are made by *agreement*: we decide to
  *read* certain bit patterns as negative. That there was more than one way to
  make that agreement, and how the competition ended, is the heart of this
  chapter.
]

#organizer[
  The first rung on the ladder of representation. You will see how unsigned
#idx("two's complement")  integers are circular (modular) numbers, how three ways of
  holding negative numbers in bits competed until two's complement settled it —
  and what C23 finally pinned down. Add numbers that overflow and shifting bits
#idx("overflow")  wholesale, and the background knowledge about integers is
  complete.
]

#chapter-questions()

== Unsigned integers — numbers that go round like a clock

First the world without any worry about minus signs. Read $n$ bits as a plain
binary number and you hold $2^n$ numbers, from $0$ to $2^n - 1$ (chapter 4).
This is the *unsigned* integer. Eight bits give 0–255.

There is one property of this world you must take with you: *the end joins back
to the beginning*. Add 1 to 255 and you get not 256 but — since there is no
ninth bit to hold 256 — 0. It is the structure of a clock, where one hour after
twelve is one. In mathematics this kind of arithmetic is called *modular
arithmetic*.

#mathbox[
  Modular arithmetic — the exact mathematics of finite numbers
][
  Addition, subtraction and multiplication of $n$-bit unsigned integers are
  exactly the operations that take the remainder of the result modulo $2^n$:

  $ "result" = (a + b) mod 2^n $

  In eight bits, $250 + 10 = 260 mod 256 = 4$. What matters is that this is not
  "wrong addition" but *a different addition* — a fully defined, predictable
  piece of mathematics. The C standard likewise defines unsigned overflow not
  as an error but as this arithmetic.
]

This circling has a name — *overflow*, or more precisely *wrap-around*. Defined
behaviour though it is, it becomes an accident when it happens somewhere you
did not expect.

#realcase[
  The wall at level 256 — the Pac-Man kill screen
][
  The arcade game Pac-Man has a famous wall. Play proceeds perfectly well up to
  level 255, and then on level 256 the right half of the screen is buried in
  meaningless symbols and the game becomes unplayable. The code counted the
  level number in *eight bits*, so the moment it passed 255 it wrapped to 0, and
  the routine that drew fruit on the assumption of "what level are we on" drew
  an absurd number of them and destroyed the screen. The container size the
  designer thought sufficient — "nobody will get to level 256" — became a wall
  in front of the best players. A container's size is always a wall that
  somebody reaches.
]

== Three agreements for holding negative numbers

Now the negatives. The task is to agree to *read* about half of the $n$-bit
patterns as negative, and historically three agreements were actually used. We
compare them by holding $-5$ in eight bits.

*First, sign-magnitude.* It imitates human notation directly — use the leading
bit as the minus sign (1 means negative) and hold the magnitude in the rest.
$-5$ is `1_0000101`. Intuitive, but it costs two things. `00000000` (+0) and
`10000000` (−0) mean there are *two zeros*. And the addition circuit is a
headache — adding two numbers of different signs needs a separate procedure of
"compare the magnitudes, subtract the smaller from the larger, and decide the
sign."

#idx("ones' complement")*Second, ones' complement.* To make a number negative,
flip every bit. $5$ is `00000101`, so $-5$ is `11111010`. The addition circuit
gets considerably simpler, but there are still two zeros (`00000000` and
`11111111`) — and every comparison has to drag along the exception "the two
zeros count as equal."

*Third, two's complement.* Flip the bits and *add one*. $-5$ is
`11111010 + 1 = 11111011`. This rule looks arbitrary at first but is in fact
the most elegant — there is only one zero and, above all, *the unsigned
addition circuit, used unchanged, gets signed addition right by itself*. No
separate procedure, no exceptions.

#qa[
  But why the name "complement"? And in "ones' complement" and "two's
  complement", what do the one and the two refer to?
][
  A *complement* is "a number that fills something up to a given reference."
  Decimal makes it easy to feel. For the three-digit number 304, the number that
  fills each digit up to 9 is 695, called the *nines' complement* (in each
  position, $9 - "that digit"$); the number that fills it up to 1000 is 696, the
  *ten's complement* ($10^3 - 304$). The relation between them is "nines'
  complement + 1 = ten's complement."

  Do the same thing in binary and the names unravel. The number that fills each
  position up to *1* — that is, subtracting from `11111111` — is the *ones'
  complement*, and since $1 - "bit"$ in binary is just flipping the bit, that is
  where the rule "flip them" comes from. And subtracting from $2^n$ — binary's
  "$1000...0$", a *power of two* — is the *two's complement*. The trick "flip
  and add one" is exactly the relation "nines' complement + 1 = ten's
  complement" in decimal.

  A word on the English spelling. The usual forms are *one's complement* and
  *two's complement* (there is no "1s'" or "2s'"). But the computer scientist
  Knuth argued for a witty distinction — the first is a complement with respect
  to *the ones in every position*, so the plural possessive *ones' complement*
  is right, while the second is a complement with respect to the *single*
  number $2^n$, so the singular *two's complement* is right. It sounds like
  grammatical pedantry, but it captures exactly the difference in the two
  mathematical definitions (per-position reference vs. whole-number reference) —
  and *the C standard itself adopted the distinction*. Its clause on
  representations writes the three schemes as "sign and magnitude", "two's
  complement" and *"ones' complement"*. Knuth's pedantry won in the statute
  book. In textbook terminology the two's complement is also called the *radix
  complement* and the ones' complement the *diminished radix complement*.
]

#mathbox[
  Why two's complement is elegant — modular arithmetic, reused
][
  The identity of two's complement is the modular arithmetic above. Since the
  agreement holds $-x$ as the pattern $2^n - x$, in eight bits $-5$ is
  $256 - 5 = 251$, that is `11111011`. Then $7 + (-5)$ is, from the circuit's
  point of view, $7 + 251 = 258 mod 256 = 2$ — the answer comes out right by
  itself. A negative number is merely "counting the other way round the modular
  clock", so the circuit need not know about signs at all. The only asymmetry is
  the range — in eight bits it runs from $-128$ to $+127$, one more negative
  than positive ($-128$ has no partner $+128$).
]

== The competition of the three, and C23's decision

All three schemes were used in real machines — sign-magnitude and ones'
complement genuinely existed on early mainframes (the UNIVAC and CDC lines
among them). Because such machines were still in service when C was
standardised in 1989, the C standard took nobody's side: *it permitted all
three representations*. That neutrality was not free — with different
representations the result bits of the same operation differ, so the standard
had no choice but to leave much of the behaviour of signed integers as "it
depends on the machine." Half the reason signed overflow became *undefined
behaviour* (chapter 49) lies here.

Meanwhile reality converged on one side. The circuit simplicity of two's
complement was overwhelming, so for decades essentially every new CPU used it
and the other two schemes went to the museum. And *C23 finally decided — the
representation of signed integers is two's complement*. Half a century of
practice was promoted to a promise of the standard (exactly the pattern of the
"byte = 8 bits" discussion in chapter 4).

#qa[
  Now that the representation is pinned to two's complement, is signed overflow
  defined as wrap-around too?
][
  No — and this is the subtle, important point. What C23 pinned down is the
  *representation* (what bit pattern a negative number has), not the *meaning of
  overflow*. Overflow of signed integers remains undefined behaviour in C23 as
  well. The reason is optimisation rather than representation — the assumption
  that "signed numbers do not overflow" is valuable to the compiler
  (chapter 13) in loop analysis and reordering, so the standard chose to keep
  it. In summary: unsigned overflow = defined wrap-around, signed overflow =
  still outside the contract. The practical rules are covered in chapter 26.
]

#misconception[
  "If an overflow happens, the computer tells you there was an error"
][
  A plausible expectation — it is only decent to be told when something goes
  wrong. But a CPU's addition circuit merely raises an internal signal (a flag)
  at the moment of overflow; *by default it neither stops nor notifies the
  program*. C is the same — unsigned numbers wrap silently, and signed numbers
  are outside the contract (anything may happen). Pac-Man's screen breaking
  spectacularly was not an overflow "alarm" but a *downstream accident* caused
  by the wrapped value. Watching for overflow is the programmer's job, not the
  machine's — which is also why verified tools like proven check arithmetic
  later on (chapters 40 and 82).
]

== Shift — pushing bits wholesale

#idx("shift")There is one more basic operation on the bits of an integer —
the *shift*, pushing the whole string of bits left or right. After pushing,
two questions remain. *Where do the bits pushed out go, and what fills the
vacancy?*

*Left shift* has one answer. Bits pushed off the top are discarded and the
vacancy below is filled with 0. Push `00010110` one place left and you get
`00101100` — just as adding a 0 on the end multiplies by ten in decimal, one
place left in binary is *doubling*.

*Right shift* has two answers, differing in what fills the vacancy at the top.

- *Logical shift*: fill with 0. This suits unsigned numbers, and one place
  right is the quotient on division by two.
- *Arithmetic shift*: fill by *copying the sign bit*. A negative number in two's
  complement has its top bits full of ones (see $-5$ = `11111011` above), so
  ones must be shifted in for the meaning "divide by two" to survive. Fill with
  zeros and a negative number is suddenly read as an enormous positive one.

So CPUs carry two right-shift instructions (logical and arithmetic), and in C
the right shift of an unsigned number is logical, while the right shift of a
negative signed number was — for a long time "machine-dependent" until
essentially every implementation converged on arithmetic in practice. Alongside
the settling of two's complement, this is the same direction: practice promoted
to promise.

#qa[
  Is the shift important enough to justify learning the rules for pushing and
  filling?
][
  It is — for two reasons.

  First, *because it is the cheapest operation.* For the circuit a shift is
  about as much work as moving wires sideways, so on nearly every CPU it is
  among the fastest, single-beat operations. As we just saw, $k$ places left is
  multiplication by $2^k$ and $k$ places right is the quotient on division by
  $2^k$ — so turning multiplication and division by powers of two into shifts
  was a classic speed trick. In today's C, though, *you need not play that trick
  yourself*. Write `x * 2` and `x / 8` in the source, meaning exactly that, and
  the compiler (the editor of chapter 13) turns them into shifts for you. This
  is a place where you give up readability and gain nothing.

  Second, *because it is the basic move for working in the world of bits.*
  Packing several values into the bit positions of one integer and taking them
  back out — assembling UTF-8 bytes as in chapter 9, the tagged pointers of
  chapter 6, splitting a colour value (RGB), reading the flags of a hardware
  register — is all a combination of shift and mask: "push to the position you
  want, and keep only the bits you need." The shift as multiplication has been
  handed over to the compiler, but the shift as a *placement tool* remains the
  everyday language of the systems programmer. Its actual use in C's syntax is
  covered in chapter 27.
]

#qa[
  What happens if you push an eight-bit number eight places, or more? Common
  sense says everything is pushed out and it becomes 0.
][
  That very "common sense" differing between machines is the trap. The shift
  count is processed by a circuit of some width inside the CPU, and machines
  diverged on what to do when a count at least as large as the width arrived —
  one family (Intel x86) looks only at the low bits of the count and ignores the
  rest, so shifting a 32-bit number by 32 leaves it *unchanged*, while others
  (older ARM and the like) really do push everything out and give *0*. The same
  code gives different answers on different machines. The C standard's response
  is by now a familiar pattern — unable to take sides, it put *shifts of at
  least the width* outside the contract, as undefined behaviour. "Where machines
  respond differently, the standard gives up on promising" — we meet this
  pattern formally again in chapter 49.
]

== Sign extension — from a narrow container to a wide one

The question "what fills the vacancy?" shows up in one more place: moving a
number held in an eight-bit container into a sixteen-bit one. What fills the
eight new positions at the top?

For unsigned numbers the answer is obvious — *fill with 0* (zero extension).
The eight-bit `11111011` (= 251) becomes the sixteen-bit
`00000000 11111011` (= 251). The value is unchanged.

For signed numbers the same method causes an accident. The eight-bit
`11111011` is $-5$ in two's complement, but filling the top with zeros gives
the sixteen-bit `00000000 11111011` — the leading bit is 0, so it reads as
*positive 251*. $-5$ turned into 251 while changing containers. The correct
answer is the same trick as the arithmetic shift — *fill by copying the sign
bit*. `11111111 11111011`, still $-5$. This is *sign extension*.

#qa[
  We filled it with a pile of ones and the value is unchanged? Is that a
  coincidence?
][
  Not a coincidence but a necessity of modular mathematics. In two's complement
  the eight-bit $-5$ was the pattern $2^8 - 5 = 251$, and the sixteen-bit $-5$
  is the pattern $2^16 - 5 = 65531$. And $65531 = 251 + 255 dot 256$ — written
  in binary, exactly "the original pattern with eight ones laid on top."
  Copying the sign bit is a trick that performs, with a single bit-copy, the
  arithmetic of "swapping a complement with respect to $2^8$ for a complement
  with respect to $2^16$." The elegance of two's complement is at work here too
  — with sign-magnitude or ones' complement there is no such free extension.
]

Conversely, *narrowing* from a wide container into a narrow one simply cuts off
the upper bits — a cousin of overflow, in that a value that does not fit its
container is silently ruined. C has rules for automatically widening small
integers before a calculation (integer promotion), and when widening and
narrowing happen and what is dangerous about them is treated formally with C's
integer types in chapters 26 and 27 — the picture in this chapter (zero fill /
sign copy / truncation) is the capital for that.

The background on integers is complete. Unsigned numbers are a modular world
that goes round like a clock; negative numbers were settled, after a
competition of three agreements, on two's complement, which C23 pinned down;
overflow is silent; and shifting and changing containers (extension) only make
sense once you know how the vacancy is filled. C's integer *types* and the
practical rules are built on this background in chapters 26 and 27.

The next chapter is the next rung on the ladder — beyond integers, the two ways
of holding numbers with a decimal point, and the contract called IEEE 754.
