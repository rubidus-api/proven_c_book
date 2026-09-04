# Build And Distribution

Use explicit output paths:

```text
build/
  generated build output and intermediate files

build/tests/
  compiled test binaries, test objects, coverage files, and test-generated artifacts

build/dist/
  release packaging staging area

dist/
  final distribution artifacts only

tests/
  test sources, fixtures, runner source, and test build scripts
```

Do not compile test binaries into `tests/`.

Do not place intermediate build files in `dist/`.

Build commands should accept configurable output paths when practical:

```text
BUILD_DIR=build
TEST_BUILD_DIR=build/tests
DIST_DIR=dist
```
