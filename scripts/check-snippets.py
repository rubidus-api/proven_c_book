#!/usr/bin/env python3
"""지면에 실린 C 조각이 컴파일되는지 본다.

★ 왜 필요한가 --- 예제(`#demo`)는 매 빌드마다 컴파일되고 돌아간다. 그런데 본문과
  표 안에 손으로 적은 코드는 *한 번도 컴파일된 적이 없었다.* 부록 P 의 표에
  `__flash char msg[] = "hi";` 가 실려 있었고, AVR 은 그 자리에서 `const` 를
  요구하며 거절한다. 눈으로 본 코드는 컴파일된 코드가 아니다.

★ 그런데 지면의 조각은 대개 *일부러* 조각이다. 선언 없는 파편, `…` 자리표,
  문법 발췌, 일부러 틀린 코드. 그래서 「전부 컴파일돼야 한다」는 규칙은 세울 수
  없다. 대신 이 검사는 *기준선*을 쓴다 --- 지금 컴파일되지 않는 조각의 목록을
  적어 두고, **그 목록이 늘거나 달라지면** 문다. 새로 적은 코드가 컴파일되지
  않으면 그 자리에서 드러난다.

★ 대상 전용 낱말(`__flash`·`__memx`)이 든 조각은 x86 컴파일러가 아니라
  AVR 컴파일러로 본다. 그것이 부록 P 의 결함을 잡는 자리다.

  check-snippets.py           무엇이 컴파일되고 무엇이 안 되는지 센다
  check-snippets.py --check   기준선과 다르면 1 을 돌려준다
  check-snippets.py --write   지금 상태를 기준선으로 적는다
"""
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASE = ROOT / "docs" / "snippet-baseline.tsv"
AVR = ROOT.parent / "usr/toolchains/avr-gcc-16.1.0-x64-linux/bin/avr-gcc"

BLOCK = re.compile(r"^```c\n(.*?)^```", re.M | re.S)
HEAD = ("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
        "#include <stddef.h>\n#include <stdint.h>\n#include <stdbool.h>\n"
        "#include <limits.h>\n#include <math.h>\n")
AVR_WORDS = re.compile(r"\b__(?:flash|flashx|memx)\b")


def snippets():
    for base in ("book", "book-en"):
        for f in sorted((ROOT / base).rglob("*.typ")):
            text = f.read_text(encoding="utf-8")
            for i, m in enumerate(BLOCK.finditer(text)):
                yield f"{f.relative_to(ROOT)}#{i + 1}", m.group(1)


def compiles(src, cc):
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as fh:
        fh.write(src)
        path = fh.name
    try:
        args = list(cc) + ["-fsyntax-only", "-w", path]
        return subprocess.run(args, capture_output=True, text=True).returncode == 0
    finally:
        os.unlink(path)


def judge(code):
    """이 조각이 어느 컴파일러로, 어떤 껍데기 안에서 서는가."""
    if AVR_WORDS.search(code):
        if not AVR.is_file():
            return "skipped"                       # 도구가 없으면 판정하지 않는다
        cc = [str(AVR), "-mmcu=atmega328p", "-std=gnu23"]
    else:
        cc = ["gcc", "-std=c23"]
    if compiles(HEAD + code, cc):
        return "ok"
    if compiles(HEAD + "void _f(void){\n" + code + "\n}\n", cc):
        return "ok"
    return "fail"


def baseline():
    if not BASE.exists():
        return set()
    return {line.split("\t")[0].strip()
            for line in BASE.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.lstrip().startswith("#")}


def main():
    failing, ok, skipped = [], 0, 0
    for name, code in snippets():
        state = judge(code)
        if state == "ok":
            ok += 1
        elif state == "skipped":
            skipped += 1
        else:
            failing.append(name)

    now, was = set(failing), baseline()
    new, gone = sorted(now - was), sorted(was - now)

    if "--write" in sys.argv:
        BASE.write_text(
            "# 지금 컴파일되지 않는 지면의 조각 (check-snippets.py)\n"
            "#\n"
            "# 대개 *일부러* 조각이다 --- 선언 없는 파편, `…` 자리표, 문법 발췌,\n"
            "# 일부러 틀린 코드. 여기 적힌 것은 「그래도 좋다」가 아니라\n"
            "# 「지금은 이렇다」이고, 이 목록이 *달라지면* 검사가 문다.\n"
            + "".join(f"{n}\n" for n in sorted(now)), encoding="utf-8")
        print(f"check-snippets: 기준선 갱신 --- {len(now)}건 → {BASE.relative_to(ROOT)}")
        return 0

    tail = f" · 도구가 없어 건너뜀 {skipped}" if skipped else ""
    if "--check" in sys.argv and (new or gone):
        for n in new:
            print(f"  ⚠️  새로 컴파일되지 않는다: {n}")
        for n in gone:
            print(f"  · 이제 컴파일된다(기준선에서 지울 것): {n}")
        print(f"check-snippets: 조각 {ok + len(now)}개 · 기준선과 다르다 "
              f"(새로 {len(new)}건, 사라진 것 {len(gone)}건)")
        print("  맞다면 --write 로 기준선을 갱신한다.")
        return 1

    print(f"check-snippets: 지면의 조각 {ok + len(now)}개 --- "
          f"컴파일됨 {ok} · 일부러 조각 {len(now)}(기준선과 같다){tail}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
