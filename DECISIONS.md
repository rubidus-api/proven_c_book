# DECISIONS

This is the single append-only accepted decision log.

Use this for non-secret accepted project decisions that should remain traceable.

Read this file only when the current task needs prior decisions, decision rationale, or supersession history.

Do not split decisions into current and old files. If a decision is superseded, append a new entry and mark the older entry as superseded or superseded-by.

Do not store credentials, private infrastructure details, personal data, private remote URLs, or private-only business context here. Put private decisions in the sibling private repository when one is used.

## Template

### YYYY-MM-DD: <decision title>

- Status: Accepted | Superseded
- Context:
- Decision:
- Consequences:
- Supersedes:

### 2026-08-04: 조판 기술 = Typst

- Status: Accepted
- Context: 프로그래밍 입문서 저술에 조판 기술 선정 필요.
- Decision: 원고와 산출(PDF)은 Typst로 고정한다.
- Consequences: 원고는 .typ 소스로 관리하고 `typst compile`이 빌드 검증의 일부가 된다.

### 2026-08-04: 예제 기반 기술 = C23 + proven 라이브러리

- Status: Accepted
- Context: 책의 예제·실습 코드가 딛고 설 기반 선정.
- Decision: 예제는 C23 표준으로 작성하고 proven C 라이브러리를 기반 기술로 사용한다.
- Consequences: 수록 예제는 전부 실제 컴파일·실행 검증 대상이며, proven 사용 방식(vendor vs 참조)은 후속 결정으로 남긴다.

### 2026-08-04: 서술 원칙 — 학습과학 기반·새 구성·"~이다" 문체

- Status: Accepted
- Context: 대상 독자는 프로그래밍 완전 입문자이며, 기존 C 입문서 목차 답습을 거부한다.
- Decision: 전산 기본부터 시작하는 완전히 새로운 구성으로 하고, 최신 교수이론에 근거한 서술 장치(즉각적 질문-답, 잦은 실제 사례, 오개념 선제 교정, 수학적 기반의 정확한 설명)를 채택하며, 문체는 평범한 "~이다/~있다" 어미로 한다. 저자 표기는 rubidus.
- Consequences: 교수 설계와 장 구성은 각각 RFC로 명문화 후 집필에 들어간다.

### 2026-08-04: 학습 방식 = 읽기 전용 문답형 (실습·연습 배제)

- Status: Accepted
- Context: 학습 장치의 형태를 정해야 한다. 통상의 입문서는 장말 연습문제·실습 과제를 둔다.
- Decision: 독자에게 실습이나 연습을 시키지 않는다. 본문 속 문답을 따라 읽는 것만으로 자연스럽게 익혀지도록 서술한다. 외울 것이 있으면 가벼운 복습 정리까지만 허용하고, 그보다는 다음 장에서 이전 학습에 대한 심화된 질문과 답을 제시하는 방식을 우선한다.
- Consequences: 연습문제·과제·"직접 해보라" 지시는 전면 금지된다. 인출 연습과 간격 반복은 장 간 심화 문답 장치로 구현된다. RFC-0001 교수 설계가 이 결정을 전제로 작성된다.

### 2026-08-04: 첫 세 장의 골격과 3장 도구 관점

- Status: Accepted
- Context: 책 구성(RFC-0002)의 출발 골격을 정해야 한다.
- Decision: 1장 배경설명 → 2장 전산의 기본 → 3장 헬로 월드와 개발환경 구축 순서로 확정한다. 3장은 특정 플랫폼 하나를 예시로 제시하되 내용은 일반론을 주로 삼고, 일반적인 컴파일 과정(전처리→컴파일→어셈블→링크)을 바탕으로 gcc·clang 등 기반으로 설명한다.
- Consequences: RFC-0002가 이 골격 위에 이후 부·장 체계를 설계한다. 3장의 예시 플랫폼 선정은 미결로 남긴다.

### 2026-08-04: 2장 "전산의 기본과 배경지식" — 내용 궤적·이중 트랙 서사·대형 비중

- Status: Accepted
- Context: 2장(전산의 기본)의 내용 범위와 서술 방식을 정해야 한다.
- Decision: 2장은 "전산의 기본과 배경지식"으로서 책에서 상당한 분량을 차지한다. 내용 궤적: CPU·메모리·클록의 단순한 모델 → 워드(기계의 자연스러운 크기·32/64비트의 의미)·메모리 주소·리틀/빅 엔디안·0번 주소의 플랫폼별 특별함(임베디드 포함)·정렬 제한·하위 주소 비트 활용(태그 포인터) → 다층 캐시·분기 예측·파이프라인 → 컴파일러 최적화 → C가 추상적인 언어인 이유와 추상적으로 접근해야 하는 이유 → 최근의 엄격한 포인터 제한(프로버넌스) 흐름. 여기에 C와 표준의 역사를 별도 절이 아니라 절 단위로 엮는다(이중 트랙). 문자와 텍스트(EBCDIC·ASCII·ISO 646과 삼중자·ISO 8859 Latin·유니코드·가변 길이 인코딩·UTF-8), 수의 표현(고정소수점·부동소수점·IEEE 754), CPU와 GPU의 차이(지연시간 vs 처리량), 스트림의 기원(펀치카드·라인프린터·프린터 터미널 → C 스트림 모델)도 이 장의 배경지식에 포함한다. 역사 서술은 기능적 역사 원칙을 따른다: 고리타분한 연대기·인물 나열은 빼고, 지금의 C 모양을 설명하는 데 필요한 역사만 쓴다.
- Consequences: 2장이 이후 장들이 심화 문답으로 회수하는 배경지식의 저수지가 된다. 비대해지면 2개 장으로 분할 가능하되 이중 트랙 서사는 유지한다.

### 2026-08-04: 장 체계 rev.b — 4장 최소 도구·5장 선언·나선형 최소 집합·구조체 후행

- Status: Accepted
- Context: 헬로 월드 이후의 전개 순서를 정해야 한다.
- Decision: 3장(헬로 월드) 다음에 4장 "최소한의 도구 상자"(프로그램 구조·수식·입출력 함수 사용 — 정의가 아니라 사용법)를 두고, 그다음 5장 "선언"(변수 선언·함수 선언)으로 간다. 구조체·공용체는 메모리 모델을 갖춘 뒤인 12장으로 미룬다. 서술은 나선형 최소 집합 원리를 따른다: 한 장에 한 주제를 다 넣지 않고, 최소 집합(예: 수식은 기본 산술 `+ - *`와 괄호만)부터 시작해 뒤 장들이 필요해지는 시점마다 덧붙여 규모를 불린다(연산자 배분표를 RFC-0002 §1에 확정).
- Consequences: 전체 5부 18장으로 재편. "사용이 정의보다 먼저" 원리 추가. 입력 시연은 변수가 생기는 5장에서 완결(4장은 예고만). RFC-0002 §6에 의존 순서 자연스러움 자체 검토를 수록.

### 2026-08-04: 장 체계 rev.c — 소형 장, 섹션 단위 빠른 전환 (10부 43장)

- Status: Accepted
- Context: rev.b의 장들이 크다. 사용자 지시: 장의 규모를 줄이고 섹션 단위로 빠르게 작은 규모로 넘어가게 한다.
- Decision: 한 장 = 하나의 착상, 대략 6~12쪽의 소형 장으로 재편한다. 큰 주제는 부(部)가 담는다 — 대형 장이던 "전산의 기본과 배경지식"은 제2부(2~10장 9개 장)로 승격("상당한 분량" 확정은 부 전체 분량으로 계승). 전체 10부 43장. 내용은 rev.b에서 전부 보존, 단위만 분해. RFC-0001 장 템플릿은 경량 적용(선행조직자 1~2문장, 심화 문답 1~3개, 오개념·실제 사례는 부 안에서 고르게)하고, 파편화 방지로 부 서두 선행조직자와 장 간 다리 문단을 의무화한다.
- Consequences: 독자의 완결감 빈도가 올라간다(동기 유지). 집필 중 6~12쪽 목표를 기준으로 분할·병합 미세 조정 허용. 책 골격(book/chapters 43 스텁, main.typ 부 표제지+동적 include) 갱신.

### 2026-08-04: 제2부 확장 2건 — 기억의 분화 장(rev.e)·정수의 표현 장(rev.f)

- Status: Accepted
- Context: 사용자 지시 2건 — ① 속도 이야기 전에 기억의 분화(레지스터 강조→외부 메모리→다층 캐시, 캐시 라인·false sharing·멀티코어)를 별도 장으로. ② 소수점 수(구 5장) 이전에 정수형 배경(부호형·무부호형·오버플로·세 가지 부호 표현과 C23의 2의 보수 확정·시프트)을 별도 장으로.
- Decision: 9장 "기억의 분화"(고백→레지스터→격차→다층 캐시)와 5장 "정수의 표현"(모듈러·부호-크기/1의 보수/2의 보수 경쟁·C23 확정·signed 오버플로는 여전히 UB라는 구별·시프트의 채움과 CPU별 상이한 대응→UB)을 신설한다. 전체 10부 45장(rev.f). 파이프라인·분기 예측·멀티코어·false sharing·pre-ANSI 역사는 10장이 담당한다.
- Consequences: 표현의 사다리가 정수(5)→소수(6)→문자(7)→흐름(8) 4단으로 완성. Duff's device는 10장 예고→27장 시연(나선). 장 번호 대이동 2회 — 상호 참조는 자리표시자 2단계 치환으로 갱신(한 자리 수 치환의 부분 문자열 오염 주의를 LESSONS에 기록).

### 2026-08-04: 15장 예시 플랫폼 = Windows(clang 우선), 플랫폼 의존 내용은 격리 절

- Status: Accepted
- Context: 개발환경 장의 예시 플랫폼과 플랫폼 의존 내용의 배치를 정해야 한다.
- Decision: 예시 플랫폼은 Windows로 한다. 도구는 LLVM clang(권장)과 MinGW-w64 gcc 두 갈래를 제시하고, Visual Studio(MSVC)는 상세 설명 대신 Microsoft 공식 문서 링크로 대신한다. ASan·UBSan·TSan 소개와 설치·사용법을 다루되 사실대로 적는다(Windows에서 ASan/UBSan=clang으로 가능, MinGW gcc=미지원, TSan=Windows 미지원→WSL). 플랫폼 의존 내용은 본문 일반론과 격리된 전용 절(lib.typ `platform` 상자)에만 둔다 — 상자를 건너뛰어도 본문이 성립해야 한다.
- Consequences: 본문 명령 표기는 `cc`(gcc/clang 공용)로 통일. 예제 교차 검증(gcc+clang) 서사와 정합. 이후 플랫폼 의존 내용이 생길 때마다 같은 상자를 쓴다.

### 2026-08-04: 22장 입력 방식 = 줄 읽기 + sscanf 해석

- Status: Accepted
- Context: 첫 입력 장의 방식 선택 — scanf 직접 사용 vs 줄 읽기 후 해석.
- Decision: 한 줄을 통째로 읽고(fgets, sizeof로 그릇 크기 전달) 그 줄을 sscanf로 해석하는 두 단계 방식을 처음부터 가르친다. 근거: 사람 입력의 자연 단위=줄(행 버퍼링 서사와 정합), 실패 뒤처리가 깨끗함(입력 스트림 오염 없음), 안전한 결(그릇 크기 명시)이 33장 안전성 서사로 직결. scanf 직접 사용은 본문 문답에서 존재만 언급.
- Consequences: 의도된 외상 2건 발생·기일 명시 — char line[100](배열, 32장), &(주소 표시, 29장) + sizeof(29장). 검증 파이프라인에 표준 입력 지원 추가(examples/*.in 파일, demo 장치 stdin: true 표시 상자). 반환값 검사는 분기(25장) 이후 40장에서 규율화.

### 2026-08-04: proven 라이브러리 = vendor 복사

- Status: Accepted
- Context: 34장부터 예제가 proven을 사용한다. 외부 참조(별도 체크아웃) vs vendor 복사 중 선택 필요.
- Decision: vendor 복사로 한다. `vendor/proven/`에 include/·src/·platform/·LICENSE를 스냅샷으로 담고(VENDOR.md에 원본 판 기록: v26.07.23b-3-gc0e4d09), 저장소만으로 전 예제가 빌드되게 한다.
- Consequences: T001 검증 스크립트가 `#include <proven`을 쓰는 예제를 감지해 vendor를 지연 빌드(src=-std=c23, platform=+_DEFAULT_SOURCE/_POSIX_C_SOURCE=200809L)하고 -lm과 함께 링크한다. 상시 검증용 스모크 예제 examples/smoke/proven_link.c 추가. 상류 갱신 시 vendor 재복사+VENDOR.md 갱신.

### 2026-08-05: 라이선스 — 본문 CC BY-NC-SA 4.0, 예제 코드 MIT

- Status: Accepted
- Context: 부트스트랩 당시 미결로 남겨 둔 라이선스를 정해야 한다. 본문과 예제 코드의 성격이 달라 한 라이선스로 묶기 어렵다.
- Decision: 책 본문(book/ 및 생성 PDF)은 CC BY-NC-SA 4.0(저작자표시-비영리-동일조건변경허락), 수록 예제 코드(examples/, scripts/)는 MIT로 한다. vendor/proven은 원저작물 라이선스를 따른다.
- Consequences: 저장소에 `LICENSE`(두 라이선스 안내)·`LICENSE-BOOK`·`LICENSE-CODE` 배치. 책에 판권 페이지 추가(표지 다음, 목차 앞) — 저자 연락처·라이선스·"모든 코드 시연은 실제 실행 결과"·조판 도구 명시. README와 REQUIREMENTS(R18)에도 반영. 근거: 본문은 무단 상업 출판을 막되 학습·공유는 열어 두고, 예제 코드는 독자가 자기 프로그램에 그대로 쓸 수 있게 하려는 것.
