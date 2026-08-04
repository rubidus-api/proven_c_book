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
- Decision: 2장은 "전산의 기본과 배경지식"으로서 책에서 상당한 분량을 차지한다. 내용 궤적: CPU·메모리·클록의 단순한 모델 → 워드·메모리 주소 → 다층 캐시·분기 예측·파이프라인 → 컴파일러 최적화 → C가 추상적인 언어인 이유와 추상적으로 접근해야 하는 이유 → 최근의 엄격한 포인터 제한(프로버넌스) 흐름. 여기에 C와 표준의 역사를 별도 절이 아니라 절 단위로 엮는다(이중 트랙). 문자와 텍스트(EBCDIC·ASCII·ISO 646과 삼중자·ISO 8859 Latin·유니코드·가변 길이 인코딩·UTF-8), 수의 표현(고정소수점·부동소수점·IEEE 754), CPU와 GPU의 차이(지연시간 vs 처리량), 스트림의 기원(펀치카드·라인프린터·프린터 터미널 → C 스트림 모델)도 이 장의 배경지식에 포함한다. 역사 서술은 기능적 역사 원칙을 따른다: 고리타분한 연대기·인물 나열은 빼고, 지금의 C 모양을 설명하는 데 필요한 역사만 쓴다.
- Consequences: 2장이 이후 장들이 심화 문답으로 회수하는 배경지식의 저수지가 된다. 비대해지면 2개 장으로 분할 가능하되 이중 트랙 서사는 유지한다.
