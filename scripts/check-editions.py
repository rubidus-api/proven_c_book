#!/usr/bin/env python3
"""판 사이의 누락과 조판→HTML 누락을 잡는다 (2026-08-06 사고 두 건의 재발 방지).

사고 ①: 장 번호를 재배치한 뒤 영어판 `main.typ` 의 `translated` 목록이 옛
        마지막 번호(81)에 멈춰 있어, 원고가 있는 82·83장이 *빌드에서 통째로
        빠졌다*. PDF 에도 HTML 에도 없었고 아무도 경고하지 않았다.
사고 ②: `memrow` 도해가 조판 전용 요소(grid·box)로만 그려져 HTML 에서는
        캡션만 남고 그림이 사라졌다.

그래서 셋을 검사한다.
  1. 두 판의 장 파일 집합이 같은가.
  2. 영어판 `translated` 목록이 존재하는 모든 장을 담고 있는가.
  3. (docs 가 빌드돼 있으면) 원고의 표·도해·그림 개수가 HTML 산출물의
     개수와 같은가 — 조판에만 나오고 HTML 에서 사라지는 장치를 잡는다.

사용법:  python3 scripts/check-editions.py
"""
import glob
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

FLOATS = (
    ("dtable", '<figure class="tbl">'),
    ("memrow", 'class="memrow"'),
    ("figure-svg", '<figure class="fig">'),
    # ★ 2026-08-09: Typst 의 HTML 내보내기가 각주 *본문*을 버리는 것을 발견했다
    #   (0.15.1 — 참조 13개에 본문 1개). 그래서 출처 주석은 `#footnote` 가 아니라
    #   lib.typ 의 `note()` 를 쓴다. HTML 에서는 wrap-html.py 가 그것을 장 끝의
    #   「주」로 옮기면서 본문에 위첨자 `fnref` 를 남긴다 — 그 자국을 센다.
    ("note", 'class="fnref"'),
)


def chapter_numbers(edition):
    d = ROOT / edition / "chapters"
    return {int(p.stem[2:]) for p in d.glob("ch*.typ")}


def translated_list():
    txt = (ROOT / "book-en" / "main.typ").read_text(encoding="utf-8")
    m = re.search(r"#let\s+translated\s*=\s*\(([^)]*)\)", txt)
    if not m:
        return None
    return {int(x) for x in re.findall(r"\d+", m.group(1))}


def float_counts(edition, web):
    src = {}
    for name, _ in FLOATS:
        # 장치는 `#name(` 로도 `#name[` 로도 불린다 — 둘 다 센다.
        pat = r"#" + re.escape(name) + r"[(\[]"
        src[name] = sum(
            len(re.findall(pat, pathlib.Path(f).read_text(encoding="utf-8")))
            for f in glob.glob(str(ROOT / edition / "**" / "*.typ"), recursive=True)
            # 조판 견본은 책이 아니다 --- 세지 않는다(RFC-0027)
            if not f.endswith(("style-specimen.typ", "style.typ")))
    out = {}
    pages = glob.glob(str(ROOT / "docs" / web / "*.html"))
    for name, marker in FLOATS:
        out[name] = sum(pathlib.Path(f).read_text(encoding="utf-8").count(marker)
                        for f in pages)
    return src, out, len(pages)


def main() -> int:
    problems = []

    ko, en = chapter_numbers("book"), chapter_numbers("book-en")
    for n in sorted(ko - en):
        problems.append(f"영어판에 ch{n:02d}.typ 이 없다(번역 누락)")
    for n in sorted(en - ko):
        problems.append(f"한국어판에 ch{n:02d}.typ 이 없다(원본 누락)")

    tr = translated_list()
    if tr is None:
        problems.append("book-en/main.typ 에서 translated 목록을 찾지 못했다")
    else:
        for n in sorted(en - tr):
            problems.append(f"영어판 ch{n:02d} 원고가 있으나 translated 목록에 "
                            f"없다 — 빌드에서 통째로 빠진다")
        for n in sorted(tr - en):
            problems.append(f"translated 목록의 {n} 장 원고가 없다")

    for edition, web in (("book", "ko"), ("book-en", "en")):
        if not (ROOT / "docs" / web).exists():
            continue
        src, out, pages = float_counts(edition, web)
        if pages == 0:
            continue
        for name, _ in FLOATS:
            if src[name] != out[name]:
                problems.append(
                    f"{web}: {name} 이 원고 {src[name]}개인데 HTML 은 "
                    f"{out[name]}개 — HTML 에서 사라진 것이 있다")

    if problems:
        for p in problems:
            print("⚠️ ", p)
        print(f"check-editions: {len(problems)} 건")
        return 1
    print(f"check-editions: 두 판의 장 {len(ko)}개가 모두 빌드에 들어가고, "
          f"표·도해·그림이 HTML 에도 그대로 나온다")
    return 0


if __name__ == "__main__":
    sys.exit(main())
