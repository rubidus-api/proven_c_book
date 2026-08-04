# Tests

TDD documentation and executable test organization.

Use `test-index.md` as the compact catalog. Put detailed test cases under `cases/`. Put executable tests and test build files under `tests/`.

Build output rules: compiled test binaries go to `build/tests/`; release packaging staging to `build/dist/`; final distribution artifacts only to `dist/`. Do not compile test binaries into `tests/`.
