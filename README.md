# Proven C Book — proven 라이브러리에 기반한 모던 C 입문

*[English README](README-en.md)*

C를 처음 배우는 사람을 위한 한국어 책이다. 문법 목록이 아니라 *컴퓨터가
어떻게 생겼는지*에서 출발해, 오늘의 표준(C23)을 기본값으로 삼고, 마지막
부에서 [proven](https://github.com/rubidus-api) C 라이브러리의 매뉴얼로
이어진다.

- **현재 판**: v0.1.0 — **초안(draft)**
- 한국어: [PDF](dist/proven_c_book-v0.1.0.pdf) · [웹으로 읽기](https://rubidus-api.github.io/proven_c_book/ko/)
- English: [PDF](dist/proven_c_book-v0.1.0-en.pdf) · [Web](https://rubidus-api.github.io/proven_c_book/en/) — 번역 진행 중
- 묶음 내려받기: [ko zip](dist/proven_c_book-v0.1.0-ko.zip) · [en zip](dist/proven_c_book-v0.1.0-en.zip) · [전체 zip](dist/proven_c_book-v0.1.0-all.zip)
- 12부 58장 + 부록 A~E + 찾아보기, 약 570쪽
- 이 저장소가 최신본이다. 갱신 내역은 [CHANGELOG.md](CHANGELOG.md)에 있다.

## 이 책의 특징

- **읽기만 하면 된다.** 연습문제도, "직접 해 보라"도 없다. 본문 곳곳의
  문답으로 진행한다.
- **인쇄된 실행 결과는 전부 실제 출력이다.** 수록 예제 60여 종을 매 빌드마다
  컴파일·실행해 그 출력을 지면에 싣는다(GCC 14 기준, Clang으로 교차 검증).
- **오늘의 C.** C23을 기본값으로 쓰고, 옛 관행은 역사로만 다룬다.
- **AI를 보조 도구로 삼아 집필했다.** 구성·방침·수록 여부는 저자가 정하고
  검토했으며, 예제는 기계가 매번 실제로 돌려 검증한다.

## 빌드

```sh
TYPST=/path/to/typst FONT_PATH=/path/to/fonts scripts/build-book.sh     # 예제 검증 + 한국어 PDF
TYPST=... FONT_PATH=... scripts/build-book-en.sh                        # 영어판 PDF
TYPST=... FONT_PATH=... scripts/build-html.sh                           # HTML (docs/, GitHub Pages)
scripts/release.sh                                                      # dist/ 에 zip 릴리스
```

- Typst 0.15 이상, 본문 글꼴 Noto Serif/Sans CJK KR, 코드 글꼴 D2Coding.
- `scripts/verify-examples.sh` — 예제 전수 빌드·실행·출력 캡처(C23).

## 구성

```
book/       Typst 원고 — 한국어판 (chapters/, appendix/, front/, back/, parts/)
book-en/    Typst 원고 — 영어판(번역 진행 중)
docs/       GitHub Pages 가 서비스하는 HTML 판
examples/   본문에 실리는 예제 소스 — 전부 검증 대상
scripts/    빌드와 검증
vendor/     proven 라이브러리 스냅샷 (예제 링크용)
dist/       배포 PDF
```

## 라이선스

- **본문**(`book/`, 생성된 PDF): **CC BY-NC-SA 4.0** — 저장소의
  [LICENSE](LICENSE)가 정본이다. 출처를 밝히면 공유·개작할 수 있으나
  영리 이용은 불가하며, 개작물에도 같은 라이선스를 적용해야 한다.
- **예제 코드**(`examples/`, `scripts/`): **MIT** — [LICENSE-CODE](LICENSE-CODE).
  책에서 배운 코드는 제약 없이 가져다 쓸 수 있다.
- `vendor/proven/`: 원저작물의 라이선스를 따른다.

자세한 안내는 [LICENSE-NOTICE.md](LICENSE-NOTICE.md)에 있다.

## 기여 — 오류 신고만 받는다

틀린 서술, 동작하지 않는 예제, 오탈자, 낡은 정보를 이슈로 알려 주시면
고맙게 반영한다. 새 장·절 원고나 구성 변경 제안은 받지 않는다. 자세한
내용은 [CONTRIBUTING.md](CONTRIBUTING.md).

연락은 rubidus@gmail.com.
