#!/usr/bin/env python3
"""공개 저장소에 이 기계의 사정이 새지 않았는가 (저자 지시 2026-08-16).

이 저장소는 통째로 공개된다. 그래서 *이 기계에서만 뜻이 통하는 것*은 무엇도
들어가서는 안 된다 --- 절대 경로, 작업공간 이름, 사용자 이름, 인증서의 자리.

실제로 샜던 자리가 셋이다.
  · 53장 예제가 `argv[0]` 을 찍는데, 그 값이 빌드 디렉터리의 절대 경로였다.
    캡처된 출력이 그대로 책과 웹판에 인쇄되었다(PDF 444쪽).
  · `make-specimen.sh` 가 공용 typst 의 자리를 절대 경로로 적고 있었다.
  · `publish-specimen.sh` 가 *인증서 파일의 자리*를 절대 경로로 적고 있었다.

무엇을 잡는가
  1. 추적 중인 파일 안의 절대 경로 --- 이 기계의 자리, 사용자 홈, 윈도 사용자 폴더
  2. 인증서로 보이는 것 --- GitHub 토큰의 접두어
  3. 빌드 산출물(웹판 HTML, 캡처된 예제 출력)에 섞인 같은 것들

무엇을 잡지 않는가
  · `/usr/include`, `/etc/passwd` 처럼 *교재로서* 쓰는 시스템 경로
  · 이 파일 자신의 규칙 문자열

사용법:  python3 scripts/check-privacy.py
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SELF = pathlib.Path(__file__).name

# 이 기계의 자리를 가리키는 꼴. 교재에 나오는 시스템 경로(/usr, /etc, /proc …)는
# 뺀다 --- 그것들은 어느 기계에서나 같은 뜻이라 새는 것이 없다.
PATTERNS = [
    (re.compile(r"/opt/[A-Za-z0-9._-]+/[A-Za-z0-9._-]+"), "이 기계의 절대 경로"),
    (re.compile(r"/ho" + r"me/[a-z][a-z0-9_-]*/"), "사용자 홈 경로"),
    (re.compile(r"/Us" + r"ers/[A-Za-z][A-Za-z0-9_-]*/"), "사용자 홈 경로"),
    (re.compile(r"[A-Z]:\\\\Users\\\\[A-Za-z]"), "사용자 홈 경로"),
    (re.compile(r"gh[pousr]_[A-Za-z0-9]{20,}"), "GitHub 인증서"),
    (re.compile(r"github_pat_[A-Za-z0-9_]{20,}"), "GitHub 인증서"),
]

# 검사에서 뺄 것 --- 이진 산출물과 이 검사 자신
SKIP_SUFFIX = {".pdf", ".zip", ".png", ".jpg", ".woff2", ".ttf", ".otf", ".svg"}


def tracked_files():
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files"],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit("check-privacy: git 저장소가 아니다")
    for line in out.stdout.splitlines():
        p = ROOT / line
        if p.suffix.lower() in SKIP_SUFFIX or p.name == SELF:
            continue
        if p.is_file():
            yield p


def build_outputs():
    """추적하지 않지만 *책에 실리는* 것들 --- 캡처된 예제 출력."""
    for d in ("build/examples-out", "build/examples-out-en"):
        base = ROOT / d
        if base.exists():
            yield from sorted(base.rglob("*.out"))


def scan(paths, label):
    hits = []
    for p in paths:
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        if ";base64," in text and p.suffix in (".html", ".css"):
            # 글꼴을 data URI 로 박은 파일 --- 이진이 우연히 경로처럼 보인다
            text = re.sub(r";base64,[A-Za-z0-9+/=]+", ";base64,…", text)
        for rx, what in PATTERNS:
            for m in rx.finditer(text):
                line = text[:m.start()].count("\n") + 1
                rel = p.relative_to(ROOT)
                hits.append(f"{rel}:{line}: {what} — {m.group(0)[:60]}")
    return hits


def main() -> int:
    hits = scan(tracked_files(), "추적 파일") + scan(build_outputs(), "빌드 산출물")
    if hits:
        for h in hits:
            print("⚠️ ", h)
        print(f"check-privacy: {len(hits)} 건 — 공개 저장소에 이 기계의 사정이 있다")
        print("    경로는 저장소 기준의 상대 경로로, 인증서 자리는 환경 변수로 뺄 것")
        return 1
    print("check-privacy: 절대 경로도 인증서도 없다 "
          f"(추적 파일과 캡처 출력을 모두 훑었다)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
