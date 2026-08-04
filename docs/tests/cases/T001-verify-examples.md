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

- 컴파일러는 `CC` 환경변수로 교체 가능(기본 gcc).
- **clang 교차 검증(2026-08-05 통과)**: 로컬에는 clang이 없으므로 원격 빌드
  컨테이너에서 수행한다 — 같은 저장소가 공유되므로 복사 없이
  `CC=clang ./scripts/verify-examples.sh`만 실행하면 된다. clang 22.1.8로
  전 예제(30여 종, vendor proven 포함) green 확인.
  주의: 교차 실행 뒤에는 gcc로 한 번 더 돌려 `build/examples-out`의 캡처를
  기준 컴파일러 결과로 되돌린다(책이 그 파일을 인쇄하므로).
- 입력이 필요한 예제는 같은 이름의 `.in` 파일을 두면 표준 입력으로 공급된다(책의 demo 장치가 `stdin: true`로 그 내용을 함께 인쇄).
- `#include <proven`을 쓰는 예제는 vendor/proven이 자동 빌드·링크된다(스모크: examples/smoke/proven_link.c).
