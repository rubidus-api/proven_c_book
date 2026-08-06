#import "../../book/lib.typ": *

= Appendix A — Operator lookup

Operators are *explained* in chapter 46, "Expressions and operators". That
chapter gives each operator as a contract — operand constraints, result type,
grey zones — and says why each rule is the way it is. This appendix does not
repeat any of that. What is here is the one-page table you scan when a piece
of code stops making sense.

#platform("What this appendix rests on")[
  Checked against the expressions clause (§6.5) of ISO/IEC 9899:2024 (C23).
  If you need to cite a rule, cite the published standard as appendix D
  explains.
]

== Precedence and associativity

The higher up, the more strongly it binds. *Associativity* decides which side
groups first when operators of the same strength stand side by side — `a - b - c`
is `(a - b) - c` because it is left-associative, and `a = b = 0` is
`a = (b = 0)` because assignment is right-associative.

#dtable(
  columns: 4,
  [*group*], [*operators*], [*assoc.*], [*why it associates that way*],
  [postfix], [`() [] . -> ++(post) --(post)`, compound literal], [L→R], [`a.b.c` only makes sense burrowing from the left],
  [unary], [`++ -- + - ! ~ (type) * & sizeof alignof`], [R→L], [the nearest one binds first: `- -x`, `*&x`],
  [multiplicative], [`* / %`], [L→R], [the convention of arithmetic],
  [additive], [`+ -`], [L→R], [subtraction only makes sense left-associative],
  [shift], [`<< >>`], [L→R], [`a << 1 << 2` pushes in turn],
  [relational], [`< <= > >=`], [L→R], [which is why `x < y < z` differs from mathematics],
  [equality], [`== !=`], [L→R], [],
  [bitwise AND], [`&`], [L→R], [],
  [bitwise XOR], [`^`], [L→R], [],
  [bitwise OR], [`|`], [L→R], [],
  [logical AND], [`&&`], [L→R], [short-circuiting only works from the left],
  [logical OR], [`||`], [L→R], [the same reason],
  [conditional], [`?:`], [R→L], [so `a ? b : c ? d : e` reads as a ladder],
  [assignment], [`= += -= *= /= %= &= ^= |= <<= >>=`], [R→L], [so that `a = b = 0` makes both zero],
  [comma], [`,`], [L→R], [the left is done first and discarded],
)

== Places where people slip

#dtable(
  columns: 3,
  [*what was written*], [*how it really groups*], [*if that was the intent*],
  [`a & b == c`], [`a & (b == c)`], [`(a & b) == c`],
  [`a << 1 + 2`], [`a << (1 + 2)`], [`(a << 1) + 2`],
  [`*p++`], [`*(p++)`], [`(*p)++`],
  [`*p.x`], [`*(p.x)`], [`(*p).x` or `p->x`],
  [`(int)x + y`], [`((int)x) + y`], [`(int)(x + y)`],
  [`a = b = 0`], [`a = (b = 0)`], [(as it is — right-associative)],
  [`x < y < z`], [`(x < y) < z`], [`x < y && y < z`],
  [`!x & y`], [`(!x) & y`], [`!(x & y)`],
  [`sizeof a + 1`], [`(sizeof a) + 1`], [`sizeof(a + 1)`],
  [`a ? b : c = d`], [`(a ? b : c) = d` (usually an error)], [`a ? b : (c = d)`],
)

The first two lines are counted among C's famous design scars — that the bitwise
operators bind *more weakly* than the comparisons is a trace of early C, before it
had `&&` and `||`. So parentheses are effectively mandatory around a bit test.
The standard itself notes in a footnote that `a<b<c` does not read as it does in
mathematics.

== Where to look instead

#dtable(
  columns: 2,
  [*What you are after*], [*Where it is*],
  [Per-operator contracts (operands, result, grey zones)], [Chapter 46, "Expressions and operators"],
  [Assignment, side effects, evaluation order], [Chapter 33],
  [The rules of pointer arithmetic], [Chapter 37],
  [Telling UB, unspecified and implementation-defined apart], [Chapter 49],
  [What an implementation must document], [Appendix D],
)
