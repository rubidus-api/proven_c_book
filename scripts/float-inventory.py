#!/usr/bin/env python3
"""표와 그림의 목록을 뽑는다 --- 제목(캡션)을 붙이는 작업용 (RFC-0029).

각 표마다 *머리행*과 *바로 앞 문장*을 함께 보여 준다. 제목은 그 둘을 보고
사람이 쓴다 --- 기계가 지어낸 제목은 대개 표를 다시 읽게 만들 뿐이다.

    float-inventory.py                 캡션 없는 표를 파일별로
    float-inventory.py --all           캡션이 있는 것까지
    float-inventory.py ch38 ch39       특정 장만
    float-inventory.py --count         남은 개수만
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
KO = ROOT / "book"


def tables(path):
    """(순번, 줄번호, 캡션있음, 머리행, 앞 문장) 목록."""
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()
    out = []
    idx = 0
    for i, line in enumerate(lines):
        if not line.lstrip().startswith("#dtable("):
            continue
        idx += 1
        # 표의 머리행 = `[*…*]` 이 처음 늘어선 줄
        head, has_cap = "", False
        for j in range(i, min(i + 8, len(lines))):
            if "caption:" in lines[j]:
                has_cap = True
            if not head and "[*" in lines[j]:
                head = " / ".join(re.findall(r"\[\*([^\]]*?)\*\]", lines[j]))
        # 앞 문장 = 위로 올라가며 만나는 첫 산문 줄
        lead = ""
        for j in range(i - 1, max(-1, i - 8), -1):
            s = lines[j].strip()
            if s and not s.startswith(("#", "]", ")", "```", "//")):
                lead = s
                break
        out.append((idx, i + 1, has_cap, head, lead))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    only_count = "--count" in sys.argv

    files = sorted(set(KO.glob("*/*.typ")))          # chapters·parts·appendix·front·back
    files = [f for f in files if f.name not in ("lib.typ", "registry.typ", "main.typ")]
    files.sort(key=lambda f: (f.parent.name != "chapters", f.name))
    if args:
        files = [f for f in files if f.stem in args]

    total = missing = 0
    for f in files:
        rows = [r for r in tables(f) if show_all or not r[2]]
        got = tables(f)
        total += len(got)
        missing += sum(1 for r in got if not r[2])
        if not rows or only_count:
            continue
        print(f"\n── {f.relative_to(ROOT)}")
        for idx, ln, has_cap, head, lead in rows:
            mark = "✔" if has_cap else " "
            print(f"  {mark}#{idx:<3} L{ln:<5} 머리: {head[:76]}")
            if lead:
                print(f"            앞: {lead[:76]}")
    print(f"\nfloat-inventory: 표 {total}개 · 제목 없는 것 {missing}개")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
