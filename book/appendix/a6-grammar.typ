#import "../lib.typ": *

= 부록 F — 문법을 적는 법(EBNF)과 C 문법 참고

이 부록은 두 가지를 담는다. 프로그래밍 언어의 문법을 *적는 표기법*인
EBNF와, 그 표기로 정리한 *C 문법의 뼈대*다. 16장에서 본 파서가 실제로
무엇을 보고 일하는지, 그리고 표준 문서의 문법 절을 어떻게 읽는지가 여기서
풀린다.

== 문법을 적는 표기 — BNF에서 EBNF로

언어의 문법은 *규칙의 목록*으로 적을 수 있다. "문장은 이런 것들로 이루어지고,
그 각각은 또 이런 것들로 이루어진다"를 끝까지 밀고 내려가는 방식이다.

#idx("BNF")이 방식을 처음 널리 쓴 것이 *BNF*(Backus–Naur Form)다. 1960년의
ALGOL 60 보고서에서 쓰였고, 두 사람(존 배커스, 페테르 나우르)의 이름이
붙었다. 표기는 단순하다.

```text
<digit>  ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
<number> ::= <digit> | <digit> <number>
```

읽는 법은 이렇다. `::=`는 "~는 다음과 같이 이루어진다", `|`는 "또는",
꺾쇠 안의 이름은 *비단말*(다른 규칙으로 더 풀리는 것), 그냥 적힌 글자는
*단말*(더 풀리지 않는 실제 글자)이다. 두 번째 줄은 재귀로 "숫자 하나, 또는
숫자 하나 뒤에 다시 수"라고 적어 *한 자리 이상의 수*를 정의한다.

#idx("EBNF")BNF에는 반복과 선택을 적을 때마다 재귀를 써야 하는 불편이 있다.
그래서 확장한 것이 *EBNF*(Extended BNF)이고, 기호 몇 개가 더해졌다.

#dtable(
  columns: 3,
  [*표기*], [*뜻*], [*예*],
  [`=` 또는 `::=`], [정의한다], [`digit = "0" | "1" ;`],
  [`|`], [또는(선택)], [`sign = "+" | "-" ;`],
  [`[ … ]`], [있어도 되고 없어도 된다(0 또는 1회)], [`[ sign ] number`],
  [`{ … }`], [0회 이상 반복], [`digit { digit }`],
  [`( … )`], [묶음], [`( "+" | "-" ) digit`],
  [`" … "`], [단말(글자 그대로)], [`";"`],
  [`;`], [규칙의 끝], [—],
)

같은 "한 자리 이상의 수"를 EBNF로 적으면 재귀가 사라진다.

```text
number = digit { digit } ;
```

표준화된 EBNF(ISO/IEC 14977)가 있지만, 실제 문서들은 조금씩 다른 방언을
쓴다. 흔한 변형이 정규식에서 온 후위 기호들이다 — `?`(0 또는 1회),
`*`(0회 이상), `+`(1회 이상). 이 표기로 적으면 이렇게 된다.

```text
number = digit+ ;
integer = ("+" | "-")? digit+ ;
```

*C 표준 문서는 EBNF를 쓰지 않는다.* 대신 BNF에 가까운 표기에
"`opt`"라는 아래첨자로 생략 가능을 나타낸다. 예컨대 표준의 반복문 규칙은
대략 이런 모습이다.

```text
iteration-statement:
    while ( expression ) statement
    do statement while ( expression ) ;
    for ( expression_opt ; expression_opt ; expression_opt ) statement
```

읽는 요령은 같다 — 들여쓴 각 줄이 하나의 선택지이고, `_opt`가 붙은 것은
없어도 된다는 뜻이다. `for (;;)`가 왜 무한 루프로 성립하는지가 이 규칙
한 줄에 그대로 적혀 있다(세 자리가 모두 생략 가능하다).

#qa[
  문법을 이렇게 적어 두면 무엇이 좋은가?
][
  세 가지가 좋아진다.

  첫째, *모호함이 드러난다.* 자연어로 "조건 뒤에 문장이 온다"고 쓰면
  `if (a) if (b) x; else y;`에서 `else`가 어느 `if`에 붙는지 알 수 없지만,
  문법으로 적으면 그 모호함이 규칙의 충돌로 드러난다(C는 "가장 가까운
  `if`에 붙는다"로 정한다).

  둘째, *도구가 읽을 수 있다.* 파서를 손으로 짜지 않고 문법에서 생성하는
  도구들(yacc/bison, ANTLR 같은 것)이 이 표기를 입력으로 받는다.

  셋째, *사람이 확인할 수 있다.* "이 자리에 이것을 적어도 되는가"라는
  질문에 표준 문서가 답하는 방식이 바로 이 문법 절이다.
]

== C 문법의 뼈대

C 표준의 문법은 부록 하나를 통째로 차지할 만큼 길다(선언 문법만 수십 개
규칙이다). 여기서는 *구조를 보는 데 필요한 만큼*을 EBNF 방언으로 간추린다.
정확한 정의는 표준 문서(부록 D의 초안 링크)를 보면 된다.

=== 번역 단위와 선언

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

이 몇 줄이 40장에서 정리한 네 축의 문법적 근거다 — 저장 클래스 지정자가
*선언 지정자의 한 종류*로 들어 있고, 그래서 `typedef`가 `static`과 같은
자리를 다툰다는 것이 규칙에 그대로 보인다.

=== 선언자 — 52장의 그 구조

```text
declarator        = pointer? direct-declarator ;
pointer           = ( "*" type-qualifier* )+ ;

direct-declarator = identifier
                  | "(" declarator ")"
                  | direct-declarator "[" assignment-expression? "]"
                  | direct-declarator "(" parameter-type-list? ")" ;
```

52장에서 손으로 익힌 독법이 이 네 줄에 들어 있다. `*`는 *앞에* 붙고
(`pointer? direct-declarator`), `[]`와 `()`는 *뒤에* 붙으며
(`direct-declarator "[" … "]"`), 괄호로 묶으면 그 안이 먼저 선언자가 된다
(`"(" declarator ")"`). "오른쪽이 왼쪽보다 세다"는 규칙이 문법의 모양에서
나온 것임을 여기서 확인할 수 있다.

=== 문장

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

읽을거리가 몇 있다. *빈 문장*이 `expression? ";"`에서 나온다(`;`만 적어도
문장이다). *`for`가 두 줄인 것*은 C99에서 첫 자리에 선언을 쓸 수 있게 된
결과다(`for (int i = 0; …)`). 그리고 `if`의 `else`가 `[ … ]`로 선택인 것이
위에서 말한 매달린 `else` 문제의 출처다.

=== 식 — 우선순위가 문법에 새겨져 있다

C의 연산자 우선순위는 표로 외우는 것이 아니라 *문법의 계층*으로 정의된다.
아래로 갈수록 더 강하게 묶인다.

```text
expression            = assignment-expression { "," assignment-expression } ;
assignment-expression = conditional-expression
                      | unary-expression assignment-operator assignment-expression ;
conditional-expression = logical-OR-expression
                       [ "?" expression ":" conditional-expression ] ;
logical-OR-expression  = logical-AND-expression  { "||" logical-AND-expression } ;
logical-AND-expression = inclusive-OR-expression { "&&" inclusive-OR-expression } ;
   … (비트 연산, 상등, 관계, 시프트) …
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

이 사다리가 부록 A의 우선순위 표와 정확히 같은 내용이다. 표는 외우기 위한
요약이고, 문법은 그 표가 어디서 왔는지를 보여 준다 — 곱셈이 덧셈보다 강한
이유는 "덧셈 식이 곱셈 식들로 이루어진다"고 정의됐기 때문이다.

*대입이 오른쪽 결합인 이유*도 여기 보인다. `assignment-expression`의
오른쪽 항이 다시 `assignment-expression`이라서 `a = b = c`가
`a = (b = c)`로 묶인다. 반대로 덧셈은 `{ … }` 반복이라 왼쪽부터 묶인다.

#realcase[
  문법이 답하지 못하는 것 — 타입 이름 문제
][
  16장에서 본 모호함이 이 문법의 한계를 보여 준다.

  ```c
  A * B;
  ```

  문법만 보면 이것은 *식 문장*(A와 B의 곱)일 수도, *선언*(A를 가리키는
  포인터 B)일 수도 있다. 두 규칙이 같은 토큰 열을 받아들이는 것이다.
  C는 이 충돌을 문법 밖에서 해결한다 — `A`가 `typedef`로 만든 타입 이름
  으로 *이미 선언되어 있는가*를 보고 정한다. 그래서 C 파서는 문법만으로는
  동작할 수 없고, 이름표(심벌 테이블)와 대화해야 한다("lexer hack"이라
  불리는 구현 관행이 여기서 나왔다).

  문법은 형태를 정하고, 의미 분석이 나머지를 정한다 — 컴파일러가 왜 여러
  단계로 나뉘는지(16장)를 이 한 줄이 설명한다.
]

== 표준 문서에서 문법을 찾는 법

C 표준(그리고 무료로 받을 수 있는 초안, 부록 D 참고)에는 문법이 두 자리에
있다. 본문의 각 절에 그 절이 다루는 규칙이 붙어 있고, 부록에 *전체 문법*이
한데 모여 있다. 실무에서 "이 자리에 이렇게 적어도 되는가"를 확인할 때는
후자를 펴는 것이 빠르다.

읽을 때 알아 둘 두 가지. 첫째, 표준의 문법은 *어휘 문법*(토큰을 만드는 규칙)과
*구문 문법*(토큰을 엮는 규칙)이 나뉘어 있다 — 16장에서 본 렉서와 파서의
구분 그대로다. 둘째, 전처리기는 *별도의 문법*을 갖는다. `#include`나 `#define`
줄은 위의 C 문법에 나오지 않는다(47장의 번역 단계에서 이미 처리되어
사라지기 때문이다).

#recap[
  #dtable(
    columns: 2,
    [*기억할 것*], [*요지*],
    [BNF], [`::=`, `|`, 비단말과 단말. 반복은 재귀로],
    [EBNF], [`[ ]` 선택, `{ }` 반복, `( )` 묶음 — 재귀 없이 적는다],
    [C 표준의 표기], [BNF 계열 + `opt` 아래첨자],
    [선언자 규칙], [`*`는 앞, `[]`·`()`는 뒤 — 52장 독법의 근거],
    [식의 계층], [우선순위 표는 이 계층의 요약],
    [문법의 한계], [`A * B;` — 타입 이름을 알아야 갈린다],
    [전처리기], [별도 문법. C 문법에는 나오지 않는다],
  )
]
