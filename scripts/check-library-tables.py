#!/usr/bin/env python3
"""부록 B「표준 라이브러리 요람」의 *완전성*을 지킨다 (RFC-0025 L0).

저자의 요구는 「모든 함수에 대해서」였다. 571행짜리 목록에서 빠진 하나를 눈으로
찾을 수는 없다. 그래서 인벤토리(`docs/library-inventory.json`)와 부록을 대조한다.

  ★ 이 검사는 *아직 안 쓴 헤더*를 봐준다 --- `docs/library-todo.tsv` 에 적어 둔
    헤더만. 다 쓰고 나면 그 파일이 비고, 그때 이 게이트가 완전성을 뜻하게 된다.
    미완을 *명시적으로* 적게 하는 것이 요점이다: 「절반만 하고 멈춤」이 이 작업의
    가장 큰 위험이라 RFC 에 적어 두었다.

사용법: python3 scripts/check-library-tables.py [--list]
종료 상태: 다뤄야 할 헤더에서 빠진 이름이 있으면 1
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INV = ROOT / "docs" / "library-inventory.json"
TODO = ROOT / "docs" / "library-todo.tsv"
APPENDIX = {"ko": ROOT / "book" / "appendix" / "a7-library.typ",
            "en": ROOT / "book-en" / "appendix" / "a7-library.typ"}


def pending():
    """아직 착수하지 않은 헤더 --- 검사에서 잠시 뺀다."""
    out = set()
    if TODO.exists():
        for line in TODO.read_text(encoding="utf-8").splitlines():
            if line.strip() and not line.lstrip().startswith("#"):
                out.add(line.split("\t")[0].strip())
    return out


def main() -> int:
    if not INV.exists():
        print("check-library-tables: 인벤토리가 없다 --- "
              "scripts/lib-inventory.py 를 먼저 돌린다")
        return 0
    inv = json.loads(INV.read_text(encoding="utf-8"))
    skip = pending()

    total_missing = 0
    for lang, path in APPENDIX.items():
        if not path.exists():
            print(f"  ⚠️  [{lang}] 부록 파일이 없다: "
                  f"{path.relative_to(ROOT)}")
            total_missing += 1
            continue
        text = path.read_text(encoding="utf-8")
        # 표에 실린 이름 = 백틱으로 감싼 식별자
        listed = set(re.findall(r"`([A-Za-z_][A-Za-z0-9_]*)`", text))
        missing = []
        for hdr, v in sorted(inv.items()):
            if hdr in skip:
                continue
            for f in v["functions"]:
                names = set(f["variants"]) | {f["name"]}
                if not (names & listed):
                    missing.append((hdr, f["name"]))
        done = sorted(h for h in inv if h not in skip)
        print(f"  [{lang}] 다룬 헤더 {len(done)}/{len(inv)} · 빠진 함수 {len(missing)}건")
        show = missing if "--list" in sys.argv else missing[:15]
        for hdr, n in show:
            print(f"      ⚠️  {hdr}: {n}")
        if len(missing) > len(show):
            print(f"      … 그리고 {len(missing) - len(show)}건 더")
        total_missing += len(missing)

    if skip:
        print(f"     아직 착수하지 않은 헤더 {len(skip)}개 "
              f"(docs/library-todo.tsv) --- 다 쓰면 그 파일을 비운다")
    if total_missing:
        print(f"check-library-tables: 빠진 항목 {total_missing}건")
        return 1
    print(f"check-library-tables: 착수한 헤더의 함수가 모두 요람에 있다"
          f"{' · 남은 헤더 ' + str(len(skip)) + '개' if skip else ' · 전 헤더 완료'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
