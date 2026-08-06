#import "../lib.typ": *

= 부록 E — 문법을 적는 법(EBNF)과 C 문법 전문

이 부록은 두 가지를 담는다. 프로그래밍 언어의 문법을 *적는 표기법*인
EBNF와, 그 표기로 읽는 *C 문법 전체*다. 16장에서 본 파서가 실제로
무엇을 보고 일하는지, 그리고 표준 문서의 문법 절을 어떻게 읽는지가 여기서
풀린다.

뒤쪽 절들은 C23 표준(ISO/IEC 9899:2024, 무료 초안 N3220)의 *부속서 A*를
그대로 따라간다 — 어휘 문법, 구문 문법, 전처리 지시문의 순서다. 규칙은
표준의 것을 옮기되, 절마다 "이 규칙이 실제로 무엇을 허용하는가"를 덧붙였다.

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
`*`(0회 이상), `+`(1회 이상).

== C 표준의 표기법 읽는 법

*C 표준 문서는 EBNF를 쓰지 않는다.* 대신 BNF에 가까운 표기를 쓰며, 규칙은
이런 모양이다.

```text
iteration-statement:
    while ( expression ) secondary-block
    do secondary-block while ( expression ) ;
    for ( expression_opt ; expression_opt ; expression_opt ) secondary-block
```

네 가지 규약만 알면 표준의 문법 절을 그대로 읽을 수 있다.

#dtable(
  columns: 2,
  [*규약*], [*뜻*],
  [`이름:` 뒤 들여쓴 각 줄], [하나의 선택지(대안). `|`를 줄바꿈으로 대신한다],
  [`_opt` (표준에서는 아래첨자 opt)], [그 요소는 없어도 된다],
  [`one of` 다음 목록], [나열된 것 중 하나. 전부 단말이다],
  [기울임체 이름], [비단말. 다른 규칙으로 풀린다],
)

이 부록에서는 아래첨자를 쓸 수 없으므로 표준의 opt를 `_opt`로 적는다.
`for (;;)`가 왜 무한 루프로 성립하는지가 위 규칙 한 줄에 그대로 적혀 있다 —
세 자리가 모두 생략 가능하기 때문이다.

문법이 두 층으로 나뉘어 있다는 점도 미리 알아 두면 좋다. *어휘 문법*은
글자를 모아 토큰을 만드는 규칙이고, *구문 문법*은 그 토큰을 엮는 규칙이다 —
16장에서 본 렉서와 파서의 구분 그대로다. 전처리기는 아예 별도의 문법을
갖는다(52장의 번역 단계에서 이미 처리되어 사라지므로, 아래 A.2의 C 문법에는
`#include` 같은 것이 나오지 않는다).

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

== A.1 어휘 문법

=== A.1.1 어휘 요소

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
    앞의 어느 것도 아닌 공백 아닌 문자 하나
```

두 규칙의 차이가 52장에서 본 번역 단계의 경계다. *전처리 토큰*은 아직 C의
낱말이 아니다 — `#include <stdio.h>`의 `<stdio.h>`가 헤더 이름 토큰으로
잡히는 것도, `123abc` 같은 것이 `pp-number`로 통째로 잡히는 것도 이 층의
일이다. 그 뒤 번역 단계 7에서 각 전처리 토큰이 *토큰*으로 바뀌는데, 이때
비로소 "이건 정수 상수가 아니다" 같은 진단이 나온다.

마지막 줄("어느 것도 아닌 문자 하나")이 중요하다. `@`나 `$`처럼 C가 쓰지
않는 글자도 *전처리 토큰으로는 존재한다.* 그래서 매크로 안에서는 그런
글자가 지나갈 수 있고, 최종 코드에 남으면 그때 오류가 된다.

=== A.1.2 키워드

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

69장에서 다룬 승격이 이 목록에 그대로 보인다. `bool`·`true`·`false`·
`nullptr`·`static_assert`·`alignas`·`thread_local`·`constexpr`·`typeof`가
*키워드*로 올라와 있고, 그 아래 밑줄 이름들(`_Bool`, `_Alignas`, …)이
옛 코드를 위해 그대로 남아 있다. 두 이름은 같은 것을 가리킨다.

`_BitInt`도 눈에 띈다 — C23이 들여온 *비트 폭을 지정하는 정수*로,
`_BitInt(5)`처럼 쓴다. 하드웨어 레지스터 필드나 고정소수점 연산처럼
"정확히 n비트"가 필요한 자리를 위한 것이다.

=== A.1.3 식별자

```text
identifier:
    identifier-start
    identifier identifier-continue

identifier-start:
    nondigit
    XID_Start 성질을 갖는 문자
    XID_Start 성질을 갖는 universal-character-name

identifier-continue:
    digit
    nondigit
    XID_Continue 성질을 갖는 문자
    XID_Continue 성질을 갖는 universal-character-name

nondigit: one of
    _ a b c d e f g h i j k l m n o p q r s t u v w x y z
      A B C D E F G H I J K L M N O P Q R S T U V W X Y Z

digit: one of
    0 1 2 3 4 5 6 7 8 9
```

첫 규칙의 재귀가 "첫 글자는 숫자가 아니어야 한다"는 그 익숙한 규칙이다 —
시작에는 `identifier-start`만 올 수 있고 `digit`은 `identifier-continue`
에만 있다.

C23이 바꾼 것은 *유니코드 처리*다. 예전 표준은 허용되는 문자를 목록으로
적어 두었지만, C23은 유니코드의 XID_Start/XID_Continue 성질을 참조한다.
그래서 한글로 된 식별자가 원리적으로는 가능하다. 다만 9장에서 본
이유들(인코딩, 정규화, 보이지 않는 글자) 때문에 실무 관행은 여전히
아스키다.

=== A.1.4 보편 문자 이름

```text
universal-character-name:
    \u hex-quad
    \U hex-quad hex-quad

hex-quad:
    hexadecimal-digit hexadecimal-digit hexadecimal-digit hexadecimal-digit
```

소스 파일의 인코딩과 무관하게 특정 코드포인트를 적는 방법이다(9장).
`"é"`는 파일이 무슨 인코딩이든 U+00E9를 뜻한다.

=== A.1.5 상수

```text
constant:
    integer-constant
    floating-constant
    enumeration-constant
    character-constant
    predefined-constant
```

*정수 상수.*

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

세 가지를 짚어 둔다.

*① `0`으로 시작하면 8진이다.* `octal-constant`의 첫 줄이 그 규칙이고,
`017`이 15가 되는 이유가 여기 있다. 날짜나 시각을 `08`처럼 적어 두었다가
컴파일 오류를 만나는 고전적 실수의 출처다(`8`은 8진 숫자가 아니다).

*② C23이 2진 리터럴과 자릿수 구분자를 들였다.* `0b1010'0110`처럼 적을 수
있다 — 규칙에 보이는 `'_opt`가 그 구분자다. 27장의 비트 연산을 적을 때
훨씬 읽기 좋아진다.

*③ 접미사에 `wb`가 생겼다.* `_BitInt` 상수를 위한 것이다(`123wb`).

*부동소수점 상수.*

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

`fractional-constant`의 둘째 줄이 `1.`을 허용하고, 첫 줄의 `_opt`가 `.5`를
허용한다. 그리고 지수부가 있으면 소수점이 없어도 된다(`1e9`).

*16진 부동소수점*(`0x1.8p3`)은 C99가 들인 것으로, 8장에서 본 비트 표현을
정확히 적고 읽는 데 쓴다 — 십진으로 적으면 반올림이 끼어들지만 이 표기는
표현을 그대로 옮긴다. 지수부 `p`가 *2의 거듭제곱*이라는 점이 요령이다.

접미사의 `df`·`dd`·`dl`은 십진 부동소수점(`_Decimal32/64/128`)의 것으로,
금액 계산처럼 십진 반올림이 요구되는 자리를 위한 선택 기능이다.

*문자 상수와 미리 정의된 상수.*

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
    작은따옴표·역슬래시·개행을 제외한 소스 문자 집합의 아무 문자
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

`hexadecimal-escape-sequence`의 재귀가 유명한 함정 하나를 설명한다 —
*자릿수에 상한이 없다.* `"\x41BC"`는 `\x41` 다음에 `BC`가 아니라
`\x41BC` 하나로 읽히고, 값이 `char`에 안 들어가면 계약 밖이다. 8진
이스케이프는 반대로 최대 세 자리로 끊긴다.

`predefined-constant`가 별도 규칙으로 있는 것이 C23의 변화다(69장) —
`true`·`false`·`nullptr`이 매크로가 아니라 *문법 요소*가 됐다.

=== A.1.6 문자열 리터럴

```text
string-literal:
    encoding-prefix_opt " s-char-sequence_opt "

s-char-sequence:
    s-char
    s-char-sequence s-char

s-char:
    큰따옴표·역슬래시·개행을 제외한 소스 문자 집합의 아무 문자
    escape-sequence
```

`_opt`가 빈 문자열 `""`을 허용한다. 접두사는 문자 상수와 같은 넷이다 —
`u8"..."`(UTF-8), `u"..."`(UTF-16), `U"..."`(UTF-32), `L"..."`(와이드).
인접한 문자열 리터럴이 하나로 이어지는 것은 문법이 아니라 *번역 단계 6*의
일이다(52장).

=== A.1.7 구두점

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

마지막 줄은 *대체 표기*(digraph)다. `<:`는 `[`, `%:`는 `#`과 같다 —
키보드에 그 글자가 없던 시절의 유물이고, 삼중자(trigraph)와 달리 C23에도
남아 있다.

`::`이 새로 들어온 것은 69장의 속성 문법 때문이다(`[[gnu::packed]]`).

=== A.1.8 헤더 이름

```text
header-name:
    < h-char-sequence >
    " q-char-sequence "

h-char-sequence:
    h-char
    h-char-sequence h-char

h-char:
    개행과 > 를 제외한 소스 문자 집합의 아무 문자

q-char-sequence:
    q-char
    q-char-sequence q-char

q-char:
    개행과 큰따옴표를 제외한 소스 문자 집합의 아무 문자
```

이 토큰은 *전처리 지시문 안에서만* 만들어진다. 그래서 `<stdio.h>` 안의
`/`나 `.`이 나눗셈이나 멤버 접근으로 읽히지 않는다 — 렉서가 문맥에 따라
다르게 자르는 드문 자리다.

=== A.1.9 전처리 수

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

전처리기는 아직 수의 문법을 모른다. 그래서 "숫자로 시작해서 글자·점·부호가
이어지는 덩어리"를 통째로 하나의 토큰으로 잡아 둔다 — `1.2e+3`도,
`0xFFul`도, 심지어 `1abc`도 전부 `pp-number` 하나다. 유효한지는 번역 단계 7
에서 정수·부동소수점 상수 문법에 대어 보며 가려진다.

이 규칙 덕에 매크로 치환이 수를 쪼개지 않고, `1.2e+3` 한가운데의 `+`가
덧셈으로 읽히지도 않는다.

== A.2 구문 문법

=== A.2.1 식

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

이 사다리가 부록 A의 우선순위 표와 *같은 내용*이다. 표는 외우기 위한
요약이고, 문법은 그 표가 어디서 왔는지를 보여 준다 — 곱셈이 덧셈보다 강한
이유는 "덧셈 식이 곱셈 식들로 이루어진다"고 정의됐기 때문이다.

문법에서만 보이는 것 몇 가지를 짚는다.

*① 대입의 왼쪽은 `unary-expression`이다.* 그래서 `a + b = c`는 문법 단계
에서 이미 걸린다 — 좌변이 단항 식이 아니기 때문이다. 반면 `*p = c`나
`a[i] = c`는 통과한다(둘 다 단항 식·후위 식이다).

*② 대입이 오른쪽 결합인 이유가 보인다.* 우변이 다시
`assignment-expression`이라 `a = b = c`가 `a = (b = c)`로 묶인다. 덧셈은
좌재귀(`additive-expression + …`)라 왼쪽부터 묶인다.

*③ 조건 연산자의 가운데는 `expression`이다.* 그래서 `a ? b, c : d`처럼
쉼표 식을 넣을 수 있지만, 마지막 자리는 `conditional-expression`이라
대입을 적으려면 괄호가 필요하다.

*④ `constant-expression`은 문법이 아니라 제약으로 정해진다.* 규칙만 보면
그냥 조건 식이고, "상수여야 한다"는 요구는 별도의 제약 조항에 있다.
배열 크기나 `case` 라벨에 무엇을 적을 수 있는지가 그 조항의 몫이다(23장).

*⑤ 복합 리터럴에 저장 클래스가 붙는다.* `storage-class-specifiers_opt`가
C23의 추가로, `(static int[]){1,2,3}`처럼 수명을 정할 수 있게 됐다
(44장의 블록 수명 함정을 피하는 길이다).

=== A.2.2 선언

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

첫 규칙의 `init-declarator-list_opt`가 `struct point { int x; };`처럼
*선언자 없는 선언*을 허용한다 — 타입만 만들고 변수는 만들지 않는 경우다.

`declaration-specifiers`가 재귀라는 사실이 `unsigned static long const int`
같은 뒤죽박죽 순서를 허용하는 근거다. 문법은 순서를 강제하지 않고 의미도
같지만, 읽는 사람을 위해 관행적 순서를 지키는 것이 좋다.

*저장 클래스와 타입.*

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

41장에서 정리한 네 축이 이 몇 줄에 그대로 있다. `typedef`가
*저장 클래스 지정자의 하나*라는 사실이 여기 보이고(55장), "저장 클래스는
하나만"이라는 요구는 문법이 아니라 제약 조항이 못박는다. `constexpr`이
저장 클래스 자리에 온 것도 69장에서 본 대로다.

*구조체와 공용체.*

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

`member-declarator`의 둘째 줄이 *비트 필드*이고, `declarator_opt`가 이름
없는 비트 필드(`int : 3;` — 자리만 띄우는 채움)를 허용한다(43장).

`member-declaration-list`에 `static_assert-declaration`이 들어 있는 것도
눈여겨볼 만하다. 구조체 안에서 배치를 검사할 수 있다는 뜻이다.

```c
struct header {
    uint32_t magic;
    uint16_t version;
    static_assert(sizeof(uint32_t) == 4, "");   /* 구조체 안에서 */
};
```

*열거.*

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

둘째 줄의 `, }`가 *마지막 쉼표*를 허용한다 — 목록에 항목을 더할 때 차이가
깨끗해지는 실용적인 배려다.

`enum-type-specifier`는 C23의 추가로, 열거의 밑바탕 타입을 정할 수 있게
한다.

```c
enum status : uint8_t { OK = 0, FAIL = 1 };   /* 정확히 1바이트 */
```

프로토콜 구조체처럼 크기가 계약인 자리에서 값을 한다(45장).

*원자적 타입과 typeof.*

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

`_Atomic`이 두 자리에 나온다는 점이 헷갈리기 쉽다 — *한정자*로 쓰면
`_Atomic int x;`이고, *타입 지정자*로 쓰면 `_Atomic(int) x;`다(67장).

`typeof`는 GCC 확장으로 30년 넘게 쓰이다 C23에서 표준이 됐다.
`typeof_unqual`은 `const`·`volatile`을 벗겨 낸 판이라, 매크로에서 임시
변수를 만들 때 특히 쓸모 있다.

*선언자 — 55장의 그 구조.*

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

55장에서 손으로 익힌 독법이 이 규칙들에 들어 있다. `*`는 *앞에* 붙고
(`pointer_opt direct-declarator`), `[]`와 `()`는 *뒤에* 붙으며
(`array-declarator`와 `function-declarator` 모두 `direct-declarator`로
시작한다), 괄호로 묶으면 그 안이 먼저 선언자가 된다(`( declarator )`).
"오른쪽이 왼쪽보다 세다"는 규칙이 문법의 모양에서 나온 것임을 여기서
확인할 수 있다.

`array-declarator`의 네 줄이 배열 매개변수의 특수 문법을 담는다(37장).

- 둘째·셋째 줄의 `static` — `void f(int a[static 10])`은 "적어도 10개는
  있다"는 계약이다.
- 넷째 줄의 `*` — `int a[*]`는 원형에서만 쓰는 "길이가 가변인 배열" 표기다.
- `type-qualifier-list` — `void f(int a[const 10])`처럼 매개변수 자리에서
  (실제로는 포인터인) 그것에 한정자를 붙이는 표기다.

`parameter-type-list`의 셋째 줄(`...`만)이 C23의 변화다. 이제
`int f(...)`처럼 이름 있는 매개변수 없이 가변 인자만 받을 수 있다(53장의
`va_start` 완화와 짝이다).

한 가지 더 — *구식(K&R) 함수 정의가 문법에서 사라졌다.* C17까지 있던
`identifier-list`(`int f(a, b) int a, b; { }`)가 C23에서 제거됐고, 빈 괄호
`f()`도 이제 "인자를 받지 않는다"(`f(void)`와 같다)는 뜻이 됐다(12장).

*타입 이름과 추상 선언자.*

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

*추상 선언자*는 이름이 빠진 선언자다 — 캐스트와 `sizeof`에 적는 그것이다.
55장에서 말한 요령("이름이 있어야 할 자리에 이름을 얹어 놓고 읽는다")이
문법으로는 이렇게 표현된다: 선언자 규칙에서 `identifier`가 있던 자리에
아무것도 없는 판이 추상 선언자다.

*초기화.*

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

첫 줄의 빈 중괄호 `{ }`가 C23의 추가다. 예전에는 `{0}`이라고 적어야 했지만
이제 `struct point p = { };`로 "전부 0"을 적을 수 있다.

`designator-list`가 반복이라는 점이 중첩 지정 초기화를 허용한다.

```c
struct config c = { .net.port = 8080, .paths[2] = "/tmp" };
```

44장에서 본 *이름 붙은 인자* 관용구가 이 규칙 위에 서 있다.

*정적 단언과 속성.*

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
    괄호·대괄호·중괄호가 아닌 아무 토큰
```

메시지 없는 `static_assert(cond);`가 C23의 추가다(65장).

속성 문법은 C++에서 건너온 것으로, C23이 언어에 정식으로 들였다. 표준이
정한 것은 `[[deprecated]]`, `[[fallthrough]]`, `[[maybe_unused]]`,
`[[nodiscard]]`, `[[noreturn]]`, `[[unsequenced]]`, `[[reproducible]]`이고,
컴파일러 확장은 접두사를 붙여 `[[gnu::packed]]`처럼 적는다. 74장에서 본
`[[nodiscard]]`가 이 문법의 산물이다.

`balanced-token`이 "괄호만 짝이 맞으면 아무 토큰이나"라고 열어 둔 덕에,
표준이 모르는 확장 속성의 인자도 문법적으로는 통과한다 — 모르는 속성은
*무시하는 것*이 표준의 규정이다.

=== A.2.3 문장

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

C23이 문장 문법을 재구성했다. 예전에는 `labeled-statement`가 라벨과 문장을
한 덩어리로 묶었는데, 지금은 `label`을 따로 떼어 `block-item`에도 넣었다.
효과는 하나다 — *블록 끝에 라벨을 둘 수 있다.*

```c
void f(void) {
    ...
cleanup:            /* C17 까지는 문법 오류(뒤에 문장이 있어야 했다) */
}
```

65장에서 본 `goto cleanup` 관용구가 마지막 라벨 뒤에 억지로 `;`를 넣지
않아도 되게 됐다.

나머지도 읽을거리가 있다.

*① 빈 문장은 `expression_opt ;`에서 나온다.* `;`만 적어도 문장이다.

*② `for`가 두 줄인 것*은 C99에서 첫 자리에 선언을 쓸 수 있게 된
결과다(`for (int i = 0; …)`). 그 선언의 유효범위가 루프 안이라는 것은
문법이 아니라 의미 규칙이 정한다(41장).

*③ `if`의 `else`가 선택*이라는 사실이 매달린 `else` 문제의 출처다. 문법만
으로는 `if (a) if (b) x; else y;`가 모호하고, 표준이 별도 문장으로 "가장
가까운 `if`에 붙는다"고 정한다. 30장에서 중괄호를 권한 이유다.

*④ `case`의 라벨 값은 `constant-expression`이다* — 그래서 변수는 올 수
없고, 69장의 `constexpr` 상수는 올 수 있다.

=== A.2.4 외부 정의

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

51장에서 본 *번역 단위*가 여기서 문법의 시작 기호로 나온다. 파일 하나가
전처리를 마친 뒤 이 규칙에 대어진다.

`function-definition`에서 두 가지가 보인다. 첫째, 정의는 *선언자 하나*만
갖는다 — 그래서 `int f(void), g(void) { }`처럼 여러 함수를 한 번에 정의할
수 없다. 둘째, 본문은 반드시 복합 문장이다 — 그래서 함수 하나는 언제나
중괄호로 감싼 블록이다.

== A.3 전처리 지시문

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
    바로 앞에 공백이 없는 ( 문자

replacement-list:
    pp-tokens_opt

pp-tokens:
    preprocessing-token
    pp-tokens preprocessing-token

new-line:
    개행 문자

identifier-list:
    identifier
    identifier-list , identifier
```

여기가 52장에서 본 "C의 문법을 모르는 두 번째 언어"다. 규칙에서 세 가지가
드러난다.

*① `lparen`이 따로 정의된 이유.* "바로 앞에 공백이 없는 `(`"라는 조건이
함수형 매크로와 객체형 매크로를 가른다.

```c
#define F(x) ((x)+1)      /* 함수형 — F(3) 이 치환된다 */
#define G (x) ((x)+1)     /* 객체형 — G 가 "(x) ((x)+1)" 로 치환된다 */
```

공백 하나가 뜻을 바꾸는 드문 자리이고, 초보자가 한 번은 밟는 지뢰다.

*② `# new-line`(널 지시문)이 합법이다.* `#`만 있는 줄은 아무 일도 하지
않는다.

*③ C23이 셋을 더했다.* `#elifdef`/`#elifndef`(그전에는
`#elif defined(X)`라고 적어야 했다), `#warning`(오래된 관행의 표준화),
그리고 `#embed`다.

*`#embed`와 조건부 검사 연산자.*

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
    괄호·대괄호·중괄호가 아닌 아무 전처리 토큰

pp-parameters:
    pp-parameter
    pp-parameters pp-parameter

defined-macro-expression:
    defined identifier
    defined ( identifier )

h-preprocessing-token:
    > 가 아닌 아무 preprocessing-token

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

*`#embed`* 가 C23의 가장 실용적인 추가일지 모른다. 파일의 *내용*을 정수
목록으로 그 자리에 박아 넣는다.

```c
static const unsigned char logo[] = {
#embed "logo.png"
};
```

그전에는 이 일을 하려고 별도 도구(`xxd -i` 같은)로 소스를 생성해 빌드에
끼워야 했다. 펌웨어에 폰트·아이콘·인증서를 넣는 자리에서 흔한 일이다.
매개변수 문법(`limit(…)`, `prefix(…)`, `if_empty(…)`)이 위의
`pp-parameter`이고, 확장은 `gnu::` 같은 접두사를 붙인다.

*`__has_include`* 는 68장에서 이미 쓴 그것이다 — 헤더가 있는지 물어보고
없으면 다른 길로 간다. `__has_c_attribute`는 속성 지원 여부를,
`__has_embed`는 `#embed` 가능 여부를 묻는다.

*`__VA_OPT__`* 는 C23이 들인 가변 인자 매크로의 도우미다. "가변 인자가
비어 있지 않을 때만" 무언가를 넣는다.

```c
#define LOG(fmt, ...) printf(fmt __VA_OPT__(,) __VA_ARGS__)
LOG("hi");              /* printf("hi")     — 쉼표가 안 붙는다 */
LOG("%d", 42);          /* printf("%d", 42) — 쉼표가 붙는다 */
```

인자가 없을 때 남는 쉼표 때문에 GCC 확장(`, ##__VA_ARGS__`)에 기대던
자리가 표준으로 정리됐다. 78장에서 본 proven의 형식화 매크로가 이것을
쓴다.

== 문법이 답하지 못하는 것

#realcase[
  타입 이름 문제와 렉서 해킹
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

문법이 정하지 않는 것을 정리하면 이렇다.

#dtable(
  columns: 2,
  [*문법이 정하는 것*], [*문법 밖에서 정하는 것*],
  [토큰의 배열이 합법인가], [이름이 무엇을 가리키는가(유효범위·연결)],
  [무엇이 무엇에 묶이는가(우선순위)], [평가의 시간 순서(20·32장)],
  [`case` 자리에 식이 온다는 것], [그 식이 상수여야 한다는 제약],
  [선언의 형태], [타입이 서로 맞는가],
  [`A * B;`의 두 가지 해석 가능성], [`A`가 타입 이름인지 여부],
)

== 표준 문서에서 문법을 찾는 법

C 표준(그리고 무료로 받을 수 있는 초안, 부록 D 참고)에는 문법이 두 자리에
있다. 본문의 각 절에 그 절이 다루는 규칙이 붙어 있고, *부속서 A*에 전체
문법이 한데 모여 있다 — 이 부록이 따라간 그 부속서다. 실무에서 "이 자리에
이렇게 적어도 되는가"를 확인할 때는 부속서를 펴는 것이 빠르고, "왜 그런가"를
알고 싶으면 본문의 해당 절과 그 제약 조항을 읽는다.

#recap[
  #dtable(
    columns: 2,
    [*기억할 것*], [*요지*],
    [BNF], [`::=`, `|`, 비단말과 단말. 반복은 재귀로],
    [EBNF], [`[ ]` 선택, `{ }` 반복, `( )` 묶음 — 재귀 없이 적는다],
    [C 표준의 표기], [들여쓴 줄이 대안, `_opt`는 생략 가능, `one of`는 단말 나열],
    [두 층], [어휘 문법(토큰 만들기) + 구문 문법(토큰 엮기). 전처리는 별도],
    [식의 계층], [우선순위 표는 이 계층의 요약. 대입은 우결합, 좌변은 단항 식],
    [선언자 규칙], [`*`는 앞, `[]`·`()`는 뒤 — 55장 독법의 근거],
    [C23의 변화], [`_BitInt`·2진 리터럴·자릿수 구분자·`enum : T`·빈 `{ }`·라벨 위치·`#embed`·`__VA_OPT__`],
    [문법의 한계], [`A * B;` — 타입 이름을 알아야 갈린다],
  )
]
