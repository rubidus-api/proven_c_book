#!/usr/bin/env python3
"""장 번호 재배치 (RFC-0009 §3).

두 장을 끼우면 뒤 번호가 밀린다. 매핑은 함수 `remap` 하나로 정의하고,
원고·예제·경로·설정을 모두 그 매핑으로 옮긴다.

안전 규칙 (LESSONS 2026-08-04 의 사고 재발 방지):
  1. **두 단계 자리표시자 치환** — 먼저 «old» 로 바꾸고 그다음 새 번호로.
     한 자리 수 패스가 두 자리 결과를 다시 치환하는 오염이 원천 봉쇄된다.
  2. **복합형을 함께 잡는다** — `A·B장`, `A~B장`, `chapters A and B`,
     `ch. A, B` 처럼 숫자가 여럿인 형태도 정규식 하나로 묶어 처리한다.
  3. 파일·디렉터리·`#demo` 경로·sync.json 키까지 같은 매핑으로 옮긴다.

사용법:
    python3 scripts/renumber-chapters.py --dry-run     # 통계만
    python3 scripts/renumber-chapters.py               # 실제 적용
"""
import json
import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# 새로 끼우는 장: 52~53(이름 공간 2장, RFC-0015), 51장 바로 다음
INSERTS = (52, 53)


def remap(n: int) -> int:
    """옛 장 번호 → 새 장 번호."""
    return n if n < 52 else n + 2


PLACE = "«{}»"   # «n»

# 한국어: 33장 / 26·27장 / 5~6장
KO_GROUP = re.compile(r"((?:\d+\s*[·~,]\s*)*\d+)\s*장")
# 영어: chapter 33 / chapters 26 and 27 / ch. 21, 24, 50
EN_GROUP = re.compile(r"\b(chapters?|ch\.)(\s+)((?:\d+\s*(?:,|and|–|-|to)\s*)*\d+)", re.I)
NUM = re.compile(r"\d+")


def stage1(text: str) -> tuple[str, int]:
    """모든 장 참조의 숫자를 «n» 자리표시자로 바꾼다."""
    count = 0

    def nums_to_place(s: str) -> str:
        nonlocal count
        def one(m):
            nonlocal count
            count += 1
            return PLACE.format(m.group(0))
        return NUM.sub(one, s)

    text = KO_GROUP.sub(lambda m: nums_to_place(m.group(1)) + "장", text)
    text = EN_GROUP.sub(
        lambda m: m.group(1) + m.group(2) + nums_to_place(m.group(3)), text)
    return text, count


def stage2(text: str) -> str:
    """«n» → 새 번호."""
    return re.sub(r"«(\d+)»", lambda m: str(remap(int(m.group(1)))), text)


def convert(text: str) -> tuple[str, int]:
    t, c = stage1(text)
    return stage2(t), c


def targets():
    """본문·예제에서 장 참조가 들어 있는 파일들."""
    for ed in ("book", "book-en"):
        yield from (ROOT / ed).rglob("*.typ")
    for tree in ("examples", "examples-en"):
        d = ROOT / tree
        if d.exists():
            yield from d.rglob("*.c")
            yield from d.rglob("*.h")


def rename_chapter_files(dry: bool) -> int:
    moved = 0
    for ed in ("book", "book-en"):
        chdir = ROOT / ed / "chapters"
        # 큰 번호부터 옮겨야 덮어쓰지 않는다
        for path in sorted(chdir.glob("ch*.typ"), reverse=True):
            old = int(path.stem[2:])
            new = remap(old)
            if new == old:
                continue
            dest = chdir / f"ch{new:02d}.typ"
            moved += 1
            if not dry:
                path.rename(dest)
    return moved


def rename_example_dirs(dry: bool) -> int:
    moved = 0
    for tree in ("examples", "examples-en"):
        base = ROOT / tree
        if not base.exists():
            continue
        for path in sorted([p for p in base.iterdir() if p.is_dir() and re.fullmatch(r"ch\d+", p.name)],
                           key=lambda p: -int(p.name[2:])):
            old = int(path.name[2:])
            new = remap(old)
            if new == old:
                continue
            moved += 1
            if not dry:
                path.rename(base / f"ch{new}")
    return moved


def rewrite_demo_paths(dry: bool) -> int:
    """#demo("examples/ch36/...") 의 번호를 옮긴다."""
    pat = re.compile(r'((?:examples|examples-en)/ch)(\d+)(/)')
    changed = 0
    for path in (list((ROOT / "book").rglob("*.typ")) + list((ROOT / "book-en").rglob("*.typ"))):
        s = path.read_text(encoding="utf-8")
        new = pat.sub(lambda m: m.group(1) + str(remap(int(m.group(2)))) + m.group(3), s)
        if new != s:
            changed += 1
            if not dry:
                path.write_text(new, encoding="utf-8")
    return changed


def rewrite_sync(dry: bool) -> int:
    p = ROOT / "book-en" / "sync.json"
    if not p.exists():
        return 0
    db = json.loads(p.read_text())
    out = {}
    for key, val in db.items():
        m = re.fullmatch(r"book/chapters/ch(\d+)\.typ", key)
        out[f"book/chapters/ch{remap(int(m.group(1))):02d}.typ" if m else key] = val
    if not dry:
        p.write_text(json.dumps(out, indent=2, ensure_ascii=False, sort_keys=True) + "\n")
    return len(out)


def main() -> int:
    dry = "--dry-run" in sys.argv
    refs = files = 0
    for path in targets():
        s = path.read_text(encoding="utf-8")
        new, c = convert(s)
        if new != s:
            files += 1
            refs += c
            if not dry:
                path.write_text(new, encoding="utf-8")
    print(f"참조 치환: {refs}곳 / {files}파일")
    print(f"장 파일 이름 변경: {rename_chapter_files(dry)}개")
    print(f"예제 디렉터리 이름 변경: {rename_example_dirs(dry)}개")
    print(f"demo 경로 갱신: {rewrite_demo_paths(dry)}파일")
    print(f"sync.json 키: {rewrite_sync(dry)}개")
    if dry:
        print("(--dry-run: 아무것도 쓰지 않았다)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
