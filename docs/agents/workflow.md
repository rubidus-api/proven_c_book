# Workflow

## Active Planning

For every non-trivial request:

1. Read the narrowest useful context.
2. Draft `docs/plans/active/YYYY-MM-DD-<short-task-id>.md`.
3. Ask the user for confirmation or missing decisions.
4. Implement only after confirmation unless explicitly told to proceed.
5. Move completed, cancelled, or superseded plans to `docs/plans/archive/`.

The active plan must include request, acceptance criteria, TDD, expected file changes, token budget, stop conditions, user questions, and small steps.

## Token Budget

Start from `AGENTS.md`, `CONTEXT.md`, `SPEC.md`, `REQUIREMENTS.md`, and `docs/tests/test-index.md`.

For task selection or grooming, also read `BACKLOGS.md`. Continuation and session recovery need no extra file: the resume packet is the `## Resume Packet` section of `CONTEXT.md`.

Read only triggered modules and relevant detail files. Do not browse archives, detailed backlog items, handoff archives, or broad history by default.

## Local Automation

Prefer project scripts for repeated, mechanical, or easily verified work:

- `scripts/check-tools.sh`: check required local tools and print user action if something is missing.
- `scripts/project-check.sh`: run narrow local consistency checks before reporting or committing.
- `scripts/new-plan.sh <short-task-id>`: create an active plan template.
- `scripts/archive-plan.sh docs/plans/active/<plan>.md`: move a completed plan to archive.
- `scripts/changelog-entry.sh Added|Changed|Deprecated|Removed|Fixed|Security <message>`: add a public changelog entry.

These are POSIX shell scripts targeting Linux only. Windows is out of scope for now. If a required tool or shell is missing, stop and ask the user to install it or approve an alternative.

Do not spend tokens manually reproducing a deterministic script result when an existing script can run locally and safely.

## Backlogs And Continuation

Use `BACKLOGS.md` for actionable future work and active focus. Keep root entries short. Move detailed task specifications to `docs/backlogs/items/` and list them in `docs/backlogs/backlog-index.md` only when the root backlog would become too large.

Use the `## Resume Packet` section of `CONTEXT.md` for the current resume state. Update it before pausing, switching sessions or models, leaving an unfinished goal, or stopping after a blocker; clear or trim it when the work completes.

Do not create new `TODO.md`, `HANDOFF.md`, or `TESTS.md` files. Older projects may keep them only as temporary migration input: merge `TODO.md` into `BACKLOGS.md`, and `HANDOFF.md`/`TESTS.md` into `CONTEXT.md`.

## Stop Conditions

Stop and ask when:

- the same check fails three times for the same reason;
- the next action cannot produce new evidence or changed output;
- the task grows beyond the active plan;
- requirements conflict or destructive ambiguity appears.
