# T002: 책 전체 빌드

- Requirement: R14
- Command: `scripts/build-book.sh` (환경: `TYPST`=typst 경로, `FONT_PATH`=Noto CJK KR 디렉터리)
- Status: active

## Purpose

T001(예제 검증·출력 캡처)을 선행한 뒤 `typst compile --root .` 로 원고
전체를 `build/book.pdf` 로 산출한다. 컴파일 오류·읽기 실패(캡처 누락 포함)가
있으면 실패한다.

## Pass Criteria

- `build-book: build/book.pdf` 출력과 함께 0 종료.
- 한글 본문이 Noto Serif/Sans CJK KR 로 렌더링된다 (표본 페이지 육안 확인).

## Notes

- typst 바이너리와 글꼴은 저장소 밖 도구 디렉터리에 있다 (git 미추적).
