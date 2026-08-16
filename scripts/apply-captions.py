#!/usr/bin/env python3
"""표에 id 와 제목(캡션)을 붙인다 --- 두 판에 같은 순서로 (RFC-0029).

표준 입력으로 탭 구분 표를 받는다.

    파일줄기 <TAB> 표순번 <TAB> id <TAB> 한국어 제목 <TAB> English caption

같은 순번의 표는 두 판에서 같은 표다(표 수가 1:1로 맞는 것을 확인해 두었다).
`#dtable(` 바로 뒤(또는 `columns:` 줄 다음)에 두 줄을 끼워 넣는다.

    python3 scripts/apply-captions.py < 작업파일.tsv
    python3 scripts/apply-captions.py --dry-run < 작업파일.tsv
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def find_file(stem):
    for tree in ("book", "book-en"):
        for sub in ("chapters", "parts", "appendix", "front", "back"):
            p = ROOT / tree / sub / f"{stem}.typ"
            if p.exists():
                yield p
                break


def apply(path, want, dry):
    """want: {표순번: (id, 캡션)}"""
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    out, idx, done = [], 0, 0
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        if line.lstrip().startswith("#dtable("):
            idx += 1
            if idx in want:
                cid, cap = want[idx]
                # columns: 줄이 바로 뒤에 있으면 그 뒤에 넣는다
                j = i + 1
                if j < len(lines) and lines[j].lstrip().startswith("columns:"):
                    out.append(lines[j])
                    i = j
                if "caption:" not in "".join(lines[i:i + 3]):
                    # 들여쓰기는 표를 따라간다 --- 상자 안의 표는 더 들어가 있다
                    pad = " " * (len(line) - len(line.lstrip()) + 2)
                    out.append(f'{pad}id: "{cid}",\n')
                    out.append(f"{pad}caption: [{cap}],\n")
                    done += 1
        i += 1
    if not dry and done:
        path.write_text("".join(out), encoding="utf-8")
    return done


def main():
    dry = "--dry-run" in sys.argv
    rows = {}
    for raw in sys.stdin:
        raw = raw.rstrip("\n")
        if not raw.strip() or raw.startswith("#"):
            continue
        stem, idx, cid, ko, en = raw.split("\t")
        rows.setdefault(stem, {})[int(idx)] = (cid, ko, en)

    total = 0
    for stem, want in rows.items():
        for path in find_file(stem):
            lang = "ko" if "/book/" in str(path) else "en"
            per = {k: (v[0], v[1] if lang == "ko" else v[2]) for k, v in want.items()}
            n = apply(path, per, dry)
            total += n
            if n != len(per):
                print(f"  ⚠️  {path.relative_to(ROOT)}: {n}/{len(per)} 만 붙였다", file=sys.stderr)
    print(f"apply-captions: {total}곳" + ("  (--dry-run)" if dry else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
