#import "../../book/lib.typ": *

= Index

Gathered here are only the places where this book *defines a concept or treats it head
on*. Places where a word merely passes by are not listed — the purpose of an index is
not to count every appearance but to tell you where to open the book.

#v(0.4cm)

#if sys.inputs.at("mode", default: "paged") == "html" [
  This edition (HTML) has no page numbers, so only the headwords are listed. The index
  with page numbers is in the PDF edition.

  #make-index(pages: false)
] else [
  #make-index()
]
