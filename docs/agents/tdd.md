# TDD

Development is test-first. Before changing behavior:

1. Name the requirement and how it will be verified.
2. Add or change the executable test under `tests/` and run it; confirm it fails for the missing behavior when practical.
3. Implement, run the test again, then run `scripts/gate.sh`.
4. Add or update the row in `docs/tests/test-index.md`.

## Where Things Live

- `docs/tests/test-index.md`: the compact authoritative TDD catalog. One row per test.
- `tests/`: executable tests, fixtures, runners, and test build files.
- `build/tests/`: compiled test binaries and test-generated artifacts. Never compile into `tests/`.
- `docs/tests/cases/`: detail files, **optional**. Write one only when the executable test cannot state the contract by itself: a manual procedure, a hardware or device check, a long fixture explanation, a case the owner must be able to read without the code. A test whose source reads as its own specification needs no case file; put `-` in the Detail column.
- `CONTEXT.md` (current state or resume packet): the red/green state of the task in progress, if the session may be interrupted.

## The Index

`docs/tests/test-index.md` entries should stay compact:

```text
| ID | Requirement | Purpose | Command | Detail | Status |
```

Do not put long logs, detailed assertions, or temporary red/green notes in the index. Update it with the test, not instead of the test.
