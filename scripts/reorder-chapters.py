#!/usr/bin/env python3
"""장의 순서를 바꾼다 --- 등록부와 파일 이름만 (RFC-0028).

옛 `renumber-chapters.py` 는 원고 전체의 `N장` 을 다시 썼다(149파일·3,789곳).
장 참조가 이름이 된 뒤로 *원고는 전혀 건드리지 않는다.* 바뀌는 것은 둘뿐이다.

  ① `book/registry.typ` 의 순서
  ② 장 파일 이름 --- 이 책은 「N 번째 장 = `chapters/chNN.typ`」를 유지한다

쓰는 법
    reorder-chapters.py --move loops --after arrays-2d      한 장을 옮긴다
    reorder-chapters.py --insert scope --after linking       장을 새로 끼운다
    reorder-chapters.py --order id1,id2,…                    전체 순서를 준다
    reorder-chapters.py --dry-run …                          무엇이 바뀌는지만

`--insert` 는 등록부에 id 를 끼우고, 뒤 장들의 파일 이름을 한 칸씩 밀고, 두 판에
빈 장 파일을 만든다. 내용은 사람이 쓴다 --- 도구는 자리만 낸다.

옮긴 뒤에는 사람이 두 가지를 본다.
    scripts/check-counting-prose.py    「앞선 여덟 장」 같은 *개수* 서술
    부 도입부와 장 닫는 안내의 서사 --- 기계가 판정할 수 없는 자리
"""
import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "book" / "registry.typ"


def current_order():
    text = REGISTRY.read_text(encoding="utf-8")
    ids = []
    for m in re.finditer(r"chapters:\s*\(([^)]*)\)", text):
        ids += re.findall(r'"([^"]+)"', m.group(1))
    return ids


def write_order(new_ids):
    """등록부의 id 들을 새 순서로 바꿔 적는다 (부 경계는 그대로 --- 각 부의 장 수를 지킨다)."""
    text = REGISTRY.read_text(encoding="utf-8")
    sizes = [len(re.findall(r'"([^"]+)"', m.group(1)))
             for m in re.finditer(r"chapters:\s*\(([^)]*)\)", text)]
    assert sum(sizes) == len(new_ids), "장 수가 맞지 않는다"
    it = iter(new_ids)
    chunks = [[next(it) for _ in range(n)] for n in sizes]

    out, i = [], 0
    def repl(m):
        nonlocal i
        ids = chunks[i]; i += 1
        return "chapters: (" + ", ".join(f'"{x}"' for x in ids) + ",)"
    text = re.sub(r"chapters:\s*\(([^)]*)\)", repl, text)
    REGISTRY.write_text(text, encoding="utf-8")


def insert_id(text, newid, anchor, before):
    """등록부의 부 목록 안에 id 를 끼운다 --- 기준 장이 있는 부에 들어간다."""
    def repl(m):
        ids = re.findall(r'"([^"]+)"', m.group(1))
        if anchor not in ids:
            return m.group(0)
        at = ids.index(anchor) + (0 if before else 1)
        ids.insert(at, newid)
        return "chapters: (" + ", ".join(f'"{x}"' for x in ids) + ",)"
    return re.sub(r"chapters:\s*\(([^)]*)\)", repl, text)


def shift_files(old, new, dry):
    """장이 하나 늘었다 --- 뒤 장들의 파일 이름을 뒤에서부터 한 칸씩 민다."""
    pos_old = {cid: i + 1 for i, cid in enumerate(old)}
    moved = 0
    for ed in ("book", "book-en"):
        base = ROOT / ed / "chapters"
        for cid in reversed(old):          # 뒤에서부터 밀어야 서로 덮지 않는다
            a, b = pos_old[cid], new.index(cid) + 1
            if a == b:
                continue
            moved += 1
            if not dry:
                shutil.move(base / f"ch{a:02d}.typ", base / f"ch{b:02d}.typ")
    return moved


def make_stub(newid, no, dry):
    """두 판에 빈 장 파일을 만든다 --- 서두 정형은 사람이 채운다."""
    for ed, note in (("book", "// 새 장 --- 내용은 사람이 쓴다"),
                     ("book-en", "// new chapter --- the text is written by hand")):
        path = ROOT / ed / "chapters" / f"ch{no:02d}.typ"
        if dry:
            continue                       # 아직 밀지 않았으니 자리는 차 있다
        if path.exists():
            raise SystemExit(f"{path} 가 이미 있다 --- 자리를 잘못 냈다")
        path.write_text(f"{note} ({newid})\n", encoding="utf-8")


def rename_files(old, new, dry):
    """옛 순서에서의 위치 → 새 순서에서의 위치로 장 파일을 옮긴다 (두 단계)."""
    pos_old = {cid: i + 1 for i, cid in enumerate(old)}
    pos_new = {cid: i + 1 for i, cid in enumerate(new)}
    moved = 0
    for ed in ("book", "book-en"):
        base = ROOT / ed / "chapters"
        staged = []
        for cid in old:
            a, b = pos_old[cid], pos_new[cid]
            if a == b:
                continue
            src = base / f"ch{a:02d}.typ"
            tmp = base / f"__{cid}.tmp"
            staged.append((tmp, base / f"ch{b:02d}.typ"))
            moved += 1
            if not dry:
                shutil.move(src, tmp)
        for tmp, dest in staged:
            if not dry:
                shutil.move(tmp, dest)
    return moved


def main():
    dry = "--dry-run" in sys.argv
    old = current_order()
    new = None

    if "--insert" in sys.argv:
        who = sys.argv[sys.argv.index("--insert") + 1]
        if who in old:
            print(f"{who} 는 이미 있다", file=sys.stderr)
            return 1
        if "--after" in sys.argv:
            anchor, before = sys.argv[sys.argv.index("--after") + 1], False
        elif "--before" in sys.argv:
            anchor, before = sys.argv[sys.argv.index("--before") + 1], True
        else:
            print("--after 또는 --before 가 필요하다", file=sys.stderr)
            return 2
        if anchor not in old:
            print(f"기준 장 {anchor} 를 찾지 못했다", file=sys.stderr)
            return 1
        new = list(old)
        new.insert(new.index(anchor) + (0 if before else 1), who)
        moved = shift_files(old, new, dry)
        make_stub(who, new.index(who) + 1, dry)
        if not dry:
            REGISTRY.write_text(insert_id(REGISTRY.read_text(encoding="utf-8"),
                                          who, anchor, before), encoding="utf-8")
        print(f"장 신설: {who} --- 파일 {moved}개를 밀고 빈 장 둘을 만들었다"
              + ("  (--dry-run)" if dry else ""))
        print("이제 볼 것: 장 서두 정형, 규모 표기(check-counts), 부 도입부.")
        return 0

    if "--order" in sys.argv:
        new = sys.argv[sys.argv.index("--order") + 1].split(",")
    elif "--move" in sys.argv:
        who = sys.argv[sys.argv.index("--move") + 1]
        if "--after" in sys.argv:
            anchor, before = sys.argv[sys.argv.index("--after") + 1], False
        elif "--before" in sys.argv:
            anchor, before = sys.argv[sys.argv.index("--before") + 1], True
        else:
            print("--after 또는 --before 가 필요하다", file=sys.stderr)
            return 2
        new = [x for x in old if x != who]
        at = new.index(anchor) + (0 if before else 1)
        new.insert(at, who)
    else:
        print(__doc__)
        return 0

    if sorted(new) != sorted(old):
        print("id 집합이 달라졌다 --- 오타를 확인하라", file=sys.stderr)
        return 1

    moved = rename_files(old, new, dry)
    if not dry:
        write_order(new)
    print(f"순서 변경: 파일 {moved}개" + ("  (--dry-run)" if dry else ""))
    print("원고는 건드리지 않았다 --- 장 참조는 이름이라 그대로 맞는다.")
    print("이제 볼 것: scripts/check-counting-prose.py, 그리고 부 도입부·장 닫는 안내.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
