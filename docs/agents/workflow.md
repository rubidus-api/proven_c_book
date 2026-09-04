# Workflow

## Planning By Tier

`AGENTS.md` defines the tiers. Only two of them produce a plan:

- **T2**: five lines in the reply (goal, files, verification, risks, open questions). No file.
- **T3**: `scripts/new-plan.sh <short-task-id>` creates `docs/plans/active/YYYY-MM-DD-<id>.md`; fill it, ask the owner, edit only after confirmation. When the work is complete, cancelled, or superseded, `scripts/archive-plan.sh <plan>` moves it to `docs/plans/archive/`.

A plan states what will change and how it will be verified. It is not a design document and it does not restate the request. Do not write a plan to satisfy the process; write it when the owner needs to see the shape of the work before it happens.

## Session Start

```sh
scripts/brief.sh            # resume packet, git status, active focus, open plans
scripts/context-budget.sh   # is the default context still within budget?
```

Then read only what the task triggers (table in `AGENTS.md`). Do not re-read a file already in context.

## Local Scripts

Mechanical work goes through `scripts/`, not through tokens:

| Script | Use |
|---|---|
| `scripts/gate.sh` | run the minimum verification tier for what changed (`--explain` shows the choice); raise a tier explicitly, never lower it |
| `scripts/project-check.sh` | the full local check; `gate.sh` calls it at the top tier |
| `scripts/context-budget.sh` | measure the default context and fail over budget |
| `scripts/brief.sh` | one-call session start |
| `scripts/worklog.sh [n]` | work history from `git log`; replaces a hand-written work log |
| `scripts/new-plan.sh`, `scripts/archive-plan.sh` | T3 plans |
| `scripts/changelog-entry.sh <Section> <message>` | a public changelog line |
| `scripts/check-tools.sh` | required local tools |

These are POSIX shell scripts for Linux. If a required tool is missing, stop and ask the user to install it or approve an alternative. Never reproduce by hand what a script prints.

## State Updates

Update a state file only when its content changed:

- `CONTEXT.md`: the current state or the next step changed, or the work is paused (`## Resume Packet`).
- `BACKLOGS.md`: new actionable work appeared or priorities moved.
- `CHANGELOG.md`: a user-visible change shipped.
- `DECISIONS.md`: the owner accepted a lasting choice.
- `LESSONS.md`: a hazard or a repeatable check was learned.
- `docs/tests/test-index.md`: a test was added, changed, or retired.

A routine T0/T1 change touches none of them. Do not narrate the session into any file; `git log` already holds it.

## Stop Conditions

Stop and ask when:

- the same check fails three times for the same reason;
- the next action cannot produce new evidence or changed output;
- the task grows past its tier (a T1 fix turns out to need a schema change);
- requirements conflict or a destructive step is ambiguous.
