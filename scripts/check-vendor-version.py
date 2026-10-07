#!/usr/bin/env python3
r"""책이 말하는 proven 판이 함께 실린 스냅샷과 같은가 (2026-10-08).

`vendor/proven/` 을 v0.6.0 으로 바꿨을 때 원고 세 곳이 옛 판 이름(`v26.07.23b`·
`v26.07.23d`)을 그대로 적고 있었다. 스냅샷은 한 줄(`VENDOR.md`)로 바뀌지만 그것을
인용한 문장은 아무도 고쳐 주지 않는다.

  · `VENDOR.md` 의 판 = 헤더(`version.h`)의 판.
  · 원고가 적은 proven 판 이름은 전부 그 판이다(두 판 모두, 한 번 이상).
  · 날짜식 옛 이름(`v26.MM.DDx`)은 원고에 남지 않는다.

사용법: python3 scripts/check-vendor-version.py
종료 상태: 어긋나면 1
"""
import pathlib
import re

from chapters import is_draft

ROOT = pathlib.Path(__file__).resolve().parent.parent
VENDOR = ROOT / "vendor" / "proven"


def main():
    bad = []
    m = re.search(r"snapshot:\s*v(\d+\.\d+\.\d+)", (VENDOR / "VENDOR.md").read_text(encoding="utf-8"))
    if not m:
        print("check-vendor-version: VENDOR.md 에서 판을 읽지 못했다")
        return 1
    want = m.group(1)
    head = (VENDOR / "include" / "proven" / "version.h").read_text(encoding="utf-8")
    parts = [re.search(rf"#define\s+PROVEN_VERSION_{k}\s+(\d+)", head) for k in ("MAJOR", "MINOR", "PATCH")]
    if not all(parts):
        bad.append("version.h 에서 판을 읽지 못했다")
    elif ".".join(p.group(1) for p in parts) != want:
        bad.append(f"VENDOR.md 는 v{want}, version.h 는 v" + ".".join(p.group(1) for p in parts))

    # 원고가 proven 의 판을 말하는 자리 --- 「스냅샷」 곁의 판 이름.
    cite = re.compile(r"v(\d+\.\d+\.\d+)\s*(?:의\s*)?(?:스냅샷|snapshot)|snapshot of v(\d+\.\d+\.\d+)")
    old = re.compile(r"v26\.\d\d\.\d\d[a-z]?")
    for ed in ("book", "book-en"):
        found = 0
        for path in sorted((ROOT / ed).rglob("*.typ")):
            if is_draft(path):
                continue
            text = path.read_text(encoding="utf-8")
            rel = path.relative_to(ROOT)
            for o in old.finditer(text):
                bad.append(f"{rel}: 날짜식 옛 판 이름 {o.group(0)}")
            for c in cite.finditer(text):
                found += 1
                got = c.group(1) or c.group(2)
                if got != want:
                    bad.append(f"{rel}: 스냅샷을 v{got} 라 적었다 --- 실린 것은 v{want}")
        if found == 0:
            bad.append(f"{ed}: 실린 proven 의 판을 적은 자리가 없다")
    for b in bad:
        print(f"  ⚠️  {b}")
    if bad:
        print(f"check-vendor-version: 책이 말하는 proven 판이 스냅샷과 다르다 {len(bad)}건")
        return 1
    print(f"check-vendor-version: 원고가 적은 proven 판이 실린 스냅샷(v{want})과 같다")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
