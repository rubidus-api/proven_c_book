# Changelog

All notable changes to this project will be documented in this file.

This project follows Keep a Changelog.

## [v0.1.0] - 2026-08-05

첫 공개 판. 12부 58장 + 부록 A~E + 찾아보기, 약 570쪽.

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
