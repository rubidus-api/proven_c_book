"""장 등록부를 읽는 공용 모듈 (RFC-0028).

원고가 장을 이름으로 가리키게 된 뒤(`#chref("loops")`), 원고를 *분석하는*
스크립트들은 그 이름을 다시 번호로 펴야 한다. 그 일을 한 곳에 모아 둔다.

    from chapters import NO, expand
    text = expand(path.read_text(encoding="utf-8"), lang="ko")
    # 이제 본문에 "32장" 이 들어 있는 것처럼 보인다 --- 옛 정규식이 그대로 통한다
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
REGISTRY = ROOT / "book" / "registry.typ"

_text = REGISTRY.read_text(encoding="utf-8")
_ids = []
for _m in re.finditer(r"chapters:\s*\(([^)]*)\)", _text):
    _ids += re.findall(r'"([^"]+)"', _m.group(1))

#: 장 id → 장 번호
NO = {cid: i + 1 for i, cid in enumerate(_ids)}
#: 읽기 순서대로의 장 id
IDS = list(_ids)

_REF = re.compile(r'#(chref|chrefs|chrange)\(([^()]*)\)(?:/\*\*/)?')


def _nums(args):
    return [NO[i] for i in re.findall(r'"([^"]+)"', args) if i in NO]


def expand(text, lang="ko"):
    """`#chref("loops")` 를 `32장` / `chapter 32` 로 편다 (분석용)."""
    def one(m):
        kind, args = m.group(1), m.group(2)
        ns = _nums(args)
        if not ns:
            return m.group(0)
        if lang == "en":
            head = "Chapter" if "cap: true" in args else "chapter"
            if kind == "chref":
                return f"{head} {ns[0]}"
            if kind == "chrange":
                return f"{head}s {ns[0]}–{ns[-1]}"
            return f"{head}s " + (", ".join(str(n) for n in ns[:-1]) + " and " + str(ns[-1])
                                  if len(ns) > 1 else str(ns[0]))
        if kind == "chref":
            return f"{ns[0]}장"
        if kind == "chrange":
            return f"{ns[0]}–{ns[-1]}장"
        return "·".join(str(n) for n in ns) + "장"
    return _REF.sub(one, text)


def lang_of(path):
    return "en" if "book-en" in str(path) else "ko"
