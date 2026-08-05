#import "../../book/lib.typ": *

= Appendix A — Operator precedence and associativity

The body (chapter 20) advised "do not memorise the table; use parentheses". This table
is not for memorising but for looking up when you are stuck reading somebody else's
code. The higher up, the more strongly it binds.

#dtable(
  columns: 3,
  [*group*], [*operators*], [*associativity*],
  [postfix], [`() [] . -> ++(postfix) --(postfix)`, compound literals], [left→right],
  [unary], [`++(prefix) --(prefix) + - ! ~ (type) * & sizeof alignof`], [right→left],
  [multiplicative], [`* / %`], [left→right],
  [additive], [`+ -`], [left→right],
  [shift], [`<< >>`], [left→right],
  [relational], [`< <= > >=`], [left→right],
  [equality], [`== !=`], [left→right],
  [bitwise AND], [`&`], [left→right],
  [bitwise XOR], [`^`], [left→right],
  [bitwise OR], [`|`], [left→right],
  [logical AND], [`&&`], [left→right],
  [logical OR], [`||`], [left→right],
  [conditional], [`?:`], [right→left],
  [assignment], [`= += -= *= /= %= &= ^= |= <<= >>=`], [right→left],
  [comma], [`,`], [left→right],
)

== The places one slips often

#dtable(
  columns: 3,
  [*what was written*], [*how it actually binds*], [*if this was meant*],
  [`a & b == c`], [`a & (b == c)`], [`(a & b) == c`],
  [`a << 1 + 2`], [`a << (1 + 2)`], [`(a << 1) + 2`],
  [`*p++`], [`*(p++)`], [`(*p)++`],
  [`*p.x`], [`*(p.x)`], [`(*p).x` or `p->x`],
  [`(int)x + y`], [`((int)x) + y`], [`(int)(x + y)`],
  [`a = b = 0`], [`a = (b = 0)`], [(as written — right associative)],
  [`x < y < z`], [`(x < y) < z`], [`x < y && y < z`],
  [`!x & y`], [`(!x) & y`], [`!(x & y)`],
  [`sizeof a + 1`], [`(sizeof a) + 1`], [`sizeof(a + 1)`],
  [`a ? b : c = d`], [`(a ? b : c) = d` (usually an error)], [`a ? b : (c = d)`],
)

The first two rows in particular are counted among C's famous design flaws — that the
bitwise operators bind *more weakly* than the comparison operators is a trace of early
C before it had `&&` and `||`. So parentheses are effectively compulsory in bit tests.

== What precedence cannot answer

Precedence is only *a rule of binding*, not the order in time of evaluation
(chapters 17 and 32). The three below have no answer however long you look at the
table.

- *The evaluation order of subexpressions* — which side of `f() + g()` is called first
  is unspecified.
- *Expressions that touch the same object twice* — changing the same object twice
  between sequence points, as in `i = i++`, is outside the contract.
- *The evaluation order of arguments* — `h(f(), g())` is likewise unspecified.

The knack of reading is one: *if the binding confuses you, parenthesise; if the order
troubles you, split the statement.*
