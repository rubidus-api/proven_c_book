# Changelog

All notable changes to this project will be documented in this file.

This project follows Keep a Changelog.

## [Unreleased]

### Added

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
