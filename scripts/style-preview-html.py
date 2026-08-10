#!/usr/bin/env python3
"""조판 견본의 HTML 판을 감싼다 (RFC-0027).

Typst 가 낸 알맹이에 실제 CSS 를 입혀 브라우저로 볼 수 있게 만든다.
`scripts/style-preview.sh` 가 부른다 --- 따로 쓸 일은 없다.

  style-preview-html.py <raw> <out> <css> [--font-prefix P] [--embed-fonts D]

★ 글꼴을 어떻게 딸려 보내는가
  CSS 는 `url("../fonts/…")` 로 글꼴을 찾는다. 이 경로는 `docs/ko/*.html`
  기준이라, 견본을 다른 깊이에 두면 글꼴이 사라져 *디자인이 아닌 것*을 보게
  된다. 그래서 두 길을 둔다.
    --font-prefix  : 상대 경로만 바꾼다 (같은 저장소 안에 둘 때)
    --embed-fonts  : woff2 를 data: 로 심는다 (혼자 돌아다닐 파일)
"""
import base64
import pathlib
import re
import sys

args = sys.argv[1:]
opts = {}
pos = []
i = 0
while i < len(args):
    if args[i].startswith("--"):
        opts[args[i][2:]] = args[i + 1]
        i += 2
    else:
        pos.append(args[i])
        i += 1

raw, out, css_path = (pathlib.Path(p) for p in pos[:3])
inner = raw.read_text(encoding="utf-8")
m = re.search(r"<body[^>]*>(.*)</body>", inner, re.S)
body = m.group(1) if m else inner
css = css_path.read_text(encoding="utf-8")

if "font-prefix" in opts:
    css = css.replace('url("../fonts/', 'url("%s' % opts["font-prefix"])

if "embed-fonts" in opts:
    fdir = pathlib.Path(opts["embed-fonts"])

    def _embed(mo):
        f = fdir / mo.group(1)
        if not f.exists():
            return mo.group(0)
        b64 = base64.b64encode(f.read_bytes()).decode("ascii")
        return 'url("data:font/woff2;base64,%s"' % b64

    css = re.sub(r'url\("\.\./fonts/([^"]+)"', _embed, css)

out.write_text(
    '<!doctype html><html lang="ko"><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width, initial-scale=1">'
    "<title>Proven C Book — 서식 예시</title><style>"
    + css
    + '</style></head><body><main><div class="wrap">'
    + body
    + "</div></main></body></html>",
    encoding="utf-8",
)
print(f"style-preview-html: {out.name} ({out.stat().st_size // 1024} KB)")
