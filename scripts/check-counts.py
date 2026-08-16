#!/usr/bin/env python3
"""책의 규모를 적은 숫자가 실제와 맞는지 대조한다 (B001 감사 2, 2026-08-10).

★ 왜 필요한가
  README 가 「13부 97장 + 부록 A~E」라고 적고 있었는데, 실제로는 **98장 + 부록
  A~F** 였다. 장을 하나 더하고(79장 `<threads.h>`), 부록을 하나 더하면서(F 표준
  라이브러리 요람) 문장을 고치지 않은 것이다. 사람이 세는 한 또 어긋난다.

무엇을 정본으로 삼는가
  `book/main.typ` 의 `parts` 목록과 `book/appendix/*.typ` 의 제목. 거기에 없는
  장은 책에 실리지 않으므로, 그것이 유일한 사실이다.

무엇을 검사하는가
  README.md · README-en.md 에 적힌 부·장·부록 수. 어긋나면 실패한다.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def truth():
    src = (ROOT / "book" / "main.typ").read_text(encoding="utf-8")
    # 부·장의 단일 출처는 등록부다(RFC-0028). main.typ 은 그것을 옮겨 담을 뿐이다.
    reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
    parts = re.findall(r'ko: "(제\d+부[^"]*)",[\s\S]*?chapters: \(([^)]*)\)', reg)
    chapters = sum(
        len(re.findall(r'"[^"]+"', chs)) for _, chs in parts
    )
    letters = []
    for f in sorted((ROOT / "book" / "appendix").glob("*.typ")):
        m = re.search(r"^= 부록 ([A-Z])", f.read_text(encoding="utf-8"), re.M)
        if m:
            letters.append(m.group(1))
    return len(parts), chapters, sorted(letters)


def main():
    n_parts, n_chapters, letters = truth()
    span = f"{letters[0]}~{letters[-1]}"
    span_en = f"{letters[0]}–{letters[-1]}"
    bad = []

    ko = (ROOT / "README.md").read_text(encoding="utf-8")
    m = re.search(r"(\d+)부 (\d+)장 \+ 부록 ([A-Z]~[A-Z])", ko)
    if not m:
        bad.append("README.md: 「N부 N장 + 부록 X~Y」 줄을 찾지 못했다")
    elif (int(m.group(1)), int(m.group(2)), m.group(3)) != (n_parts, n_chapters, span):
        bad.append(
            f"README.md: 「{m.group(0)}」 → 실제는 "
            f"「{n_parts}부 {n_chapters}장 + 부록 {span}」"
        )

    en = (ROOT / "README-en.md").read_text(encoding="utf-8")
    m = re.search(r"(\d+) parts, (\d+) chapters, appendices ([A-Z][–-][A-Z])", en)
    if not m:
        bad.append("README-en.md: 「N parts, N chapters, appendices X–Y」 줄이 없다")
    elif (int(m.group(1)), int(m.group(2))) != (n_parts, n_chapters) or m.group(
        3
    ).replace("-", "–") != span_en:
        bad.append(
            f"README-en.md: 「{m.group(0)}」 → 실제는 "
            f"「{n_parts} parts, {n_chapters} chapters, appendices {span_en}」"
        )

    for line in bad:
        print("·  " + line, file=sys.stderr)
    if bad:
        print("check-counts: 적어 둔 규모가 실제와 다르다", file=sys.stderr)
        return 1
    print(
        f"check-counts: {n_parts}부 {n_chapters}장 · 부록 {span} — "
        "적어 둔 숫자가 실제와 같다"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
