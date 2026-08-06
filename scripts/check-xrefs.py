#!/usr/bin/env python3
"""번역된 장의 '장 번호 참조'가 한국어 원본과 일치하는지 대조한다.

재번호를 반복하다 보면 영어판의 `chapter N` 이 한국어판의 `N장` 과 어긋날 수
있다. 이 검사는 두 파일에서 참조된 장 번호의 *집합*을 비교해 차이를 알린다.
(문장 구조가 달라 개수는 다를 수 있으므로 집합으로 본다.)
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO, EN = ROOT / "book", ROOT / "book-en"

def ko_refs(text):
    """`12장`, `9–12장`, `20·21·22장`, `10장, 15장` 을 모두 읽는다."""
    out = set()
    for m in re.finditer(r"((?:\d+[·,]\s*)*\d+(?:[–-]\d+)?)장", text):
        for part in re.split(r"[·,]\s*", m.group(1)):
            # 연도 같은 큰 수는 장 번호가 아니다 (예: "1988, 37장")
            out.update(n for n in (int(x) for x in re.findall(r"\d+", part)) if n <= 200)
    return out

def en_refs(text):
    """`chapter 12`, `chapters 9-12`, `chapters 20, 21 and 22` 를 모두 읽는다."""
    out = set()
    for m in re.finditer(r"[Cc]hapters?\s+(\d+(?:\s*(?:,|and|[–-])\s*\d+)*)", text):
        out.update(n for n in (int(x) for x in re.findall(r"\d+", m.group(1))) if n <= 200)
    return out

bad = 0
for en in sorted(EN.rglob("*.typ")):
    rel = en.relative_to(EN)
    ko = KO / rel
    if not ko.exists() or rel.name == "main.typ":
        continue
    a, b = ko_refs(ko.read_text()), en_refs(en.read_text())
    if a != b:
        bad += 1
        print(f"⚠️  {rel}")
        if a - b: print(f"      한국어에만: {sorted(a - b)}")
        if b - a: print(f"      영어에만  : {sorted(b - a)}")
print(f"check-xrefs: {bad} 개 파일에서 장 참조가 어긋난다" if bad
      else "check-xrefs: 장 참조가 모두 일치한다")
sys.exit(1 if bad else 0)
