#import "../../book/lib.typ": *

= Appendix C — Implicit conversions in summary

A compression of chapter 28's rules into tables.

== Integer promotion

When `bool`, `char`, `signed char`, `unsigned char`, `short`, `unsigned short` or a
bit-field takes part in arithmetic:

- if all the values fit in an `int` → `int`
- otherwise → `unsigned int`

That is, *arithmetic does not happen on a type narrower than int.*

== The usual arithmetic conversions (the common type of two operands)

1. If one side is `long double` → both `long double`
2. Otherwise if one side is `double` → both `double`
3. Otherwise if one side is `float` → both `float`
4. If both are integers → after promoting each:
   - if the signedness is the same → the wider side
   - if the unsigned side is wider or equal → *the unsigned side*
   - if the signed side is wider and holds all of the other's values → the signed side
   - otherwise → the unsigned edition of the signed side

The practical summary: *when they mix, to the wider side; when the widths are equal,
the unsigned side wins.* Avoid comparisons of mixed signedness (turn the warnings on
and the compiler points them out).

== The default promotions of variadic arguments

Arguments crossing into a place with no type in the prototype (`...`):

- `float` → `double`
- the `bool`, `char` and `short` families → `int` (or `unsigned int`)

So `printf` has no float-specific format, and `%f` takes a double (chapters 19, 25 and
50).

== Integer conversion rank (the criterion of the conversion rules)

The order used when the usual arithmetic conversions choose "the wider side". Higher is
above.

```text
long long  >  long  >  int  >  short  >  char / bool
```

At the same width the ranks of the unsigned and the signed side are the same, and then
*the unsigned side wins*. That is why `-1 < 1u` is false — `-1` turns into a huge
unsigned value (chapter 28).

== Which conversion calls down which danger

#dtable(
  columns: 3,
  [*conversion*], [*what happens*], [*danger*],
  [signed → unsigned], [the two's complement value is reinterpreted as it is], [a negative becomes a huge value],
  [unsigned → signed], [as it is if it fits, otherwise implementation-defined], [the value changes at the boundary],
  [wide integer → narrow integer], [the high bits are discarded], [quiet loss of value],
  [integer → real], [rounded to the nearest value], [loss of precision for large integers (chapter 8)],
  [real → integer], [the fractional part is discarded (towards 0)], [outside the range it is outside the contract],
  [`double` → `float`], [the precision is reduced], [rounding error],
  [pointer → `void *`], [lossless], [only the type information is lost],
  [`void *` → another pointer], [lossless], [outside the contract if the alignment does not match (chapter 35)],
  [integer → pointer], [implementation-defined], [the provenance is lost (chapter 14)],
)

== A list of the places quiet conversions happen

Implicit conversion happens *without asking* in the following places.

- Between the two operands of an arithmetic operation (the usual arithmetic
  conversions)
- The right of an assignment → the left's type
- A function argument → the parameter's type (when there is a prototype)
- A `return`'s value → the return type
- A condition place (the condition of `if`, `while`, `?:`) → judgement as `bool`
- Arguments crossing into `...` (the default promotions)
- When an array or function name is used as a value (decay into a pointer)

== Other conversions often met

- *Narrowing* (wide → narrow integer): the high bits are cut (chapter 7). In a signed
  type, if the value does not fit it is implementation-defined behaviour.
- *Real → integer*: the fractional part is discarded (towards 0). Outside the range it
  is outside the contract.
- *Array → pointer*: in most contexts it decays into the address of the first element.
  The exceptions are `sizeof`, `alignof` and `&` (chapter 36).
- *Function → pointer*: a function name decays into a function pointer.
