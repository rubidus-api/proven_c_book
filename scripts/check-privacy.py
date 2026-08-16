#!/usr/bin/env python3
"""공개 저장소에 이 기계의 사정이 새지 않았는가.

규칙과 구현은 작업공간의 공용 도구 하나에 모여 있다(`usr/bin/check-privacy`).
이 파일은 그 도구를 이 저장소의 사정에 맞게 부르는 얇은 껍데기다 --- 추적 파일에
더해 *캡처된 예제 출력*까지 보게 한다. 실행 결과는 아무도 다시 읽지 않은 채 책에
인쇄되기 때문이다(53장 `argv[0]` 이 빌드 디렉터리를 찍어 444쪽에 실렸다).

도구가 없는 곳(저장소만 따로 clone 한 경우)에서는 건너뛴다.

사용법:  python3 scripts/check-privacy.py
"""
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TOOL = ROOT.parent / "usr" / "bin" / "check-privacy"


def main() -> int:
    if not os.access(TOOL, os.X_OK):
        print("check-privacy: 공용 검사 도구가 없다 --- 건너뛴다 "
              "(작업공간의 usr/bin/check-privacy)")
        return 0
    args = [str(TOOL), "build/examples-out", "build/examples-out-en"]
    return os.spawnv(os.P_WAIT, str(TOOL), args)


if __name__ == "__main__":
    sys.exit(main())
