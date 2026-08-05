#!/usr/bin/env python3
"""Typst 의 실험적 HTML 출력에 읽기 좋은 스타일과 골격을 입힌다."""
import sys, re, pathlib

lang, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
html = pathlib.Path(src).read_text(encoding="utf-8")

body = html
m = re.search(r"<body[^>]*>(.*)</body>", html, re.S)
if m:
    body = m.group(1)

TITLE = {"ko": "Proven C Book — proven 라이브러리에 기반한 모던 C 입문",
         "en": "Proven C Book — Modern C, built on the proven library"}[lang]
NOTE = {"ko": ("이 판은 <strong>초안(draft)</strong>이다. "
               "쪽 번호가 필요한 찾아보기와 조판이 정확한 판은 "
               '<a href="../../dist/">PDF</a>를 보라.'),
        "en": ("This edition is a <strong>draft</strong>. "
               "For the paginated index and exact typesetting, see the "
               '<a href="../../dist/">PDF</a>.')}[lang]
OTHER = {"ko": '<a href="../en/">English</a>', "en": '<a href="../ko/">한국어</a>'}[lang]

css = """
:root { color-scheme: light dark; --fg:#1a1a1a; --bg:#fdfdfc; --muted:#666;
        --rule:#ddd; --code-bg:#f5f5f3; --accent:#3b6ea5; }
@media (prefers-color-scheme: dark) {
  :root { --fg:#e6e6e6; --bg:#16181a; --muted:#9aa0a6; --rule:#333;
          --code-bg:#1f2225; --accent:#7aa9d6; }
}
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--fg);
       font-family: "Noto Serif CJK KR","Noto Serif KR",Georgia,serif;
       line-height:1.7; }
.topbar { position:sticky; top:0; z-index:10; background:var(--bg);
          border-bottom:1px solid var(--rule); padding:.6rem 1rem;
          display:flex; gap:1rem; align-items:center; font-size:.9rem; }
.topbar strong { font-family: "Noto Sans CJK KR",system-ui,sans-serif; }
.topbar .spacer { flex:1 1 auto; }
main { max-width: 46rem; margin: 0 auto; padding: 1.5rem 1.1rem 6rem; }
.note { border-left:3px solid var(--accent); background:var(--code-bg);
        padding:.7rem .9rem; margin:1.2rem 0; font-size:.92rem; }
h1,h2,h3 { font-family:"Noto Sans CJK KR",system-ui,sans-serif; line-height:1.35; }
h1 { font-size:1.7rem; margin:2.4rem 0 1rem; padding-top:1rem;
     border-top:2px solid var(--rule); }
h2 { font-size:1.25rem; margin:2rem 0 .6rem; }
h3 { font-size:1.05rem; margin:1.4rem 0 .4rem; }
p { margin:.7rem 0; }
pre, code { font-family:"D2Coding",ui-monospace,SFMono-Regular,Menlo,monospace; }
code { background:var(--code-bg); padding:.1em .3em; border-radius:3px;
       font-size:.92em; }
pre { background:var(--code-bg); padding:.8rem 1rem; border-radius:6px;
      overflow-x:auto; line-height:1.5; }
pre code { background:none; padding:0; }
table { border-collapse:collapse; width:100%; margin:1rem 0; font-size:.93rem;
        display:block; overflow-x:auto; }
th,td { border:1px solid var(--rule); padding:.4rem .55rem; text-align:left;
        vertical-align:top; }
th { background:var(--code-bg); }
a { color:var(--accent); }
figure { margin:1rem 0; }
"""

out = f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{TITLE}</title>
<style>{css}</style>
</head>
<body>
<div class="topbar"><strong>Proven C Book</strong>
  <span class="spacer"></span>{OTHER}
  <a href="https://github.com/rubidus-api/proven_c_book">GitHub</a></div>
<main>
<div class="note">{NOTE}</div>
{body}
</main>
</body>
</html>
"""
pathlib.Path(dst).write_text(out, encoding="utf-8")
print(f"wrap-html: {dst} ({len(out)//1024} KB)")
