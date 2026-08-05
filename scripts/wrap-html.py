#!/usr/bin/env python3
"""Typst 의 실험적 HTML 출력을 장별 페이지로 나누고 읽기 좋게 꾸민다."""
import sys, re, pathlib, html as htmlmod

lang, src, out_dir = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3])
raw = pathlib.Path(src).read_text(encoding="utf-8")
m = re.search(r"<body[^>]*>(.*)</body>", raw, re.S)
body = m.group(1) if m else raw

STR = {
 "ko": dict(title="Proven C Book — proven 라이브러리에 기반한 모던 C 입문",
   short="Proven C Book", other="English", other_href="../en/",
   toc="목차", prev="이전", next="다음", top="목차로", extra="부록과 찾아보기",
   note=('이 판은 <strong>초안(draft)</strong>이다. 쪽 번호가 붙은 찾아보기와 정확한 조판은 '
         '<a href="https://github.com/rubidus-api/proven_c_book/tree/main/dist">PDF</a>를 보라.')),
 "en": dict(title="Proven C Book — Modern C, built on the proven library",
   short="Proven C Book", other="한국어", other_href="../ko/",
   toc="Contents", prev="Prev", next="Next", top="Contents", extra="Front and back matter",
   note=('This edition is a <strong>draft</strong>. For the paginated index and exact '
         'typesetting, see the '
         '<a href="https://github.com/rubidus-api/proven_c_book/tree/main/dist">PDF</a>.')),
}[lang]

body = re.sub(r'<ol style="list-style-type: none">.*?</ol>\s*(?=<h2)', "", body, count=1, flags=re.S)

# 아이콘이 붙는 장치는 언어와 무관하고, 글자 라벨만 판마다 다르다
# (라벨의 단일 출처는 book/lib.typ 의 _L 이다).
DEVICES = [("이 장이 끝나면","organizer"),("By the end of this chapter","organizer"),
           ("돌아보기","deepqa"),("Looking back","deepqa"),
           ("⚠","misconception"),("◉","realcase"),("✗","antipattern"),
           ("∑","mathbox"),("☰","recap"),("⊞","platform"),
           ("문","qa-q"),("답","qa-a"),("Q","qa-q"),("A","qa-a")]

def _is_label(text, prefix):
    """라벨 판정: 정확히 같거나, 라벨 뒤에 공백이 와야 한다.
    접두 일치만 보면 `문`이 "문자열…"을, `A` 가 "A bundle…"을 물어 버린다.
    아이콘(⚠·◉ 등)은 글자가 아니므로 뒤에 무엇이 오든 라벨로 본다."""
    if text == prefix:
        return True
    if not text.startswith(prefix):
        return False
    last = prefix[-1]
    if not (last.isalnum() or "가" <= last <= "힣"):
        return True
    return text[len(prefix):len(prefix) + 1] in (" ", " ")


def tag_devices(text):
    def repl(mo):
        inner = mo.group(1)
        first = re.search(r"<p>(.*?)</p>", inner, re.S)
        if first:
            label = re.sub(r"<[^>]+>", "", first.group(1)).strip()
            for prefix, cls in DEVICES:
                if _is_label(label, prefix):
                    head = re.sub(r"<p>(.*?)</p>", r'<p class="dev-label">\1</p>', inner, count=1, flags=re.S)
                    return f'<div class="dev {cls}">{head}</div>'
            return mo.group(0)
        # <p> 없이 라벨이 앞에 붙은 형태: "문 …", "답 …", "돌아보기 …"
        plain = re.sub(r"<[^>]+>", "", inner).strip()
        for prefix, cls in DEVICES:
            if plain.startswith(prefix):
                rest = inner.replace(prefix, "", 1).lstrip()
                return (f'<div class="dev {cls}">'
                        f'<p class="dev-label">{prefix}</p><p>{rest}</p></div>')
        return mo.group(0)
    return re.sub(r"<div>((?:(?!<div>).)*?)</div>", repl, text, flags=re.S)

body = tag_devices(body)

parts = re.split(r'(<h2[^>]*>.*?</h2>)', body, flags=re.S)
front = parts[0]
chapters = []
for i in range(1, len(parts), 2):
    head = parts[i]; content = parts[i+1] if i+1 < len(parts) else ""
    # 제목은 순수 텍스트로 보관한다. 내보낸 HTML 에서 뽑은 것이라 이미
    # 이스케이프되어 있으므로 한 번 되돌린다 — 그러지 않으면 목차와 <title>
    # 에서 `<stdio.h>` 가 `&lt;stdio.h>` 로 이중 이스케이프된다.
    chapters.append((htmlmod.unescape(re.sub(r"<[^>]+>", "", head)).strip(), head + content))

CSS = """
:root { color-scheme: light dark; --fg:#141414; --bg:#ffffff; --muted:#5c5c5c;
  --rule:#d6d6d6; --box-bg:#f6f6f6; --box-rule:#9a9a9a; --code-bg:#f2f2f2; --link:#1a4f8a; }
@media (prefers-color-scheme: dark) { :root { --fg:#e8e8e8; --bg:#121314; --muted:#a6a6a6;
  --rule:#333; --box-bg:#1c1e20; --box-rule:#6d6d6d; --code-bg:#1b1d1f; --link:#8ab4e8; } }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--fg); line-height:1.75;
       font-family:"Noto Serif CJK KR","Noto Serif KR",Georgia,serif; }
.bar { position:sticky; top:0; z-index:20; display:flex; gap:.9rem; align-items:center;
       padding:.55rem clamp(.8rem,3vw,2.5rem); background:var(--bg);
       border-bottom:1px solid var(--rule); font-size:.86rem;
       font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.bar a { color:var(--link); text-decoration:none; } .bar a:hover { text-decoration:underline; }
.bar .sp { flex:1 1 auto; }
main { width:100%; padding:1.6rem clamp(.9rem,4vw,4rem) 5rem; }
.wrap { max-width:min(100%, 105ch); margin:0 auto; }
h1,h2,h3,h4 { font-family:"Noto Sans CJK KR",system-ui,sans-serif; line-height:1.35; }
h1 { font-size:clamp(1.5rem,3.4vw,2.1rem); margin:.4rem 0 1.4rem; }
h2 { font-size:clamp(1.35rem,3vw,1.8rem); margin:0 0 1.2rem; padding-bottom:.5rem;
     border-bottom:2px solid var(--fg); }
h3 { font-size:clamp(1.05rem,2.2vw,1.25rem); margin:2.2rem 0 .6rem; padding-left:.55rem;
     border-left:4px solid var(--fg); }
p { margin:.75rem 0; } a { color:var(--link); }
pre,code,kbd { font-family:"D2Coding",ui-monospace,SFMono-Regular,Menlo,monospace; }
code { background:var(--code-bg); padding:.08em .3em; border-radius:2px; font-size:.93em; }
pre { background:var(--code-bg); border:1px solid var(--rule); border-radius:4px;
      padding:.85rem 1rem; overflow-x:auto; line-height:1.55; font-size:.9rem; }
pre code { background:none; padding:0; }
table { border-collapse:collapse; margin:1.1rem 0; font-size:.92rem; width:100%;
        display:block; overflow-x:auto; }
th,td { border:1px solid var(--rule); padding:.42rem .6rem; text-align:left; vertical-align:top; }
th { background:var(--box-bg); font-weight:700; }
ul,ol { padding-left:1.4rem; } li { margin:.3rem 0; }
.dev { margin:1.1rem 0; padding:.7rem .95rem; background:var(--box-bg);
       border-left:3px solid var(--box-rule); border-radius:2px; }
.dev > p:first-child { margin-top:0; } .dev > p:last-child { margin-bottom:0; }
.dev-label { font-family:"Noto Sans CJK KR",system-ui,sans-serif; font-weight:700;
             font-size:.88rem; color:var(--muted); margin-bottom:.25rem; }
.organizer { border-left-style:double; border-left-width:6px; }
.deepqa { border-left-style:dashed; }
.qa-q { background:transparent; border-left:3px solid var(--fg); }
.qa-a { background:transparent; border-left:1px solid var(--rule); }
.misconception { border-left-width:5px; }
.antipattern { border-left-style:dotted; border-left-width:5px; }
.realcase { border-left-width:1px; border-top:1px solid var(--rule); border-bottom:1px solid var(--rule); }
.mathbox { font-size:.95rem; }
.recap { border:1px solid var(--rule); border-left:3px solid var(--box-rule); }
.platform { border-left-style:double; }
.note { border:1px solid var(--rule); background:var(--box-bg); padding:.65rem .9rem;
        margin:0 0 1.6rem; font-size:.9rem; }
.toc { columns:2 24rem; column-gap:2.4rem; }
.toc-part { break-inside:avoid; margin:1.1rem 0 .3rem; font-size:.95rem; letter-spacing:.02em;
            font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.toc-part:first-child { margin-top:0; }
.toc-group { break-inside:avoid; padding-left:.85rem; border-left:1px solid var(--rule); }
.toc a { display:block; padding:.28rem 0; text-decoration:none; border-bottom:1px solid var(--rule); }
.toc a:hover { text-decoration:underline; }
.nav { display:flex; gap:1rem; align-items:center; margin-top:3rem; padding-top:1rem;
       border-top:1px solid var(--rule); font-size:.9rem;
       font-family:"Noto Sans CJK KR",system-ui,sans-serif; }
.nav .sp { flex:1 1 auto; }
"""

def page(title, inner, prev=None, nxt=None, is_index=False):
    nav = ""
    if not is_index:
        left = f'<a href="{prev[0]}">← {STR["prev"]}</a>' if prev else "<span></span>"
        right = f'<a href="{nxt[0]}">{STR["next"]} →</a>' if nxt else "<span></span>"
        nav = (f'<div class="nav">{left}<span class="sp"></span>'
               f'<a href="index.html">{STR["top"]}</a><span class="sp"></span>{right}</div>')
    return f"""<!doctype html>
<html lang="{lang}">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{htmlmod.escape(title)}</title><style>{CSS}</style></head>
<body>
<div class="bar"><strong><a href="index.html">{STR['short']}</a></strong><span class="sp"></span>
<a href="{STR['other_href']}">{STR['other']}</a>
<a href="https://github.com/rubidus-api/proven_c_book">GitHub</a></div>
<main><div class="wrap">
{inner}
{nav}
</div></main></body></html>
"""

out_dir.mkdir(parents=True, exist_ok=True)
for f in out_dir.glob("*.html"): f.unlink()
names = []
for i, (t, _) in enumerate(chapters):
    mnum = re.match(r"\s*(\d+)\s", t)
    names.append(f"ch{int(mnum.group(1)):02d}.html" if mnum else f"sec{i+1:02d}.html")
# ── 목차를 *부 단위로* 묶는다 ──────────────────────────────────
# 부 구성의 단일 출처는 main.typ 이다 (여기에 목록을 베끼지 않는다).
def read_parts(lang):
    src = pathlib.Path(__file__).resolve().parent.parent / ("book" if lang == "ko" else "book-en") / "main.typ"
    out = []
    for line in src.read_text(encoding="utf-8").splitlines():
        m = re.match(r'\s*\("([^"]+)",\s*(?:none|"[^"]*"),\s*\(([\d,\s]+)\)\),', line)
        if m:
            nums = [int(x) for x in re.findall(r"\d+", m.group(2))]
            out.append((m.group(1), nums))
    return out

by_num = {}
for i, (t, _) in enumerate(chapters):
    mn = re.match(r"\s*(\d+)\s", t)
    if mn:
        by_num[int(mn.group(1))] = i

toc_parts = []
placed = set()
for part_title, nums in read_parts(lang):
    rows = []
    for n in nums:
        i = by_num.get(n)
        if i is None:
            continue
        placed.add(i)
        rows.append(f'<a href="{names[i]}">{htmlmod.escape(chapters[i][0])}</a>')
    if rows:
        toc_parts.append(f'<h4 class="toc-part">{htmlmod.escape(part_title)}</h4>'
                         f'<div class="toc-group">{"".join(rows)}</div>')
rest = [f'<a href="{names[i]}">{htmlmod.escape(chapters[i][0])}</a>'
        for i in range(len(chapters)) if i not in placed]
if rest:
    toc_parts.append(f'<h4 class="toc-part">{STR["extra"]}</h4>'
                     f'<div class="toc-group">{"".join(rest)}</div>')
toc = "".join(toc_parts)
index_inner = (f'<div class="note">{STR["note"]}</div><h1>{htmlmod.escape(STR["title"])}</h1>'
               f'<h3>{STR["toc"]}</h3><div class="toc">{toc}</div>'
               f'<hr style="border:none;border-top:1px solid var(--rule);margin:2.4rem 0">{front}')
(out_dir/"index.html").write_text(page(STR["title"], index_inner, is_index=True), encoding="utf-8")
for i,(title,content) in enumerate(chapters):
    prev = ("index.html", STR["toc"]) if i==0 else (names[i-1], "")
    nxt = (names[i+1], "") if i+1 < len(chapters) else None
    (out_dir/names[i]).write_text(page(f"{title} — {STR['short']}", content, prev, nxt), encoding="utf-8")
# 분할로 어긋난 내부 앵커(#loc-N)를 "파일#앵커" 로 고친다
anchor_home = {}
for f in out_dir.glob("*.html"):
    for aid in re.findall(r'id="([^"]+)"', f.read_text(encoding="utf-8")):
        anchor_home[aid] = f.name
for f in out_dir.glob("*.html"):
    txt = f.read_text(encoding="utf-8")
    def fix(mo):
        aid = mo.group(1)
        home = anchor_home.get(aid)
        if home is None:
            return mo.group(0)
        return f'href="#{aid}"' if home == f.name else f'href="{home}#{aid}"'
    new = re.sub(r'href="#([^"]+)"', fix, txt)
    if new != txt:
        f.write_text(new, encoding="utf-8")

total = sum(f.stat().st_size for f in out_dir.glob("*.html"))
print(f"wrap-html: {out_dir}/ — {len(chapters)} pages + index ({total//1024} KB)")
