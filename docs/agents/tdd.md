# TDD

Development is always TDD.

Before implementation:

1. Identify the requirement and acceptance criteria.
2. Decide how the requirement will be verified.
3. Add or update `docs/tests/test-index.md`.
4. Add or update relevant detail files under `docs/tests/cases/`.
5. Add or update executable tests under `tests/`.
6. Run the relevant test and confirm it fails for missing behavior when practical.

Use:

- `docs/tests/test-index.md` as the compact authoritative TDD catalog.
- `docs/tests/cases/` for detailed test intent, setup, steps, assertions, fixtures, and edge cases.
- `CONTEXT.md` (current state or resume packet) for the active task's red/green/refactor status.
- `tests/` for executable test source, fixtures, runners, and test build files.
- `build/tests/` for compiled test binaries and test-generated artifacts.

`docs/tests/test-index.md` entries should stay compact:

```text
| ID | Requirement | Purpose | Command | Detail | Status |
```

Update the index before or with the test detail file whenever behavior changes. Do not put long logs, detailed assertions, or temporary red/green notes in the index.
