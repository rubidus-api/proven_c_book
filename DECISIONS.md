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
