#!/usr/bin/env python3
"""연습문제와 해답의 짝을 본다 (RFC-0060).

문제는 `book/exercises/exNN.typ`(부마다), 해답은 `book/back/answers.typ` 에 있고, 영어판도
같은 자리에 있다. 이 검사가 보는 것:
  ① 문제마다 같은 id 의 해답이 있고, 해답마다 같은 id 의 문제가 있다.
  ② id 는 그 파일의 부 번호로 시작한다(ex07.typ 의 문제는 "7-…").
  ③ 두 판의 id 목록이 같다.
  ④ (--check 에서만) 빈자리 표시 `#exercises-pending()` 가 하나도 남지 않았다.

  check-exercises.py            짝과 빈자리를 센다(짝이 어긋나면 1)
  check-exercises.py --check    빈자리가 남아도 1 --- 릴리스 게이트
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EX = re.compile(r'#exercise\(\s*"([^"]+)"')
ANS = re.compile(r'#answer\(\s*"([^"]+)"')
PENDING = "#exercises-pending()"


def scan(tree):
    exs, problems, pending = {}, [], 0
    for f in sorted((ROOT / tree / "exercises").glob("ex*.typ")):
        part = int(f.stem[2:])
        text = f.read_text(encoding="utf-8")
        pending += text.count(PENDING)
        for i in EX.findall(text):
            if not i.startswith(f"{part}-"):
                problems.append(f"{f.relative_to(ROOT)}: 문제 {i} 는 제{part}부 id 가 아니다")
            if i in exs:
                problems.append(f"{f.relative_to(ROOT)}: 문제 {i} 가 두 번 나온다")
            exs[i] = f
    ans_file = ROOT / tree / "back" / "answers.typ"
    text = ans_file.read_text(encoding="utf-8") if ans_file.exists() else ""
    pending += text.count(PENDING)
    answers = ANS.findall(text)
    for i in sorted(set(exs) - set(answers)):
        problems.append(f"{tree}: 문제 {i} 에 해답이 없다")
    for i in sorted(set(answers) - set(exs)):
        problems.append(f"{tree}: 해답 {i} 에 문제가 없다")
    return set(exs), problems, pending


def main():
    check = "--check" in sys.argv
    ko, p_ko, n_ko = scan("book")
    en, p_en, n_en = scan("book-en")
    problems = p_ko + p_en
    if ko != en:
        problems.append(f"두 판의 문제 id 가 다르다: 한국어판에만 {sorted(ko - en)}, 영어판에만 {sorted(en - ko)}")
    for p in problems:
        print("  ✗ " + p)
    print(f"check-exercises: 문제 {len(ko)}개(한) · {len(en)}개(영) · 빈자리 {n_ko}곳(한) · {n_en}곳(영) · 어긋남 {len(problems)}건")
    if problems:
        return 1
    if check and (n_ko or n_en):
        print("check-exercises: 아직 채우지 않은 빈자리가 있다 --- 배포하지 않는다")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
