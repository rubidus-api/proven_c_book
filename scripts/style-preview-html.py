#!/usr/bin/env python3
"""조판 견본의 HTML 판을 감싼다 (RFC-0027).

Typst 가 낸 알맹이에 실제 CSS 를 입혀 브라우저로 볼 수 있게 만든다.
`scripts/style-preview.sh` 가 부른다 --- 따로 쓸 일은 없다.
"""
import pathlib
import re
import sys

raw, out, css = (pathlib.Path(p) for p in sys.argv[1:4])
inner = raw.read_text(encoding="utf-8")
m = re.search(r"<body[^>]*>(.*)</body>", inner, re.S)
body = m.group(1) if m else inner
out.write_text(
    '<!doctype html><html lang="ko"><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width, initial-scale=1">'
    "<title>style preview</title><style>"
    + css.read_text(encoding="utf-8")
    + '</style></head><body><main><div class="wrap">'
    + body
    + "</div></main></body></html>",
    encoding="utf-8",
)
print(f"style-preview-html: {out.name}")
