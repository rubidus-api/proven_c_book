#!/usr/bin/env python3
"""판 번호를 한 곳에서 뿌린다 (저자 지적 2026-08-06: README 가 갱신되지 않았다).

단일 출처는 `book/main.typ` 의 `book-version` 이다. 여기서 읽어
`book-en/main.typ`·`VERSION.md`·`README.md`·`README-en.md` 를 맞춘다.

README 에서 갱신하는 자리는 둘뿐이다.
  ① "현재 판: vX.Y.Z" / "Current edition: vX.Y.Z"
  ② 내려받기 링크의 파일 이름 `proven_c_book-vX.Y.Z-…`
  ③ 릴리스 직접 링크의 태그 `…/releases/download/vX.Y.Z/…`
     — 이 셋 덕분에 새 판을 낼 때 PDF 바로받기 링크가 저절로 새 판을 가리킨다
본문 속 서술("since v0.9.0 …" 처럼 *역사*를 가리키는 번호)은 건드리지 않는다.

사용법:
    python3 scripts/sync-version.py            # 맞춘다
    python3 scripts/sync-version.py --check    # 어긋나면 1 을 돌려준다
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
VER = re.compile(r"v\d+\.\d+\.\d+")


def current() -> str:
    txt = (ROOT / "book" / "main.typ").read_text(encoding="utf-8")
    m = re.search(r'#let\s+book-version\s*=\s*"(v[\d.]+)"', txt)
    if not m:
        raise SystemExit("sync-version: book/main.typ 에서 book-version 을 찾지 못했다")
    return m.group(1)


def fix(path: pathlib.Path, ver: str) -> bool:
    """파일을 맞추고, 바뀌었으면 True."""
    text = path.read_text(encoding="utf-8")
    out = text

    if path.name == "main.typ":
        out = re.sub(r'(#let\s+book-version\s*=\s*")v[\d.]+(")',
                     r"\g<1>" + ver + r"\g<2>", out)
    elif path.name == "VERSION.md":
        out = re.sub(r"(Current:\s*\*\*)v[\d.]+(\*\*)",
                     r"\g<1>" + ver + r"\g<2>", out)
    else:  # README
        out = re.sub(r"((?:현재 판|Current edition)\*{0,2}:\s*\*{0,2})v[\d.]+",
                     r"\g<1>" + ver, out)
        # 내려받기 링크의 파일 이름과, 릴리스 URL 의 태그 부분을 함께 옮긴다
        out = re.sub(r"proven_c_book-v[\d.]+-", f"proven_c_book-{ver}-", out)
        out = re.sub(r"(/releases/download/)v[\d.]+/", r"\g<1>" + ver + "/", out)

    if out != text:
        path.write_text(out, encoding="utf-8")
        return True
    return False


def main() -> int:
    check = "--check" in sys.argv
    ver = current()
    targets = [ROOT / "book-en" / "main.typ", ROOT / "VERSION.md",
               ROOT / "README.md", ROOT / "README-en.md"]

    stale = []
    for path in targets:
        if not path.exists():
            continue
        before = path.read_text(encoding="utf-8")
        changed = fix(path, ver)
        if changed:
            stale.append(path.relative_to(ROOT))
            if check:                      # 검사 모드에서는 되돌린다
                path.write_text(before, encoding="utf-8")

    if check:
        if stale:
            for p in stale:
                print(f"⚠️  {p}: 판 번호가 {ver} 와 어긋난다")
            print("sync-version: 판 번호가 어긋난다 — "
                  "`python3 scripts/sync-version.py` 로 맞출 것")
            return 1
        print(f"sync-version: 판 번호 {ver} 가 모든 파일에서 일치한다")
        return 0

    if stale:
        for p in stale:
            print(f"맞춤: {p}")
    print(f"sync-version: {ver}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
