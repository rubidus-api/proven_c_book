#!/usr/bin/env bash
# 웹 판에 실을 글꼴을 만든다 (RFC-0026 D2, 저자 지시 2026-08-10).
#
# ★ 왜 필요한가 --- 전에는 `@font-face` 가 하나도 없었다. CSS 가 이름만 적어
# 두었으므로 그 글꼴이 *방문자 기계에 설치되어 있어야* 했는데, `Noto Serif CJK KR`
# 은 리눅스 데스크톱 밖에서는 드물다. 그러면 대체 글꼴로 떨어져 --- 세리프 본문이
# 고딕으로 보인다. 우리가 본 화면과 독자가 본 화면이 달랐던 것이다.
#
# ★ 이 책은 글꼴을 *세 갈래*만 쓴다(저자 확인 2026-08-10):
#     serif  = 본문      sans-serif = 제목·UI      monospace = 코드
#   그래서 실어야 할 것도 그 셋뿐이다.
#
# 용량은 서브셋으로 잡는다. 웹 판에 실제로 쓰인 글자만 남기면 한글 세리프가
# 수 MB 에서 150KB 남짓으로 줄어든다. 쓰인 글자는 만들어진 HTML 에서 뽑는다 ---
# 그래서 이 스크립트는 build-html.sh *뒤에* 돌려야 한다.
#
# 필요한 것: fonttools + brotli. 없으면 가상환경을 만들어 깐다.
#
# 사용법: scripts/make-webfonts.sh
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
fonts=${FONT_PATH:-$root/../toolchains/fonts}
out="$root/docs/fonts"
venv=${WEBFONT_VENV:-$root/build/fontvenv}

if [ ! -x "$venv/bin/pyftsubset" ]; then
  echo "make-webfonts: 도구를 준비한다 ($venv)"
  python3 -m venv "$venv"
  "$venv/bin/pip" install --quiet fonttools brotli
fi

mkdir -p "$out"
chars="$root/build/used-chars.txt"
python3 - "$root/docs" "$chars" <<'PY'
import html, pathlib, re, sys
docs, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
chars = set()
for lang in ("ko", "en"):
    d = docs / lang
    if not d.exists():
        continue
    for f in d.glob("*.html"):
        t = html.unescape(re.sub(r"<[^>]+>", " ", f.read_text(encoding="utf-8")))
        chars.update(t)
chars = {c for c in chars if ord(c) >= 0x20}
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("".join(sorted(chars)), encoding="utf-8")
print(f"make-webfonts: 웹 판에 쓰인 글자 {len(chars)}자")
PY

sub() {                      # sub <나갈이름> <원본>
  [ -f "$2" ] || { echo "  ⚠️  없는 글꼴: $2" >&2; return 0; }
  "$venv/bin/pyftsubset" "$2" --text-file="$chars" --flavor=woff2 \
      --layout-features='' --no-hinting --desubroutinize \
      --output-file="$out/$1.woff2"
}

# 세 갈래 × (한국어판 CJK / 영어판 라틴)
sub serif-kr       "$fonts/noto-cjk-kr/NotoSerifCJKkr-Regular.otf"
sub serif-kr-bold  "$fonts/noto-cjk-kr/NotoSerifCJKkr-Bold.otf"
sub sans-kr        "$fonts/noto-cjk-kr/NotoSansCJKkr-Regular.otf"
sub sans-kr-bold   "$fonts/noto-cjk-kr/NotoSansCJKkr-Bold.otf"
sub serif          "$fonts/noto-latin/NotoSerif-Regular.ttf"
sub serif-bold     "$fonts/noto-latin/NotoSerif-Bold.ttf"
sub serif-italic   "$fonts/noto-latin/NotoSerif-Italic.ttf"
sub sans           "$fonts/noto-latin/NotoSans-Regular.ttf"
sub sans-bold      "$fonts/noto-latin/NotoSans-Bold.ttf"
sub mono           "$fonts/d2coding/D2Coding-Ver1.3.2-20180524.ttf"
sub mono-bold      "$fonts/d2coding/D2CodingBold-Ver1.3.2-20180524.ttf"

# 라이선스를 함께 싣는다 --- Noto 와 D2Coding 은 둘 다 SIL Open Font License 1.1 이라
# 재배포할 수 있지만, 그 조건이 라이선스 사본을 함께 두는 것이다.
cat > "$out/README.md" <<'EOF'
# 웹 판에 실은 글꼴

이 디렉터리의 `.woff2` 파일은 아래 글꼴을 *이 책에 쓰인 글자만 남겨* 줄인 것이다
(`scripts/make-webfonts.sh` 가 만든다. 손으로 고치지 않는다).

| 파일 | 원본 | 라이선스 |
|---|---|---|
| `serif-kr*`, `sans-kr*` | Noto Serif / Sans CJK KR (Google) | SIL Open Font License 1.1 |
| `serif*`, `sans*` | Noto Serif / Noto Sans (Google) | SIL Open Font License 1.1 |
| `mono*` | D2Coding (NAVER) | SIL Open Font License 1.1 |

세 파일 모두 OFL 1.1 이므로 서브셋과 재배포가 허용된다. 전문은 각 배포처를 보라 ---
Noto: `github.com/notofonts`, D2Coding: `github.com/naver/d2codingfont`.
EOF

echo "make-webfonts: $(ls "$out"/*.woff2 | wc -l) 벌 · 합계 $(du -sh "$out" | cut -f1) → docs/fonts/"
