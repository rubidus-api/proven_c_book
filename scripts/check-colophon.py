#!/usr/bin/env python3
"""판권(저작권과 연락처)이 두 판·두 매체에서 같은 말을 하는지 대조한다.

★ 왜 필요한가
  판권 문구가 세 자리에 있다 --- `book/main.typ`(텍스트 한국어판),
  `book-en/main.typ`(텍스트 영어판), `scripts/wrap-html.py`(웹판 두 언어).
  조판 도구가 서로 달라 한 곳에서 생성할 수가 없다. 그러면 남는 길은 하나다:
  *어긋나면 알아채게 하는 것.* 라이선스 이름과 번호는 사람이 눈으로 맞추기에
  가장 쉽게 틀리는 자리라, 그것만 기계로 잡는다.

무엇을 검사하는가
  라이선스 식별자(CC BY-NC-SA 4.0 · MIT)와 그 URL 이 네 자리에 모두 있는가.
  문장 전체를 대조하지는 않는다 --- 언어와 매체가 다르면 문장은 달라도 된다.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
NEEDED = ["CC BY-NC-SA 4.0", "MIT", "creativecommons.org/licenses/by-nc-sa/4.0"]


def colophon_of_typst(path):
    src = path.read_text(encoding="utf-8")
    m = re.search(r"(저작권과 연락처|Copyright and contact)(.*?)\n\]", src, re.S)
    return m.group(0) if m else ""


def colophon_of_web(path):
    if not path.exists():
        return None
    s = path.read_text(encoding="utf-8")
    m = re.search(r'<section class="colophon".*?</section>', s, re.S)
    return m.group(0) if m else ""


def main():
    places = {
        "book/main.typ": colophon_of_typst(ROOT / "book" / "main.typ"),
        "book-en/main.typ": colophon_of_typst(ROOT / "book-en" / "main.typ"),
    }
    for lang in ("ko", "en"):
        got = colophon_of_web(ROOT / "docs" / lang / "index.html")
        if got is not None:
            places[f"docs/{lang}/index.html"] = got

    bad = []
    for where, text in places.items():
        if not text:
            bad.append(f"{where}: 판권을 찾지 못했다")
            continue
        for token in NEEDED:
            if token not in text:
                bad.append(f"{where}: 「{token}」 가 없다")

    for line in bad:
        print("·  " + line, file=sys.stderr)
    if bad:
        print("check-colophon: 판권이 자리마다 다르다", file=sys.stderr)
        return 1
    print(f"check-colophon: 판권 {len(places)}자리가 같은 라이선스를 말한다")
    return 0


if __name__ == "__main__":
    sys.exit(main())
