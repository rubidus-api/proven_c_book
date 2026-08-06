#import "../../book/lib.typ": *

= Appendix E — How grammar is written (EBNF) and the whole C grammar

This appendix holds two things. EBNF, the *notation for writing* a programming
language's grammar, and *the whole C grammar* read in that notation. What the parser
seen in chapter 16 actually looks at while it works, and how to read the grammar
sections of the standard document, are resolved here.

The later sections follow *annex A* of the C23 standard (ISO/IEC 9899:2024, free draft
N3220) as it stands — in its order of lexical grammar, phrase structure grammar and
preprocessing directives. The rules are carried over from the standard, with "what this
rule actually permits" added to each section.

== The notation for writing grammar — from BNF to EBNF

A language's grammar can be written as *a list of rules*. It is the way of pushing "a
sentence is made of these, and each of them is again made of these" down to the end.

#idx("BNF")The first to use this way widely was *BNF* (Backus–Naur Form). It was used
in the 1960 ALGOL 60 report and carries the names of two people (John Backus and Peter
Naur). The notation is simple.

```text
<digit>  ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
<number> ::= <digit> | <digit> <number>
```

The way to read it is this. `::=` is "is made up as follows", `|` is "or", a name
inside angle brackets is a *nonterminal* (something that unfolds further into other
rules), and a letter written plainly is a *terminal* (an actual character that does not
unfold further). The second line writes, by recursion, "one digit, or one digit
followed again by a number", and so defines *a number of one or more places*.

#idx("EBNF")BNF has the inconvenience that recursion must be used every time repetition
or option is written. So it was extended into *EBNF* (Extended BNF), with a few more
symbols added.

#dtable(
  columns: 3,
  [*notation*], [*meaning*], [*example*],
  [`=` or `::=`], [defines], [`digit = "0" | "1" ;`],
  [`|`], [or (choice)], [`sign = "+" | "-" ;`],
  [`[ … ]`], [may or may not be there (0 or 1 time)], [`[ sign ] number`],
  [`{ … }`], [repeated 0 or more times], [`digit { digit }`],
  [`( … )`], [grouping], [`( "+" | "-" ) digit`],
  [`" … "`], [terminal (literally)], [`";"`],
  [`;`], [the end of a rule], [—],
)

Write the same "number of one or more places" in EBNF and the recursion disappears.

```text
number = digit { digit } ;
```

There is a standardised EBNF (ISO/IEC 14977), but real documents use dialects that
differ a little. A common variation is the postfix symbols that came from regular
expressions — `?` (0 or 1 time), `*` (0 or more), `+` (1 or more).

== How to read the C standard's notation

*The C standard document does not use EBNF.* It uses a notation close to BNF instead,
and its rules look like this.

```text
iteration-statement:
    while ( expression ) secondary-block
    do secondary-block while ( expression ) ;
    for ( expression_opt ; expression_opt ; expression_opt ) secondary-block
```

Four conventions are all you need to read the standard's grammar sections as they
stand.

#dtable(
  columns: 2,
  [*convention*], [*meaning*],
  [each indented line after `name:`], [one alternative. a line break stands in for `|`],
  [`_opt` (a subscript opt in the standard)], [that element need not be there],
  [the list after `one of`], [one of those listed. all are terminals],
  [an italic name], [a nonterminal. it unfolds into other rules],
)

Subscripts cannot be used in this appendix, so the standard's opt is written `_opt`.
Why `for (;;)` holds as an infinite loop is written in that one rule above as it
stands — because all three places may be omitted.

It is worth knowing in advance too that the grammar is divided into two layers. *The
lexical grammar* is the rules that gather characters into tokens, and *the phrase
structure grammar* is the rules that weave those tokens — exactly the division of lexer
and parser seen in chapter 16. The preprocessor has a separate grammar altogether
(having already been handled and vanished in chapter 52's translation phases, so
nothing like `#include` appears in the C grammar of A.2 below).

#qa[
  What is good about writing grammar down like this?
][
  Three things become better.

  First, *ambiguity comes to light.* Write in natural language "a statement comes after
  the condition" and in `if (a) if (b) x; else y;` it cannot be known which `if` the
  `else` attaches to; write it as a grammar and that ambiguity shows itself as a
  conflict of rules (C settles it as "it attaches to the nearest `if`").

  Second, *a tool can read it.* Tools that generate a parser from a grammar rather than
  writing it by hand (yacc/bison, ANTLR and the like) take this notation as input.

  Third, *a person can confirm things.* The way the standard document answers the
  question "may this be written in this place" is exactly these grammar sections.
]

== A.1 Lexical grammar

=== A.1.1 Lexical elements

```text
token:
    keyword
    identifier
    constant
    string-literal
    punctuator

preprocessing-token:
    header-name
    identifier
    pp-number
    character-constant
    string-literal
    punctuator
    each non-white-space character that cannot be one of the above
```

The difference between the two rules is the boundary of the translation phases seen in
chapter 52. A *preprocessing token* is not yet a word of C — that the `<stdio.h>` of
`#include <stdio.h>` is caught as a header-name token, and that something like `123abc`
is caught whole as a `pp-number`, are the business of this layer. Then in translation
phase 7 each preprocessing token turns into a *token*, and only then does a diagnosis
such as "this is not an integer constant" appear.

The last line ("a single non-white-space character that is none of the above") matters.
Characters C does not use, such as `@` and `$`, *do exist as preprocessing tokens*. So
they can pass through inside a macro, and if they remain in the final code it becomes
an error then.

=== A.1.2 Keywords

```text
keyword: one of
    alignas        alignof        auto           bool
    break          case           char           const
    constexpr      continue       default        do
    double         else           enum           extern
    false          float          for            goto
    if             inline         int            long
    nullptr        register       restrict       return
    short          signed         sizeof         static
    static_assert  struct         switch         thread_local
    true           typedef        typeof         typeof_unqual
    union          unsigned       void           volatile
    while          _Alignas       _Alignof       _Atomic
    _BitInt        _Bool          _Complex       _Decimal128
    _Decimal32     _Decimal64     _Generic       _Imaginary
    _Noreturn      _Static_assert _Thread_local
```

The promotion treated in chapter 71 is visible in this list as it stands.
`bool`, `true`, `false`, `nullptr`, `static_assert`, `alignas`, `thread_local`,
`constexpr` and `typeof` have risen to being *keywords*, and beneath them the
underscored names (`_Bool`, `_Alignas`, …) remain as they were for old code. The two
names point at the same thing.

`_BitInt` stands out too — the *integer with a specified bit width* C23 brought in,
written like `_BitInt(5)`. It is for places that need "exactly n bits", such as hardware
register fields and fixed-point arithmetic.

=== A.1.3 Identifiers

```text
identifier:
    identifier-start
    identifier identifier-continue

identifier-start:
    nondigit
    a character with the XID_Start property
    a universal-character-name with the XID_Start property

identifier-continue:
    digit
    nondigit
    a character with the XID_Continue property
    a universal-character-name with the XID_Continue property

nondigit: one of
    _ a b c d e f g h i j k l m n o p q r s t u v w x y z
      A B C D E F G H I J K L M N O P Q R S T U V W X Y Z

digit: one of
    0 1 2 3 4 5 6 7 8 9
```

The recursion in the first rule is that familiar rule "the first character must not be
a digit" — only `identifier-start` may come at the beginning, and `digit` is only in
`identifier-continue`.

What C23 changed is *the handling of Unicode*. The older standard listed the permitted
characters, while C23 refers to Unicode's XID_Start and XID_Continue properties. So
identifiers in Korean are possible in principle. But for the reasons seen in chapter 9
(encodings, normalisation, invisible characters) the practice in the field is still
ASCII.

=== A.1.4 Universal character names

```text
universal-character-name:
    \u hex-quad
    \U hex-quad hex-quad

hex-quad:
    hexadecimal-digit hexadecimal-digit hexadecimal-digit hexadecimal-digit
```

It is the way of writing a particular code point regardless of the source file's
encoding (chapter 9). `"é"` means U+00E9 whatever the file's encoding is.

=== A.1.5 Constants

```text
constant:
    integer-constant
    floating-constant
    enumeration-constant
    character-constant
    predefined-constant
```

*Integer constants.*

```text
integer-constant:
    decimal-constant integer-suffix_opt
    octal-constant integer-suffix_opt
    hexadecimal-constant integer-suffix_opt
    binary-constant integer-suffix_opt

decimal-constant:
    nonzero-digit
    decimal-constant '_opt digit

octal-constant:
    0
    octal-constant '_opt octal-digit

hexadecimal-constant:
    hexadecimal-prefix hexadecimal-digit-sequence

binary-constant:
    binary-prefix binary-digit
    binary-constant '_opt binary-digit

hexadecimal-prefix: one of  0x 0X
binary-prefix:      one of  0b 0B
nonzero-digit:      one of  1 2 3 4 5 6 7 8 9
octal-digit:        one of  0 1 2 3 4 5 6 7
hexadecimal-digit:  one of  0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F

hexadecimal-digit-sequence:
    hexadecimal-digit
    hexadecimal-digit-sequence '_opt hexadecimal-digit

binary-digit: one of  0 1

integer-suffix:
    unsigned-suffix long-suffix_opt
    unsigned-suffix long-long-suffix
    unsigned-suffix bit-precise-int-suffix
    long-suffix unsigned-suffix_opt
    long-long-suffix unsigned-suffix_opt
    bit-precise-int-suffix unsigned-suffix_opt

bit-precise-int-suffix: one of  wb WB
unsigned-suffix:        one of  u U
long-suffix:            one of  l L
long-long-suffix:       one of  ll LL
```

Three things to point out.

*① Beginning with `0` means octal.* The first line of `octal-constant` is that rule,
and it is why `017` is 15. It is the source of the classic mistake of writing a date or
a time as `08` and meeting a compile error (`8` is not an octal digit).

*② C23 brought in binary literals and digit separators.* They can be written like
`0b1010'0110` — the `'_opt` visible in the rules is that separator. Writing chapter 27's
bit operations becomes far more readable.

*③ A `wb` suffix appeared.* It is for `_BitInt` constants (`123wb`).

*Floating constants.*

```text
floating-constant:
    decimal-floating-constant
    hexadecimal-floating-constant

decimal-floating-constant:
    fractional-constant exponent-part_opt floating-suffix_opt
    digit-sequence exponent-part floating-suffix_opt

hexadecimal-floating-constant:
    hexadecimal-prefix hexadecimal-fractional-constant
        binary-exponent-part floating-suffix_opt
    hexadecimal-prefix hexadecimal-digit-sequence
        binary-exponent-part floating-suffix_opt

fractional-constant:
    digit-sequence_opt . digit-sequence
    digit-sequence .

exponent-part:
    e sign_opt digit-sequence
    E sign_opt digit-sequence

sign: one of  + -

digit-sequence:
    digit
    digit-sequence '_opt digit

hexadecimal-fractional-constant:
    hexadecimal-digit-sequence_opt . hexadecimal-digit-sequence
    hexadecimal-digit-sequence .

binary-exponent-part:
    p sign_opt digit-sequence
    P sign_opt digit-sequence

floating-suffix: one of  f l F L df dd dl DF DD DL
```

The second line of `fractional-constant` permits `1.`, and the `_opt` in the first line
permits `.5`. And if there is an exponent part the decimal point may be absent (`1e9`).

*Hexadecimal floating constants* (`0x1.8p3`) were brought in by C99 and are used to
write and read exactly the bit representation seen in chapter 8 — writing in decimal
lets rounding in, while this notation carries the representation over as it is. The
knack is that the exponent `p` is *a power of two*.

The suffixes `df`, `dd` and `dl` belong to decimal floating point
(`_Decimal32/64/128`), an optional facility for places that demand decimal rounding,
such as calculating money.

*Character constants and predefined constants.*

```text
enumeration-constant:
    identifier

character-constant:
    encoding-prefix_opt ' c-char-sequence '

encoding-prefix: one of  u8 u U L

c-char-sequence:
    c-char
    c-char-sequence c-char

c-char:
    any member of the source character set except ' \ and newline
    escape-sequence

escape-sequence:
    simple-escape-sequence
    octal-escape-sequence
    hexadecimal-escape-sequence
    universal-character-name

simple-escape-sequence: one of
    \' \" \? \\ \a \b \f \n \r \t \v

octal-escape-sequence:
    \ octal-digit
    \ octal-digit octal-digit
    \ octal-digit octal-digit octal-digit

hexadecimal-escape-sequence:
    \x hexadecimal-digit
    hexadecimal-escape-sequence hexadecimal-digit

predefined-constant: one of
    false true nullptr
```

The recursion in `hexadecimal-escape-sequence` explains one famous trap — *there is no
upper limit on the number of digits.* `"\x41BC"` is read not as `\x41` followed by `BC`
but as one `\x41BC`, and if the value does not fit in a `char` it is outside the
contract. Octal escapes, by contrast, are cut at three digits at most.

That `predefined-constant` exists as a rule of its own is C23's change (chapter 71) —
`true`, `false` and `nullptr` became *grammatical elements* rather than macros.

=== A.1.6 String literals

```text
string-literal:
    encoding-prefix_opt " s-char-sequence_opt "

s-char-sequence:
    s-char
    s-char-sequence s-char

s-char:
    any member of the source character set except " \ and newline
    escape-sequence
```

The `_opt` permits the empty string `""`. The prefixes are the same four as for
character constants — `u8"..."` (UTF-8), `u"..."` (UTF-16), `U"..."` (UTF-32),
`L"..."` (wide). That adjacent string literals join into one is not grammar but the
business of *translation phase 6* (chapter 52).

=== A.1.7 Punctuators

```text
punctuator: one of
    [  ]  (  )  {  }  .  ->
    ++  --  &  *  +  -  ~  !
    /  %  <<  >>  <  >  <=  >=  ==  !=  ^  |  &&  ||
    ?  :  ::  ;  ...
    =  *=  /=  %=  +=  -=  <<=  >>=  &=  ^=  |=
    ,  #  ##
    <:  :>  <%  %>  %:  %:%:
```

The last line is the *digraphs*. `<:` is the same as `[`, `%:` the same as `#` — relics
of the days when keyboards lacked those characters, and unlike trigraphs they remain in
C23.

`::` newly entered because of chapter 71's attribute syntax (`[[gnu::packed]]`).

=== A.1.8 Header names

```text
header-name:
    < h-char-sequence >
    " q-char-sequence "

h-char-sequence:
    h-char
    h-char-sequence h-char

h-char:
    any member of the source character set except newline and >

q-char-sequence:
    q-char
    q-char-sequence q-char

q-char:
    any member of the source character set except newline and "
```

This token is made *only inside a preprocessing directive*. So the `/` and `.` inside
`<stdio.h>` are not read as division and member access — a rare place where the lexer
cuts differently according to context.

=== A.1.9 Preprocessing numbers

```text
pp-number:
    digit
    . digit
    pp-number identifier-continue
    pp-number ' digit
    pp-number ' nondigit
    pp-number e sign
    pp-number E sign
    pp-number p sign
    pp-number P sign
    pp-number .
```

The preprocessor does not yet know the grammar of numbers. So it catches whole, as one
token, "a lump beginning with a digit and continuing with letters, dots and signs" —
`1.2e+3`, `0xFFul`, and even `1abc` are each a single `pp-number`. Whether it is valid
is sorted out in translation phase 7 by holding it against the integer and floating
constant grammar.

Thanks to this rule macro substitution does not split a number, and the `+` in the
middle of `1.2e+3` is not read as addition.

== A.2 Phrase structure grammar

=== A.2.1 Expressions

```text
primary-expression:
    identifier
    constant
    string-literal
    ( expression )
    generic-selection

generic-selection:
    _Generic ( assignment-expression , generic-assoc-list )

generic-assoc-list:
    generic-association
    generic-assoc-list , generic-association

generic-association:
    type-name : assignment-expression
    default : assignment-expression

postfix-expression:
    primary-expression
    postfix-expression [ expression ]
    postfix-expression ( argument-expression-list_opt )
    postfix-expression . identifier
    postfix-expression -> identifier
    postfix-expression ++
    postfix-expression --
    compound-literal

argument-expression-list:
    assignment-expression
    argument-expression-list , assignment-expression

compound-literal:
    ( storage-class-specifiers_opt type-name ) braced-initializer

storage-class-specifiers:
    storage-class-specifier
    storage-class-specifiers storage-class-specifier

unary-expression:
    postfix-expression
    ++ unary-expression
    -- unary-expression
    unary-operator cast-expression
    sizeof unary-expression
    sizeof ( type-name )
    alignof ( type-name )

unary-operator: one of  & * + - ~ !

cast-expression:
    unary-expression
    ( type-name ) cast-expression

multiplicative-expression:
    cast-expression
    multiplicative-expression * cast-expression
    multiplicative-expression / cast-expression
    multiplicative-expression % cast-expression

additive-expression:
    multiplicative-expression
    additive-expression + multiplicative-expression
    additive-expression - multiplicative-expression

shift-expression:
    additive-expression
    shift-expression << additive-expression
    shift-expression >> additive-expression

relational-expression:
    shift-expression
    relational-expression <  shift-expression
    relational-expression >  shift-expression
    relational-expression <= shift-expression
    relational-expression >= shift-expression

equality-expression:
    relational-expression
    equality-expression == relational-expression
    equality-expression != relational-expression

AND-expression:
    equality-expression
    AND-expression & equality-expression

exclusive-OR-expression:
    AND-expression
    exclusive-OR-expression ^ AND-expression

inclusive-OR-expression:
    exclusive-OR-expression
    inclusive-OR-expression | exclusive-OR-expression

logical-AND-expression:
    inclusive-OR-expression
    logical-AND-expression && inclusive-OR-expression

logical-OR-expression:
    logical-AND-expression
    logical-OR-expression || logical-AND-expression

conditional-expression:
    logical-OR-expression
    logical-OR-expression ? expression : conditional-expression

assignment-expression:
    conditional-expression
    unary-expression assignment-operator assignment-expression

assignment-operator: one of
    =  *=  /=  %=  +=  -=  <<=  >>=  &=  ^=  |=

expression:
    assignment-expression
    expression , assignment-expression

constant-expression:
    conditional-expression
```

This ladder is *the same content* as appendix A's precedence table. The table is a
summary for memorising, and the grammar shows where that table came from — the reason
multiplication is stronger than addition is that "an additive expression is defined as
being made of multiplicative expressions".

A few things visible only in the grammar.

*① The left of an assignment is a `unary-expression`.* So `a + b = c` is already caught
at the grammar stage — the left side is not a unary expression. `*p = c` and `a[i] = c`,
on the other hand, pass (they are unary and postfix expressions).

*② Why assignment is right-associative is visible.* The right-hand side is again an
`assignment-expression`, so `a = b = c` binds as `a = (b = c)`. Addition is
left-recursive (`additive-expression + …`) and so binds from the left.

*③ The middle of the conditional operator is an `expression`.* So a comma expression can
go in, as in `a ? b, c : d`, but the last place is a `conditional-expression`, so
parentheses are needed to write an assignment there.

*④ `constant-expression` is settled not by the grammar but by a constraint.* By the rule
alone it is simply a conditional expression, and the demand that it "must be constant"
is in a separate constraints clause. What may be written for an array size or a `case`
label is that clause's part (chapter 23).

*⑤ A compound literal can carry a storage class.* `storage-class-specifiers_opt` is
C23's addition, so the lifetime can be settled as in `(static int[]){1,2,3}` (a way
round chapter 44's block-lifetime trap).

=== A.2.2 Declarations

```text
declaration:
    declaration-specifiers init-declarator-list_opt ;
    attribute-specifier-sequence declaration-specifiers init-declarator-list ;
    static_assert-declaration
    attribute-declaration

declaration-specifiers:
    declaration-specifier attribute-specifier-sequence_opt
    declaration-specifier declaration-specifiers

declaration-specifier:
    storage-class-specifier
    type-specifier-qualifier
    function-specifier

init-declarator-list:
    init-declarator
    init-declarator-list , init-declarator

init-declarator:
    declarator
    declarator = initializer

attribute-declaration:
    attribute-specifier-sequence ;
```

The `init-declarator-list_opt` of the first rule permits *a declaration with no
declarator*, as in `struct point { int x; };` — the case of making only a type and no
variable.

That `declaration-specifiers` is recursive is the ground for permitting jumbled orders
such as `unsigned static long const int`. The grammar does not enforce an order and the
meaning is the same, but it is better to keep the conventional order for the reader.

*Storage classes and types.*

```text
storage-class-specifier: one of
    auto  constexpr  extern  register  static  thread_local  typedef

type-specifier:
    void
    char
    short
    int
    long
    float
    double
    signed
    unsigned
    _BitInt ( constant-expression )
    bool
    _Complex
    _Decimal32
    _Decimal64
    _Decimal128
    atomic-type-specifier
    struct-or-union-specifier
    enum-specifier
    typedef-name
    typeof-specifier

type-qualifier: one of
    const  restrict  volatile  _Atomic

function-specifier: one of
    inline  _Noreturn

alignment-specifier:
    alignas ( type-name )
    alignas ( constant-expression )

type-specifier-qualifier:
    type-specifier
    type-qualifier
    alignment-specifier
```

The four axes organised in chapter 41 are in these few lines as they stand. That
`typedef` is *one of the storage-class specifiers* is visible here (chapter 55), and the
demand that "there be only one storage class" is pinned down not by the grammar but by a
constraints clause. That `constexpr` came into the storage-class place is as seen in
chapter 71.

*Structures and unions.*

```text
struct-or-union-specifier:
    struct-or-union attribute-specifier-sequence_opt identifier_opt
        { member-declaration-list }
    struct-or-union attribute-specifier-sequence_opt identifier

struct-or-union: one of
    struct  union

member-declaration-list:
    member-declaration
    member-declaration-list member-declaration

member-declaration:
    attribute-specifier-sequence_opt specifier-qualifier-list
        member-declarator-list_opt ;
    static_assert-declaration

specifier-qualifier-list:
    type-specifier-qualifier attribute-specifier-sequence_opt
    type-specifier-qualifier specifier-qualifier-list

member-declarator-list:
    member-declarator
    member-declarator-list , member-declarator

member-declarator:
    declarator
    declarator_opt : constant-expression
```

The second line of `member-declarator` is the *bit-field*, and `declarator_opt` permits
an unnamed bit-field (`int : 3;` — padding that merely leaves room) (chapter 43).

That `member-declaration-list` contains `static_assert-declaration` is worth noticing
too. It means the layout can be checked inside the struct.

```c
struct header {
    uint32_t magic;
    uint16_t version;
    static_assert(sizeof(uint32_t) == 4, "");   /* inside the struct */
};
```

*Enumerations.*

```text
enum-specifier:
    enum attribute-specifier-sequence_opt identifier_opt
        enum-type-specifier_opt { enumerator-list }
    enum attribute-specifier-sequence_opt identifier_opt
        enum-type-specifier_opt { enumerator-list , }
    enum identifier enum-type-specifier_opt

enumerator-list:
    enumerator
    enumerator-list , enumerator

enumerator:
    enumeration-constant attribute-specifier-sequence_opt
    enumeration-constant attribute-specifier-sequence_opt = constant-expression

enum-type-specifier:
    : specifier-qualifier-list
```

The `, }` of the second line permits *a trailing comma* — a practical courtesy that
keeps diffs clean when adding an item to a list.

`enum-type-specifier` is C23's addition, letting the underlying type of an enumeration
be settled.

```c
enum status : uint8_t { OK = 0, FAIL = 1 };   /* exactly one byte */
```

It pays where the size is a contract, as in protocol structs (chapter 45).

*Atomic types and typeof.*

```text
atomic-type-specifier:
    _Atomic ( type-name )

typeof-specifier:
    typeof ( typeof-specifier-argument )
    typeof_unqual ( typeof-specifier-argument )

typeof-specifier-argument:
    expression
    type-name
```

That `_Atomic` appears in two places is easy to confuse — used as a *qualifier* it is
`_Atomic int x;`, and as a *type specifier* `_Atomic(int) x;` (chapter 69).

`typeof` was used as a GCC extension for over thirty years and became standard in C23.
`typeof_unqual` is the edition with `const` and `volatile` stripped off, which is
especially useful when making a temporary variable inside a macro.

*Declarators — that structure of chapter 55.*

```text
declarator:
    pointer_opt direct-declarator

direct-declarator:
    identifier attribute-specifier-sequence_opt
    ( declarator )
    array-declarator attribute-specifier-sequence_opt
    function-declarator attribute-specifier-sequence_opt

array-declarator:
    direct-declarator [ type-qualifier-list_opt assignment-expression_opt ]
    direct-declarator [ static type-qualifier-list_opt assignment-expression ]
    direct-declarator [ type-qualifier-list static assignment-expression ]
    direct-declarator [ type-qualifier-list_opt * ]

function-declarator:
    direct-declarator ( parameter-type-list_opt )

pointer:
    * attribute-specifier-sequence_opt type-qualifier-list_opt
    * attribute-specifier-sequence_opt type-qualifier-list_opt pointer

type-qualifier-list:
    type-qualifier
    type-qualifier-list type-qualifier

parameter-type-list:
    parameter-list
    parameter-list , ...
    ...

parameter-list:
    parameter-declaration
    parameter-list , parameter-declaration

parameter-declaration:
    attribute-specifier-sequence_opt declaration-specifiers declarator
    attribute-specifier-sequence_opt declaration-specifiers
        abstract-declarator_opt
```

The reading learned by hand in chapter 55 is in these rules. `*` attaches *in front*
(`pointer_opt direct-declarator`), `[]` and `()` attach *behind* (both
`array-declarator` and `function-declarator` begin with `direct-declarator`), and
binding with parentheses makes what is inside a declarator first (`( declarator )`).
That the rule "the right is stronger than the left" came from the shape of the grammar
can be confirmed here.

The four lines of `array-declarator` hold the special syntax of array parameters
(chapter 37).

- The `static` of the second and third lines — `void f(int a[static 10])` is the
  contract "there are at least 10".
- The `*` of the fourth line — `int a[*]` is the "array of variable length" notation
  used only in a prototype.
- `type-qualifier-list` — `void f(int a[const 10])` is the notation for qualifying (what
  is really) the pointer in a parameter position.

The third line of `parameter-type-list` (`...` alone) is C23's change. Now variadic
arguments can be taken with no named parameter at all, as in `int f(...)` (the partner
of chapter 53's relaxation of `va_start`).

One thing more — *the old-style (K&R) function definition has vanished from the
grammar.* The `identifier-list` that existed up to C17 (`int f(a, b) int a, b; { }`) was
removed in C23, and empty parentheses `f()` now mean "takes no arguments" (the same as
`f(void)`) (chapter 12).

*Type names and abstract declarators.*

```text
type-name:
    specifier-qualifier-list abstract-declarator_opt

abstract-declarator:
    pointer
    pointer_opt direct-abstract-declarator

direct-abstract-declarator:
    ( abstract-declarator )
    array-abstract-declarator attribute-specifier-sequence_opt
    function-abstract-declarator attribute-specifier-sequence_opt

array-abstract-declarator:
    direct-abstract-declarator_opt [ type-qualifier-list_opt
        assignment-expression_opt ]
    direct-abstract-declarator_opt [ static type-qualifier-list_opt
        assignment-expression ]
    direct-abstract-declarator_opt [ type-qualifier-list static
        assignment-expression ]
    direct-abstract-declarator_opt [ * ]

function-abstract-declarator:
    direct-abstract-declarator_opt ( parameter-type-list_opt )

typedef-name:
    identifier
```

An *abstract declarator* is a declarator with the name left out — the one written in
casts and in `sizeof`. The knack stated in chapter 55 ("lay a name in the place where
the name ought to be and read") is expressed in the grammar like this: the abstract
declarator is the edition with nothing where `identifier` was in the declarator rules.

*Initialisation.*

```text
braced-initializer:
    { }
    { initializer-list }
    { initializer-list , }

initializer:
    assignment-expression
    braced-initializer

initializer-list:
    designation_opt initializer
    initializer-list , designation_opt initializer

designation:
    designator-list =

designator-list:
    designator
    designator-list designator

designator:
    [ constant-expression ]
    . identifier
```

The empty braces `{ }` of the first line are C23's addition. Formerly `{0}` had to be
written; now `struct point p = { };` says "all zero".

That `designator-list` is a repetition permits nested designated initialisation.

```c
struct config c = { .net.port = 8080, .paths[2] = "/tmp" };
```

The *named-argument* idiom seen in chapter 44 stands on this rule.

*Static assertions and attributes.*

```text
static_assert-declaration:
    static_assert ( constant-expression , string-literal ) ;
    static_assert ( constant-expression ) ;

attribute-specifier-sequence:
    attribute-specifier-sequence_opt attribute-specifier

attribute-specifier:
    [ [ attribute-list ] ]

attribute-list:
    attribute_opt
    attribute-list , attribute_opt

attribute:
    attribute-token attribute-argument-clause_opt

attribute-token:
    standard-attribute
    attribute-prefixed-token

standard-attribute:
    identifier

attribute-prefixed-token:
    attribute-prefix :: identifier

attribute-prefix:
    identifier

attribute-argument-clause:
    ( balanced-token-sequence_opt )

balanced-token-sequence:
    balanced-token
    balanced-token-sequence balanced-token

balanced-token:
    ( balanced-token-sequence_opt )
    [ balanced-token-sequence_opt ]
    { balanced-token-sequence_opt }
    any token other than a parenthesis, a bracket or a brace
```

`static_assert(cond);` without a message is C23's addition (chapter 65).

The attribute syntax came over from C++ and C23 brought it formally into the language.
What the standard settles are `[[deprecated]]`, `[[fallthrough]]`, `[[maybe_unused]]`,
`[[nodiscard]]`, `[[noreturn]]`, `[[unsequenced]]` and `[[reproducible]]`, while
compiler extensions attach a prefix and are written like `[[gnu::packed]]`. The
`[[nodiscard]]` seen in chapter 76 is a product of this syntax.

Thanks to `balanced-token` leaving it open as "any token so long as the brackets
match", the arguments of an extension attribute the standard does not know pass
grammatically too — *ignoring* an unknown attribute is the standard's prescription.

=== A.2.3 Statements

```text
statement:
    labeled-statement
    unlabeled-statement

unlabeled-statement:
    expression-statement
    attribute-specifier-sequence_opt primary-block
    attribute-specifier-sequence_opt jump-statement

primary-block:
    compound-statement
    selection-statement
    iteration-statement

secondary-block:
    statement

label:
    attribute-specifier-sequence_opt identifier :
    attribute-specifier-sequence_opt case constant-expression :
    attribute-specifier-sequence_opt default :

labeled-statement:
    label statement

compound-statement:
    { block-item-list_opt }

block-item-list:
    block-item
    block-item-list block-item

block-item:
    declaration
    unlabeled-statement
    label

expression-statement:
    expression_opt ;
    attribute-specifier-sequence expression ;

selection-statement:
    if ( expression ) secondary-block
    if ( expression ) secondary-block else secondary-block
    switch ( expression ) secondary-block

iteration-statement:
    while ( expression ) secondary-block
    do secondary-block while ( expression ) ;
    for ( expression_opt ; expression_opt ; expression_opt ) secondary-block
    for ( declaration expression_opt ; expression_opt ) secondary-block

jump-statement:
    goto identifier ;
    continue ;
    break ;
    return expression_opt ;
```

C23 restructured the statement grammar. Formerly `labeled-statement` bound label and
statement into one lump; now `label` is taken out separately and put into `block-item`
as well. The effect is one — *a label may sit at the end of a block.*

```c
void f(void) {
    ...
cleanup:            /* a syntax error up to C17 (a statement had to follow) */
}
```

The `goto cleanup` idiom seen in chapter 65 no longer needs a forced `;` after the last
label.

There is more to read in the rest.

*① The empty statement comes out of `expression_opt ;`.* Write only a `;` and it is a
statement.

*② That `for` has two lines* is the result of C99's allowing a declaration in the first
place (`for (int i = 0; …)`). That the scope of that declaration is inside the loop is
settled not by the grammar but by the semantic rules (chapter 41).

*③ That `if`'s `else` is optional* is the source of the dangling `else` problem. By the
grammar alone `if (a) if (b) x; else y;` is ambiguous, and the standard settles in a
separate sentence that "it attaches to the nearest `if`". It is why chapter 30
recommended braces.

*④ A `case` label's value is a `constant-expression`* — so a variable cannot come there,
while chapter 71's `constexpr` constants can.

=== A.2.4 External definitions

```text
translation-unit:
    external-declaration
    translation-unit external-declaration

external-declaration:
    function-definition
    declaration

function-definition:
    attribute-specifier-sequence_opt declaration-specifiers declarator
        function-body

function-body:
    compound-statement
```

The *translation unit* seen in chapter 51 appears here as the grammar's start symbol.
One file, having finished preprocessing, is held against this rule.

Two things are visible in `function-definition`. First, a definition has *one
declarator* only — so several functions cannot be defined at once, as in
`int f(void), g(void) { }`. Second, the body must be a compound statement — so one
function is always a block wrapped in braces.

== A.3 Preprocessing directives

```text
preprocessing-file:
    group_opt

group:
    group-part
    group group-part

group-part:
    if-section
    control-line
    text-line
    # non-directive

if-section:
    if-group elif-groups_opt else-group_opt endif-line

if-group:
    # if constant-expression new-line group_opt
    # ifdef identifier new-line group_opt
    # ifndef identifier new-line group_opt

elif-groups:
    elif-group
    elif-groups elif-group

elif-group:
    # elif constant-expression new-line group_opt
    # elifdef identifier new-line group_opt
    # elifndef identifier new-line group_opt

else-group:
    # else new-line group_opt

endif-line:
    # endif new-line

control-line:
    # include pp-tokens new-line
    # embed pp-tokens new-line
    # define identifier replacement-list new-line
    # define identifier lparen identifier-list_opt ) replacement-list new-line
    # define identifier lparen ... ) replacement-list new-line
    # define identifier lparen identifier-list , ... ) replacement-list new-line
    # undef identifier new-line
    # line pp-tokens new-line
    # error pp-tokens_opt new-line
    # warning pp-tokens_opt new-line
    # pragma pp-tokens_opt new-line
    # new-line

text-line:
    pp-tokens_opt new-line

non-directive:
    pp-tokens new-line

lparen:
    a ( character not immediately preceded by white space

replacement-list:
    pp-tokens_opt

pp-tokens:
    preprocessing-token
    pp-tokens preprocessing-token

new-line:
    the new-line character

identifier-list:
    identifier
    identifier-list , identifier
```

Here is "the second language that does not know C's grammar" seen in chapter 52. Three
things come out of the rules.

*① Why `lparen` is defined separately.* The condition "a `(` not immediately preceded by
white space" divides function-like macros from object-like macros.

```c
#define F(x) ((x)+1)      /* function-like — F(3) is substituted */
#define G (x) ((x)+1)     /* object-like — G is replaced by "(x) ((x)+1)" */
```

It is a rare place where one space changes the meaning, and a mine every beginner steps
on once.

*② `# new-line` (the null directive) is legal.* A line with only `#` does nothing.

*③ C23 added three.* `#elifdef`/`#elifndef` (formerly one had to write
`#elif defined(X)`), `#warning` (the standardisation of an old practice), and `#embed`.

*`#embed` and the conditional-inspection operators.*

```text
pp-parameter:
    pp-parameter-name pp-parameter-clause_opt

pp-parameter-name:
    pp-standard-parameter
    pp-prefixed-parameter

pp-standard-parameter:
    identifier

pp-prefixed-parameter:
    identifier :: identifier

pp-parameter-clause:
    ( pp-balanced-token-sequence_opt )

pp-balanced-token-sequence:
    pp-balanced-token
    pp-balanced-token-sequence pp-balanced-token

pp-balanced-token:
    ( pp-balanced-token-sequence_opt )
    [ pp-balanced-token-sequence_opt ]
    { pp-balanced-token-sequence_opt }
    any pp-token other than a parenthesis, a bracket or a brace

pp-parameters:
    pp-parameter
    pp-parameters pp-parameter

defined-macro-expression:
    defined identifier
    defined ( identifier )

h-preprocessing-token:
    any preprocessing-token other than >

h-pp-tokens:
    h-preprocessing-token
    h-pp-tokens h-preprocessing-token

header-name-tokens:
    string-literal
    < h-pp-tokens >

has-include-expression:
    __has_include ( header-name )
    __has_include ( header-name-tokens )

has-embed-expression:
    __has_embed ( header-name pp-parameters_opt )
    __has_embed ( header-name-tokens pp-parameters_opt )

has-c-attribute-express:
    __has_c_attribute ( pp-tokens )

va-opt-replacement:
    __VA_OPT__ ( pp-tokens_opt )
```

*`#embed`* may be C23's most practical addition. It plants a file's *contents* in place
as a list of integers.

```c
static const unsigned char logo[] = {
#embed "logo.png"
};
```

Formerly this work required generating source with a separate tool (something like
`xxd -i`) and wedging it into the build. It is common where fonts, icons and
certificates go into firmware. The parameter syntax (`limit(…)`, `prefix(…)`,
`if_empty(…)`) is the `pp-parameter` above, and extensions attach a prefix such as
`gnu::`.

*`__has_include`* is the one already used in chapter 70 — it asks whether a header
exists and takes another road if not. `__has_c_attribute` asks about attribute support
and `__has_embed` about whether `#embed` is possible.

*`__VA_OPT__`* is the helper for variadic macros C23 brought in. It puts something in
"only when the variadic arguments are not empty".

```c
#define LOG(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
LOG("hi");              /* printf("hi")     — no comma attaches */
LOG("%d", 42);          /* printf("%d", 42) — a comma attaches */
```

The place that leaned on a GCC extension (`, ##__VA_ARGS__`) because of the comma left
over when there are no arguments has been tidied into the standard. proven's formatting
macros seen in chapter 80 use this.

== What the grammar cannot answer

#realcase[
  The type-name problem and the lexer hack
][
  The ambiguity seen in chapter 16 shows this grammar's limit.

  ```c
  A * B;
  ```

  By the grammar alone this may be an *expression statement* (the product of A and B)
  or a *declaration* (a pointer B to A). Two rules accept the same token sequence. C
  resolves this conflict outside the grammar — it decides by looking at whether `A` *has
  already been declared* as a type name made with `typedef`. So a C parser cannot work
  by the grammar alone and must converse with the name table (the symbol table) — the
  implementation practice called the "lexer hack" came from here.

  The grammar settles the shape and semantic analysis settles the rest — this one line
  explains why a compiler is divided into several stages (chapter 16).
]

Organising what the grammar does not settle gives this.

#dtable(
  columns: 2,
  [*what the grammar settles*], [*what is settled outside the grammar*],
  [whether an arrangement of tokens is legal], [what a name refers to (scope, linkage)],
  [what binds to what (precedence)], [the order in time of evaluation (chapters 20 and 32)],
  [that an expression comes in the `case` place], [the constraint that it be constant],
  [the shape of a declaration], [whether the types match],
  [the two possible readings of `A * B;`], [whether `A` is a type name],
)

== How to find the grammar in the standard document

In the C standard (and in the freely obtainable drafts, see appendix D) the grammar is
in two places. Each section of the body carries the rules that section treats, and
*annex A* gathers the whole grammar in one place — the annex this appendix has followed.
When confirming in practice "may this be written in this place", opening the annex is
quicker; when you want to know "why", read the corresponding section of the body and its
constraints clause.

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [BNF], [`::=`, `|`, nonterminals and terminals. repetition by recursion],
    [EBNF], [`[ ]` option, `{ }` repetition, `( )` grouping — written without recursion],
    [the C standard's notation], [indented lines are alternatives, `_opt` may be omitted, `one of` lists terminals],
    [two layers], [lexical grammar (making tokens) + phrase structure grammar (weaving them). preprocessing is separate],
    [the expression hierarchy], [the precedence table is a summary of this hierarchy. assignment is right-associative, its left side a unary expression],
    [the declarator rules], [`*` in front, `[]` and `()` behind — the ground of chapter 55's reading],
    [C23's changes], [`_BitInt`, binary literals, digit separators, `enum : T`, empty `{ }`, label position, `#embed`, `__VA_OPT__`],
    [the grammar's limit], [`A * B;` — it divides only if the type name is known],
  )
]
