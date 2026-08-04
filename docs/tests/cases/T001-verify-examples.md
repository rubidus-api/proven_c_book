# T001: 수록 예제 전수 검증

- Requirement: R15
- Command: `scripts/verify-examples.sh`
- Status: active

## Purpose

`examples/` 아래 모든 `.c` 예제가 C23으로 경고 없이(-Wall -Wextra -Werror)
빌드되고, 실행이 0으로 종료함을 보장한다. 각 예제의 표준 출력을
`build/examples-out/<장>/<이름>.c.out` 으로 캡처한다 — 책의 `demo` 장치가
이 캡처를 읽어 인쇄하므로, 지면의 실행 결과는 항상 실제 결과다.

## Pass Criteria

- 모든 예제 build ok + run ok (`verify-examples: all examples green`).
- 실패가 하나라도 있으면 비영 종료.

## Notes

- 컴파일러는 `CC` 환경변수로 교체 가능(기본 gcc). clang 교차 검증은 후속.
- 대화형 입력이 필요한 예제가 생기면 입력 파일 규약을 추가한다 (미결).
