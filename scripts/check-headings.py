#!/usr/bin/env python3
"""각 장·부록 파일의 1단계 제목(`= `)이 정확히 하나인지 검사한다.

인라인 코드가 줄바꿈되며 `= 1` 처럼 시작하면 Typst 가 그것을 새 장 제목으로
읽어 목차와 장 번호를 오염시킨다. 실제로 그 사고가 있었으므로 빌드마다 본다.
"""
import pathlib, sys

root = pathlib.Path(__file__).resolve().parent.parent
bad = []
for base in ("book", "book-en"):
    for sub in ("chapters", "appendix", "front", "back", "parts"):
        d = root / base / sub
        if not d.is_dir():
            continue
        for f in sorted(d.glob("*.typ")):
            heads = [(i + 1, l) for i, l in enumerate(f.read_text().splitlines())
                     if l.startswith("= ")]
            expected = 0 if sub == "parts" else 1
            if len(heads) > expected:
                bad.append((f.relative_to(root), heads[expected:]))

for f, heads in bad:
    print(f"⚠️  {f}")
    for line, text in heads:
        print(f"      {line}행: {text[:60]}")
if bad:
    print(f"check-headings: {len(bad)} 개 파일에 예상 밖의 1단계 제목이 있다")
    sys.exit(1)
print("check-headings: 1단계 제목이 파일마다 하나씩이다")
