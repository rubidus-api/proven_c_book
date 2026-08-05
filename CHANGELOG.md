# Changelog

All notable changes to this project will be documented in this file.

This project follows Keep a Changelog.

## [Unreleased]

### Added
- **제11부에 세 장 신설(58~60장)** — `<stdatomic.h>`(원자적 연산·데이터 경쟁·
  `volatile`과의 차이·메모리 순서·CAS 관용구), `<stdckdint.h>`(검사 산술,
  사후 검사가 지워지는 이유, 할당 크기 계산), 그리고 매크로에서 키워드로
  승격된 낱말들(`bool`·`nullptr`·`static_assert`·`constexpr`·`typeof` — 도입
  배경, 무엇을 막는가, 규칙과 제한, 기존 코드 이전 순서). 예제 3종 검증.
  이에 따라 proven 부는 61~70장, 닫는 부는 71~72장으로 이동했다.
- **영어판 번역 착수** — 머리말, 제2부 도입부, 1~7장. 장 번호는 한국어판과
  동일하게 고정한다(미번역 장은 부 표지에 명시).
- **한국어판·영어판 동기화 장치** — `scripts/sync-status.py` 가 각 영어 파일이
  어느 한국어 원본 해시에서 번역됐는지 기록하고 synced/stale/pending 을
  판정한다. 두 빌드가 이를 실행하므로 원본이 바뀌면 다음 빌드에서 해당 영어
  장이 stale 로 드러난다. `TRANSLATION.md` 는 이 기록에서 자동 생성한다.
- 서술 장치 라벨을 `book/lib.typ` 한 곳에서 지역화(`--input lang=`) — 영어판이
  라이브러리 사본을 갖지 않으므로 장치를 고치면 양판이 함께 바뀐다.

### Fixed
- HTML 목차·`<title>` 에서 장 제목이 이중 이스케이프되던 문제(`<stdio.h>` 가
  `&lt;stdio.h>` 로 보임). 내보낸 HTML 에서 뽑은 제목을 한 번 되돌린 뒤
  이스케이프한다.
- HTML 장치 판정이 접두 일치라 `문` 이 "문자열…" 을, `A` 가 "A bundle…" 을
  물 수 있던 문제 — 라벨 뒤에 공백을 요구하도록 조였다.

## [v0.1.1] - 2026-08-05

### Added
- **제11부 표준 라이브러리 정독 신설(48~57장)** — 헤더 전체 지도(31종),
  `<stdio.h>` 두 장(스트림·읽고 쓰기), `<string.h>`, `<stdlib.h>`,
  문자와 로케일, 수, 시간, 진단과 제어, 그리고 새 헤더와 부속서 K(`*_s`)의
  실패사. 함정 중심으로 쓰고 정례·반례를 붙였다.
- **46장 함수 포인터** — 이름의 무너짐과 `(*****f)()`의 비대칭, 타입 호환,
  `void *`와의 비호환(하버드 구조·`dlsym`), 디스패치 표, 손수 짓는 vtable과
  GObject·커널 `file_operations`·COM 사례.
- 예제 15종 추가(전부 실제 빌드·실행 검증). 수학 예제를 위해 검증
  스크립트가 `-lm`을 자동으로 링크한다.

### Changed
- proven 부가 제12부(58~67장)로, 실전·총정리가 68·69장으로 이동.
- 13부 69장 구성.

### Fixed
- 13장에서 낱말 안쪽 `*` 강조가 HTML 페이지를 잘라먹던 문제(`stdio` 설명)
  수정, "검은 상자" 표현 정정.
- 찾아보기 조판을 2열 표제어/쪽번호로 바로잡고, HTML 색인에 본문 링크 추가.
- **PDF 태그 비활성화(`--no-pdf-tags`)** — Typst 0.15.1이 링크마다 빈 쪽을
  만들어 책이 235쪽에서 570쪽으로 부풀던 문제(typst/typst#8722). 이로써
  쪽수가 정상화되고 색인의 쪽 번호 링크도 살아났다.
- 인쇄용 흑백 전환 — 상자 채움·검은 출력 상자·구문 강조 색 제거.

## [v0.1.0] - 2026-08-05

첫 공개 판. 12부 58장 + 부록 A~E + 찾아보기, 약 235쪽.

### Added
- 제11부 proven 매뉴얼부(47~56장) — 다섯 가지 버그 제기부터 프리스탠딩까지.
- 부록 A~C 확장(연산자·서식 문자열 완전 정리·암묵 변환), 부록 D(더 읽을거리와
  표준 문서), 부록 E(참고 문헌), 그리고 쪽 번호가 붙는 찾아보기.
- 15장 디버거(빌드 옵션·맹점·릴리스 전용 버그), 46장 printf/scanf 해부,
  45장 가변 인자 승격과 `_Generic`·`PROVEN_ARG` 구조, 23장 기본 자료형 지도,
  33장 VLA·`[static N]`·배열 매개변수, 39장 비트 필드, 44장 X 매크로,
  57장 C와 C++의 관계.
- 수록 예제 60여 종, 전부 실제 빌드·실행 검증(GCC 14 기준, Clang 교차 확인).

## [Unreleased]

### Added
- **제11부에 세 장 신설(58~60장)** — `<stdatomic.h>`(원자적 연산·데이터 경쟁·
  `volatile`과의 차이·메모리 순서·CAS 관용구), `<stdckdint.h>`(검사 산술,
  사후 검사가 지워지는 이유, 할당 크기 계산), 그리고 매크로에서 키워드로
  승격된 낱말들(`bool`·`nullptr`·`static_assert`·`constexpr`·`typeof` — 도입
  배경, 무엇을 막는가, 규칙과 제한, 기존 코드 이전 순서). 예제 3종 검증.
  이에 따라 proven 부는 61~70장, 닫는 부는 71~72장으로 이동했다.

- **전 장 초고 완료 (2026-08-05)**: 10부 49장 + 부록 A~C, 약 294쪽.
  제1부 바탕 / 제2부 전산의 기본과 배경지식(2~12) / 제3부 첫 프로그램 /
  제4부 최소한의 도구 상자 / 제5부 선언 / 제6부 값과 흐름 / 제7부 기억 /
  제8부 자료의 모양 / 제9부 정밀 / 제10부 구성.
- 수록 예제 30여 종, 전부 실제 빌드·실행 검증(T001) — 표준 입력(.in),
  여러 파일(main.c 규약), vendor proven 자동 링크 지원.
- vendor/proven 스냅샷(v26.07.23b-3-gc0e4d09).
- RFC-0001 교수 설계(읽기 전용 문답형)·RFC-0002 책 구성(5부 17장) Accepted.
- Typst 빌드 골격: `book/main.typ` + 서술 장치 라이브러리 `book/lib.typ`(qa/deepqa/misconception/realcase/mathbox/recap/organizer/demo), 17장 스텁.
- 예제 전수 검증 `scripts/verify-examples.sh`(T001): C23 빌드·실행 + 출력 캡처(책의 demo가 자동 include).
- 책 빌드 `scripts/build-book.sh`(T002): 검증 → `typst compile` → `build/book.pdf`.
- 첫 예제 `examples/ch03/hello.c`, 파이프라인 관통 확인(한글 조판 포함).

### Changed

### Deprecated

### Removed

### Fixed

### Security
