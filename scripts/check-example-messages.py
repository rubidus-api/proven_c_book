#!/usr/bin/env python3
"""예제 소스의 입출력 메시지는 영어여야 한다 (저자 지시 2026-08-10).

주석은 한국어 그대로 둔다 --- 한국어판 독자가 읽는 설명이기 때문이다. 그러나
*프로그램이 찍는 글자*는 다르다 --- 출력은 소스의 일부이고, 다른 언어 화자가
그 예제를 실행했을 때 읽을 수 있어야 한다. 저자가 정한 규칙이다.

(영어판은 `examples-en/` 이라는 자체 트리를 쓴다. 두 트리 모두 검사한다.)

  ★ 주석 = 한국어  /  문자열 리터럴(=입출력 메시지) = 영어

처음 쟀을 때 96개 파일에 1024건이 있었다. 눈으로는 못 찾는다 --- 그래서 센다.

예외는 *한글 자체가 실험 대상*인 곳뿐이다. 문자 집합·와이드 문자·UTF-8 훑기·
조합형 인코딩 시연에서 "가"·"한글" 은 메시지가 아니라 *표본*이라, 영어로 바꾸면
예제가 증명하려던 것이 사라진다. 그런 자리는 docs/example-hangul-allow.tsv 에
파일 단위로 올린다.

사용법: python3 scripts/check-example-messages.py [--list]
종료 상태: 허용 목록에 없는 한국어 리터럴이 있으면 1
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TREES = (ROOT / "examples", ROOT / "examples-en")
ALLOW = ROOT / "docs" / "example-hangul-allow.tsv"
HAN = re.compile(r"[가-힣ㄱ-ㅎㅏ-ㅣ]")


def literals(src):
    """문자열 리터럴만 (줄번호, 내용) 으로 돌려준다. 주석 안은 세지 않는다."""
    i, n, line, st, start = 0, len(src), 1, "code", 0
    sl, out = 1, []
    while i < n:
        c = src[i]
        nx = src[i + 1] if i + 1 < n else ""
        if st == "code":
            if c == "/" and nx == "*":
                st = "blk"; i += 2; continue
            if c == "/" and nx == "/":
                st = "line"; i += 2; continue
            if c == '"':
                st = "str"; start = i + 1; sl = line; i += 1; continue
            if c == "'":
                st = "chr"; i += 1; continue
        elif st == "blk":
            if c == "*" and nx == "/":
                st = "code"; i += 2; continue
        elif st == "line":
            if c == "\n":
                st = "code"
        elif st == "str":
            if c == "\\":
                i += 2; continue
            if c == '"':
                out.append((sl, src[start:i])); st = "code"; i += 1; continue
        elif st == "chr":
            if c == "\\":
                i += 2; continue
            if c == "'":
                st = "code"
        if c == "\n":
            line += 1
        i += 1
    return out


def allow_files():
    out = set()
    if not ALLOW.exists():
        return out
    for line in ALLOW.read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.lstrip().startswith("#"):
            out.add(line.split("\t")[0].strip())
    return out


def main() -> int:
    allowed = allow_files()
    bad, spared, checked = [], 0, 0
    for tree in TREES:
      if not tree.exists():
        continue
      for f in sorted(tree.rglob("*.[ch]")):
        checked += 1
        rel = str(f.relative_to(tree))
        for ln, s in literals(f.read_text(encoding="utf-8")):
            if not HAN.search(s):
                continue
            if rel in allowed:   # 표본 파일은 두 트리 모두 같다
                spared += 1
            else:
                bad.append((f"{tree.name}/{rel}", ln, s))

    show = bad if "--list" in sys.argv else bad[:20]
    for rel, ln, s in show:
        print(f'  ⚠️  {rel}:{ln}  "{s[:70]}"')
    if bad:
        print("     주석은 한국어로 두어도 되지만, 프로그램이 찍는 글자는 영어다.")
        print("     한글 자체가 시연 대상이라면 docs/example-hangul-allow.tsv 에 올린다.")
    tail = f" (표본으로 허용한 것 {spared}건)" if spared else ""
    print(f"check-example-messages: 예제 {checked}개 — "
          f"{'한국어 메시지 ' + str(len(bad)) + '건' if bad else '입출력 메시지가 모두 영어다'}{tail}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
