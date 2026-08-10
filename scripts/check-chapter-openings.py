#!/usr/bin/env python3
"""장 서두의 정형을 기계적으로 검사한다 (RFC-0008 §2).

장 서두는 다음 다섯을 *이 순서로* 갖춘다.

    = N 장 제목
    #prereq(...)             ① 먼저 알아야 할 것        (1장 제외)
    #deepqa[...][...]        ② 인출 문답                 (1장 제외)
    #why[...]                ③ 이 장의 필요성과 맥락
    #organizer[...]          ④ 이 장이 끝나면
    #chapter-questions()     ⑤ 이 장에서 답할 질문

순서의 근거는 학습 동선이다 — 무엇에 기대는지 알려 주고(①), 그것을 실제로
꺼내 보게 하고(②), 이 장이 왜 필요하고 왜 하필 이 자리인지를 대고(③), 그 위에서
무엇을 얻을지 예고하고(④), 답할 질문을 미리 보여 준다(⑤). ②가 ①의 바로 뒤에
오는 것이 이 정형의 핵심이다: 선행 개념을 *표시*하는 데 그치지 않고 *인출*까지
시킨다. 그리고 ③④가 짝이다 — 자리를 대고 나서 얻을 것을 약속한다
(저자 지시 2026-08-10).

검사 항목
  1. 다섯 장치가 각각 정확히 한 번 (1장은 ③④⑤만).
  2. 다섯 장치가 정해진 순서로.
  3. 다섯 모두 장 제목(`= `)보다 뒤에, 첫 절 제목(`== `)보다 앞에.
  4. `#qa[` 가 하나 이상 (질문 목록이 비지 않도록).
  5. `prereq` 항목이 하나 이상이고, 번호와 장 이름을 함께 적었는가.
  6. 한국어판과 영어판 양쪽에 같은 규칙.

어긴 곳이 있으면 목록을 찍고 1을 돌려준다.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# (표시 이름, 원고에서 찾을 문자열, 1장에도 필요한가)
DEVICES = [
    ("prereq", "#prereq(", False),
    ("deepqa", "#deepqa[", False),
    ("why", "#why[", True),
    ("organizer", "#organizer[", True),
    ("chapter-questions", "#chapter-questions()", True),
]

# prereq 항목이 "5장 워드와 주소" / "chapter 5, Words and addresses" 꼴인지
REF_KO = re.compile(r"\[\s*\d+장\s+\S")
REF_EN = re.compile(r"\[\s*chapter\s+\d+\s*,\s*\S", re.I)


def check_file(path: pathlib.Path, rel: str, is_first: bool, lang: str) -> list[str]:
    text = path.read_text(encoding="utf-8")
    problems: list[str] = []

    first_section = text.find("\n== ")
    if first_section == -1:
        first_section = len(text)

    # ★ 서두 장치는 장 제목(`= `)보다 *뒤*에 있어야 한다. 앞에 두면 조판에서
    #   앞 장의 마지막 쪽에 얹혀 버린다(41장에서 실제로 그랬다, 2026-08-06).
    import re as _re
    title = _re.search(r"^= ", text, _re.M)
    if title:
        head = text[:title.start()]
        for name, needle, _ in DEVICES:
            if needle in head:
                problems.append(f"{name} 가 장 제목보다 앞에 있다 — "
                                f"제목 다음으로 옮길 것(앞 장 끝에 붙어 인쇄된다)")

    positions = {}
    for name, needle, needed_in_first in DEVICES:
        count = text.count(needle)
        required = needed_in_first or not is_first

        if count == 0:
            if required:
                problems.append(f"{rel}: `{name}` 이(가) 없다")
            continue
        if not required:
            problems.append(f"{rel}: 1장에는 `{name}` 을(를) 두지 않는다")
            continue
        if count > 1:
            problems.append(f"{rel}: `{name}` 이(가) {count}번 나온다 — 한 번이어야 한다")

        at = text.index(needle)
        positions[name] = at
        if at > first_section:
            problems.append(f"{rel}: `{name}` 이(가) 첫 절 제목보다 뒤에 있다")

    order = [n for n, _, _ in DEVICES if n in positions]
    if order != sorted(order, key=lambda n: positions[n]):
        actual = " → ".join(sorted(order, key=lambda n: positions[n]))
        problems.append(f"{rel}: 서두 순서가 어긋난다 (현재: {actual})")

    if len(re.findall(r"#qa\[", text)) == 0:
        problems.append(f"{rel}: 문답(#qa)이 하나도 없어 질문 목록이 비어 버린다")

    if "#prereq(" in text:
        start = text.index("#prereq(")
        body = text[start:text.index("\n)", start) + 2] if "\n)" in text[start:] else ""
        pattern = REF_KO if lang == "ko" else REF_EN
        if not pattern.search(body):
            problems.append(
                f"{rel}: prereq 참조에 장 번호와 이름이 함께 적혀 있지 않다")
    return problems


def check(edition: str, lang: str) -> list[str]:
    chapters = sorted((ROOT / edition / "chapters").glob("ch*.typ"))
    if not chapters:
        return [f"{edition}: 장 파일을 찾지 못했다"]
    problems = []
    for path in chapters:
        rel = f"{edition}/chapters/{path.name}"
        is_first = path.stem == "ch01"
        problems += check_file(path, rel, is_first, lang)
    return problems


def main() -> int:
    problems = check("book", "ko") + check("book-en", "en")
    if problems:
        print("check-chapter-openings: 장 서두의 정형을 어긴 곳이 있다", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print("check-chapter-openings: 모든 장이 서두 정형(기댄 것 → 인출 → 맥락 → 목표 → 질문)을 지킨다")
    return 0


if __name__ == "__main__":
    sys.exit(main())
