#!/usr/bin/env python3
"""인라인 코드가 원고에서 줄을 넘어가지 않게 한다 (저자 지적 2026-08-06).

`const struct point *p` 처럼 백틱 한 쌍 안에 원고의 줄바꿈이 들어가면,
Typst 는 그 자리를 공백으로 읽고 조판에서 그 공백에서 줄을 끊는다. 인쇄물에는
`const struct` 다음에 줄이 바뀌어 코드가 두 동강 난 것처럼 보인다. 42.3절에서
실제로 그렇게 나갔다.

원고를 손으로 접다 보면 다시 생기는 종류의 사고이므로 기계가 지킨다.

  1. 백틱 인라인 코드 안에 줄바꿈이 있으면 오류.
  2. 공백이 든 인라인 코드가 지나치게 길면 경고(한 줄에 안 들어가 조판이
     어색해진다) — 코드 블록으로 옮기라는 신호다.

사용법:  python3 scripts/check-inline-code.py [--warn-len N]
"""
import glob
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WARN_LEN = 56


def strip_blocks(text: str) -> str:
    """``` 코드 블록은 줄 수만 남기고 지운다(줄 번호 보존)."""
    return re.sub(r"```.*?```",
                  lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)


def main() -> int:
    warn_len = WARN_LEN
    if "--warn-len" in sys.argv:
        warn_len = int(sys.argv[sys.argv.index("--warn-len") + 1])

    errors, warnings = [], []
    for edition in ("book", "book-en"):
        for f in sorted(glob.glob(str(ROOT / edition / "**" / "*.typ"),
                                  recursive=True)):
            path = pathlib.Path(f)
            text = strip_blocks(path.read_text(encoding="utf-8"))
            rel = path.relative_to(ROOT)
            for m in re.finditer(r"`([^`]+)`", text):
                inner = m.group(1)
                line = text[:m.start()].count("\n") + 1
                if "\n" in inner:
                    flat = re.sub(r"\s+", " ", inner)[:48]
                    errors.append(f"{rel}:{line}: 인라인 코드가 줄을 넘는다 "
                                  f"— `{flat}` (한 줄로 붙일 것)")
                elif len(inner) > warn_len and " " in inner:
                    warnings.append(f"{rel}:{line}: 인라인 코드가 길다 "
                                    f"({len(inner)}자) — 코드 블록을 고려할 것")

    for w in warnings:
        print("·  " + w)
    for e in errors:
        print("⚠️  " + e)
    if errors:
        print(f"check-inline-code: {len(errors)} 건 — 인라인 코드가 줄을 넘는다")
        return 1
    print(f"check-inline-code: 인라인 코드가 모두 한 줄에 있다"
          f"{f' (긴 것 {len(warnings)}건은 참고)' if warnings else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
