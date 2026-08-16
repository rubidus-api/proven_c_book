#!/usr/bin/env python3
"""장 참조가 *이름*으로 되어 있는지, 두 판이 같은 장을 가리키는지 본다 (RFC-0028 §3.4).

옛 `check-xrefs.py` 는 두 판의 *장 번호* 집합을 맞대어 봤다. 번호가 원고에서
사라진 지금은 물을 것이 둘로 바뀐다.

  ① 원고에 *맨 숫자 장 참조*가 남아 있지 않은가
     (`32장`, `chapter 32` --- 이제는 `#chref("loops")` 로 적는다)
  ② 같은 장의 두 판이 *같은 장들*을 가리키는가
     번호가 아니라 id 를 맞대므로, 한쪽만 고친 참조가 바로 드러난다.

②는 옛 검사보다 세다. 예전에는 두 판이 *나란히 틀리면* 통과했지만(실제로
영어판 94장이 「82 (memory layout)」이라고 한 장 어긋나 있었다), id 는 뜻이
있어서 그런 어긋남이 눈에 띈다.

사용법: python3 scripts/check-chapter-refs.py
종료 상태: 어긋난 곳이 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO, EN = ROOT / "book", ROOT / "book-en"
SKIP = {"lib.typ", "registry.typ", "style.typ", "main.typ"}

FENCE = re.compile(r"```[\s\S]*?```")
INLINE = re.compile(r"`[^`\n]*`")
STRING = re.compile(r'"[^"\n]*"')
BARE_KO = re.compile(r"(?<![\d.])\d{1,3}\s*장(?![가-힣])")
BARE_EN = re.compile(r"\b[Cc]hapters?\s+\d{1,3}\b")
REF = re.compile(r'#(?:chref|chrefs|chrange)\(([^()]*)\)')


def prose(path):
    t = path.read_text(encoding="utf-8")
    return STRING.sub(" ", INLINE.sub(" ", FENCE.sub(" ", t)))


def ids_in(path):
    return set(re.findall(r'"([^"]+)"', " ".join(REF.findall(path.read_text(encoding="utf-8")))))


def main():
    bad = 0

    # ① 맨 숫자 참조가 남았는가
    for base, pat in ((KO, BARE_KO), (EN, BARE_EN)):
        for path in sorted(base.rglob("*.typ")):
            if path.name in SKIP:
                continue
            for m in pat.finditer(prose(path)):
                bad += 1
                print(f"  ⚠️  맨 숫자 참조  {path.relative_to(ROOT)}: {m.group(0).strip()}")

    # ② 두 판이 같은 장을 가리키는가
    pairs = 0
    for ko in sorted((KO / "chapters").glob("ch*.typ")):
        en = EN / "chapters" / ko.name
        if not en.exists():
            continue
        pairs += 1
        a, b = ids_in(ko), ids_in(en)
        if a != b:
            bad += 1
            only_ko = ", ".join(sorted(a - b)) or "—"
            only_en = ", ".join(sorted(b - a)) or "—"
            print(f"  ⚠️  {ko.name}: 한국어판에만 [{only_ko}] · 영어판에만 [{only_en}]")

    if bad:
        print(f"check-chapter-refs: 어긋난 곳 {bad}건", file=sys.stderr)
        return 1
    print(f"check-chapter-refs: 장 참조가 모두 이름이고, 두 판 {pairs}장이 같은 장을 가리킨다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
