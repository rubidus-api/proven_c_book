#import "../../book/lib.typ": *

= Appendix F — How grammar is written (EBNF) and a C grammar reference

This appendix holds two things. EBNF, the *notation for writing* a programming
language's grammar, and the *skeleton of C's grammar* organised in that notation. What
the parser seen in chapter 16 actually looks at while it works, and how to read the
grammar sections of the standard document, are resolved here.

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
expressions — `?` (0 or 1 time), `*` (0 or more), `+` (1 or more). Written in that
notation it goes like this.

```text
number = digit+ ;
integer = ("+" | "-")? digit+ ;
```

*The C standard document does not use EBNF.* Instead it uses a notation close to BNF
with a subscript "`opt`" marking what may be omitted. The standard's iteration
statement rule, for example, looks roughly like this.

```text
iteration-statement:
    while ( expression ) statement
    do statement while ( expression ) ;
    for ( expression_opt ; expression_opt ; expression_opt ) statement
```

The knack of reading it is the same — each indented line is one alternative, and what
carries `_opt` need not be there. Why `for (;;)` holds as an infinite loop is written in
this one rule as it stands (all three places may be omitted).

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

== The skeleton of C's grammar

The C standard's grammar is long enough to take a whole appendix (the declaration
grammar alone is dozens of rules). Here we condense *as much as is needed to see the
structure* into an EBNF dialect. For the exact definitions, look at the standard
document (the draft links in appendix D).

=== Translation units and declarations

```text
translation-unit  = external-declaration+ ;
external-declaration = function-definition | declaration ;

function-definition = declaration-specifiers declarator compound-statement ;

declaration       = declaration-specifiers init-declarator-list? ";" ;
declaration-specifiers = ( storage-class-specifier
                         | type-specifier
                         | type-qualifier
                         | function-specifier
                         | alignment-specifier )+ ;

storage-class-specifier = "typedef" | "extern" | "static"
                        | "thread_local" | "auto" | "register" ;
type-qualifier    = "const" | "restrict" | "volatile" | "_Atomic" ;
```

These few lines are the grammatical ground of the four axes organised in chapter 39 —
the storage-class specifier is in there as *one kind of declaration specifier*, and so
that `typedef` contends for the same place as `static` is visible in the rules as it
stands.

=== Declarators — that structure of chapter 52

```text
declarator        = pointer? direct-declarator ;
pointer           = ( "*" type-qualifier* )+ ;

direct-declarator = identifier
                  | "(" declarator ")"
                  | direct-declarator "[" assignment-expression? "]"
                  | direct-declarator "(" parameter-type-list? ")" ;
```

The reading learned by hand in chapter 52 is in these four lines. `*` attaches *in
front* (`pointer? direct-declarator`), `[]` and `()` attach *behind*
(`direct-declarator "[" … "]"`), and binding with parentheses makes what is inside a
declarator first (`"(" declarator ")"`). That the rule "the right is stronger than the
left" came from the shape of the grammar can be confirmed here.

=== Statements

```text
statement         = labeled-statement | compound-statement
                  | expression-statement | selection-statement
                  | iteration-statement | jump-statement ;

compound-statement    = "{" ( declaration | statement )* "}" ;
expression-statement  = expression? ";" ;
selection-statement   = "if" "(" expression ")" statement [ "else" statement ]
                      | "switch" "(" expression ")" statement ;
iteration-statement   = "while" "(" expression ")" statement
                      | "do" statement "while" "(" expression ")" ";"
                      | "for" "(" expression? ";" expression? ";" expression? ")" statement
                      | "for" "(" declaration expression? ";" expression? ")" statement ;
jump-statement        = "goto" identifier ";" | "continue" ";"
                      | "break" ";" | "return" expression? ";" ;
```

There are several things to read here. *The empty statement* comes out of
`expression? ";"` (write only a `;` and it is a statement). *That `for` has two lines*
is the result of C99's allowing a declaration in the first place
(`for (int i = 0; …)`). And that `if`'s `else` is optional as `[ … ]` is the source of
the dangling `else` problem mentioned above.

=== Expressions — precedence is carved into the grammar

C's operator precedence is not memorised from a table but defined as *a hierarchy of
the grammar*. The further down, the more strongly it binds.

```text
expression            = assignment-expression { "," assignment-expression } ;
assignment-expression = conditional-expression
                      | unary-expression assignment-operator assignment-expression ;
conditional-expression = logical-OR-expression
                       [ "?" expression ":" conditional-expression ] ;
logical-OR-expression  = logical-AND-expression  { "||" logical-AND-expression } ;
logical-AND-expression = inclusive-OR-expression { "&&" inclusive-OR-expression } ;
   … (bitwise operations, equality, relational, shift) …
additive-expression    = multiplicative-expression { ("+" | "-") multiplicative-expression } ;
multiplicative-expression = cast-expression { ("*" | "/" | "%") cast-expression } ;
cast-expression        = unary-expression | "(" type-name ")" cast-expression ;
unary-expression       = postfix-expression
                       | ("++" | "--") unary-expression
                       | unary-operator cast-expression
                       | "sizeof" ( unary-expression | "(" type-name ")" ) ;
postfix-expression     = primary-expression
                       { "[" expression "]" | "(" argument-list? ")"
                       | "." identifier | "->" identifier | "++" | "--" } ;
primary-expression     = identifier | constant | string-literal
                       | "(" expression ")" | generic-selection ;
```

This ladder is exactly the same content as appendix A's precedence table. The table is
a summary for memorising, and the grammar shows where that table came from — the reason
multiplication is stronger than addition is that "an additive expression is defined as
being made of multiplicative expressions".

*Why assignment is right-associative* is visible here too. The right-hand term of
`assignment-expression` is again an `assignment-expression`, so `a = b = c` binds as
`a = (b = c)`. Addition, by contrast, being a `{ … }` repetition, binds from the left.

#realcase[
  What the grammar cannot answer — the type-name problem
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

== How to find the grammar in the standard document

In the C standard (and in the freely obtainable drafts, see appendix D) the grammar is
in two places. Each section of the body carries the rules that section treats, and an
annex gathers *the whole grammar* in one place. When confirming in practice "may this be
written in this place", opening the latter is quicker.

Two things to know in reading. First, the standard's grammar is divided into *the
lexical grammar* (the rules that make tokens) and *the phrase structure grammar* (the
rules that weave tokens) — exactly the division of lexer and parser seen in chapter 16.
Second, the preprocessor has *a separate grammar*. `#include` and `#define` lines do not
appear in the C grammar above (because they have already been handled and have vanished
in chapter 49's translation phases).

#recap[
  #dtable(
    columns: 2,
    [*to remember*], [*the point*],
    [BNF], [`::=`, `|`, nonterminals and terminals. repetition by recursion],
    [EBNF], [`[ ]` option, `{ }` repetition, `( )` grouping — written without recursion],
    [the C standard's notation], [the BNF family + the `opt` subscript],
    [the declarator rules], [`*` in front, `[]` and `()` behind — the ground of chapter 52's reading],
    [the expression hierarchy], [the precedence table is a summary of this hierarchy],
    [the grammar's limit], [`A * B;` — it divides only if the type name is known],
    [the preprocessor], [a separate grammar. it does not appear in the C grammar],
  )
]
