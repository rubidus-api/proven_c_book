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


def header_rows():
    """★ 헤더 표의 「어디서 다루는가」 열이 *그 헤더의 이름을 단 장*을 가리키는지 본다.

    2026-08-29 에 열한 줄이 틀린 것을 찾았다 --- `<setjmp.h>` 가 82장(제목이
    「비지역 점프 --- `<setjmp.h>`」)이 아니라 80장을, `<threads.h>` 가 84장이 아니라
    83장을 가리키고 있었다. 표가 먼저 쓰이고 장이 나중에 서면서 갈라진 것이다.
    장 *제목*에 헤더 이름이 박혀 있으면, 그 판정은 사람이 아니라 기계가 할 수 있다.
    """
    reg = (ROOT / "book" / "registry.typ").read_text(encoding="utf-8")
    ids = []
    for m in re.finditer(r"chapters:\s*\(([^)]*)\)", reg):
        ids += re.findall(r'"([^"]+)"', m.group(1))

    owner = {}          # <헤더> → 그 헤더를 제목에 단 장들의 id
                        #   ★ 둘 이상일 수 있다 --- 80장은 `<signal.h>` 를 *소개*하고
                        #     81장이 그것으로 한 장을 쓴다. 그 중 하나를 가리키면 된다.
    for i, cid in enumerate(ids, 1):
        text = (ROOT / "book" / "chapters" / f"ch{i:02d}.typ").read_text(encoding="utf-8")
        title = next((l for l in text.split("\n") if l.startswith("= ")), "")
        for h in re.findall(r"<([a-z0-9_]+\.h)>", title):
            owner.setdefault(h, []).append(cid)

    bad = 0
    for ed in ("book", "book-en"):
        for path in sorted((ROOT / ed / "chapters").glob("ch*.typ")):
            # ★ 「어디서 다루는가」 열을 가진 표만 본다. 다른 표의 참조는 뜻이
            #   다르다 --- 83장의 `<stdatomic.h>` 행은 「오늘의 위치」를 적으면서
            #   캐시 이야기(13장)를 가리키는데, 그것은 틀린 것이 아니다.
            here = None
            for line in path.read_text(encoding="utf-8").split("\n"):
                idm = re.search(r'id:\s*"([^"]+)"', line)
                if idm:
                    here = idm.group(1)
                m = re.match(r"\s*\[`<([a-z0-9_]+\.h)>`\],", line)
                if not m or m.group(1) not in owner or here != "headers-by-edition":
                    continue
                want = owner[m.group(1)]
                refs = re.findall(r'#chrefs?\(([^)]*)\)', line)
                if not refs:
                    continue
                if not any(f'"{w}"' in " ".join(refs) for w in want):
                    bad += 1
                    print(f"  ⚠️  <{m.group(1)}> 를 제목에 단 장은 {'·'.join(want)} 인데 "
                          f"표는 딴 곳을 가리킨다  "
                          f"{path.relative_to(ROOT)}: {' '.join(refs)}")
    return bad


# 한글 수사 --- 서두 문단이 「스물두 헤더」처럼 적으므로 그대로 견준다.
_KO = {2: "둘", 4: "넷"}
_KO_N = {20: "스무", 21: "스물한", 22: "스물두", 23: "스물세", 24: "스물네",
         25: "스물다섯", 26: "스물여섯"}


def progress_claim(skip) -> int:
    """부록 F 서두가 밝힌 진행 상태가 실제와 같은가.

    ★ 왜 --- 서두는 「빠짐없이 싣는 것을 *목표*로 한다」고만 말해서, 독자가 첫
      화면에서 진행률을 알 수 없었다(검토 2026-08-29 §4). 그래서 지금 어디까지
      왔는지를 적었는데, 적어 두기만 하면 그 문장이 곧 낡는다. 여기서 대조한다.
    """
    import re
    app = ROOT / "book" / "appendix" / "a7-library.typ"
    if not app.exists():
        return 0
    txt = app.read_text(encoding="utf-8")
    caps = set(re.findall(r"caption:\s*\[`<([a-z]+\.h)>`", txt))
    done = sorted(caps - set(skip))
    partial = sorted(caps & set(skip))
    rest = len(skip) - len(partial)
    if "어디까지 왔는지 먼저 밝힌다" not in txt:
        if not skip:
            return 0        # 다 썼으면 그 문단은 사라져 있어야 한다
        print("  ⚠️  부록 F 서두에 진행 상태가 없다 --- 독자가 진행률을 알 수 없다")
        return 1
    if not skip:
        print("  ⚠️  전 헤더를 다 썼는데 부록 F 서두의 진행 문단이 남아 있다 --- 지울 것")
        return 1
    bad = 0
    for h in done:
        if f"`<{h}>`" not in txt.split("어디까지 왔는지")[1][:400]:
            print(f"  ⚠️  부록 F 서두가 완결된 <{h}> 을 말하지 않는다")
            bad += 1
    if _KO.get(len(done)) and _KO[len(done)] not in txt.split("어디까지 왔는지")[1][:300]:
        print(f"  ⚠️  부록 F 서두의 완결 헤더 수가 실제({len(done)})와 다르다")
        bad += 1
    if _KO_N.get(rest) and _KO_N[rest] not in txt:
        print(f"  ⚠️  부록 F 서두의 미착수 헤더 수가 실제({rest})와 다르다")
        bad += 1
    return bad


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
    total_missing += header_rows()
    total_missing += progress_claim(skip)

    if total_missing:
        print(f"check-library-tables: 빠진 항목 {total_missing}건")
        return 1
    print(f"check-library-tables: 착수한 헤더의 함수가 모두 요람에 있다"
          f"{' · 남은 헤더 ' + str(len(skip)) + '개' if skip else ' · 전 헤더 완료'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
