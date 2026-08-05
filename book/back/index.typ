#import "../lib.typ": *

= 찾아보기

이 책이 개념을 *정의하거나 정면으로 다루는* 자리만 모았다. 낱말이 스쳐
지나가는 자리는 싣지 않았다 — 찾아보기의 목적은 모든 등장을 세는 것이
아니라 "어디를 펴면 되는지"를 알려 주는 것이기 때문이다.

#v(0.4cm)

#if sys.inputs.at("mode", default: "paged") == "html" [
  이 판(HTML)에는 쪽 번호가 없으므로 표제어만 싣는다. 쪽 번호가 붙은
  찾아보기는 PDF 판에 있다.

  #make-index(pages: false)
] else [
  #make-index()
]
