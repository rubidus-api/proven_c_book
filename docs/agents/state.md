# Project State

This file is the single source of truth for root file roles and update rules, the required directory layout, and the bootstrap profiles. `AGENTS.md` routes here; do not duplicate this content in other documents.

Current accepted behavior lives in `SPEC.md`. Current accepted requirements live in `REQUIREMENTS.md`. Read history files only when history, conflict analysis, or prior work context is relevant.

## Root File Operations

| File | Role | Update when | Do not use for |
|---|---|---|---|
| `README.md` | Public GitHub-facing introduction page. | Public purpose, install, usage, public API, structure, license, or visible behavior changes. | Private notes, local paths, run logs, secrets. |
| `CHANGELOG.md` | Public release history following Keep a Changelog. | A user-visible change ships. Keep `Unreleased` current. | Private work traces, raw task logs, secrets. |
| `AGENTS.md` | Permanent agent router and operating rules. Read first. | Routing, rules, or context policy change. | Task notes or history. |
| `CLAUDE.md` | One line, `@AGENTS.md`: makes Claude Code load the same rules. | Never; it has no content of its own. | Anything else. |
| `CONTEXT.md` | Current state, blockers, next step, and the `## Resume Packet`. Read by default; kept inside the context budget. | State or next step changes, a blocker appears, or work is interrupted or handed off. | Requirements, history, test details, session narration. |
| `MEMORY.md` | Stable facts, preferences, reusable patterns (standard profile; light keeps a `## Facts` section in `LESSONS.md`). | A durable fact or preference is discovered. | Decisions, work logs, requirement history, plans. |
| `SPEC.md` | Current accepted behavior, architecture, design, and operating contract. Read by section, on trigger. | Accepted behavior or structure changes. | Discussion history, rejected alternatives, chronology. |
| `REQUIREMENTS.md` | Current accepted requirements, constraints, and success criteria. | A requirement or criterion changes. | Chronology, contradictions, rationale, implementation notes. |
| `DECISIONS.md` | One append-only accepted decision log. | A lasting choice is accepted or superseded. | Secrets, private-only decisions, facts, lessons, backlogs. |
| `LESSONS.md` | Lessons, severe bugs, hazards, prohibited actions, and the `## Checks` section of short repeatable checks (there is no separate `CHECKLIST.md`). | A hazard or a repeatable check is learned. | General memory (standard profile), decisions, release notes. |
| `BACKLOGS.md` | Compact backlog and active-work queue. | Actionable work appears, moves, or changes priority. | Decisions, requirements, wishlists. |
| `docs/tests/test-index.md` | Compact authoritative TDD catalog. | A test is added, changed, or retired. | Procedures, fixtures, logs, red/green notes. |

The work log is `git log` (`scripts/worklog.sh`). Kits before 0.9.0 created `WORKLOG.md`; an existing one may stay as cold history but is not updated. Legacy files from older kit versions (`TODO.md`, `HANDOFF.md`, `TESTS.md`, `CHECKLIST.md`) are migration input only; `boot/MIGRATION_PROMPT.md` says where each merges.

Operating docs may live in the sibling private directory `../<project>_private/` when the project was bootstrapped with `Operating docs location: private-sibling`; the `## Operating Docs Location` section of `AGENTS.md` records this. All roles and rules here apply wherever the files live.

## Structure

Standard profile:

```text
README.md  CHANGELOG.md  AGENTS.md  CLAUDE.md  CONTEXT.md  MEMORY.md  SPEC.md
REQUIREMENTS.md  DECISIONS.md  LESSONS.md  BACKLOGS.md
docs/agents/  docs/manual/  docs/rfc/  docs/research/  docs/benchmark/results/
docs/backlogs/items/  docs/plans/active/  docs/plans/archive/  docs/requirements/
docs/resources/  docs/tests/cases/
resources/distribution/  resources/prep/  resources/cache/
build/tests/  build/dist/  tests/  dist/  scripts/
```

Light profile creates `docs/agents/`, `docs/plans/`, `docs/tests/`, `scripts/`, omits `MEMORY.md` (facts go to the `## Facts` section of `LESSONS.md`), and creates any other standard directory on demand.

Durable, shareable docs are tracked by default: `README.md`, `CHANGELOG.md`, `DECISIONS.md`, `docs/`. Working operating docs (`AGENTS.md`, `CLAUDE.md`, `CONTEXT.md`, `MEMORY.md`, `SPEC.md`, `REQUIREMENTS.md`, `LESSONS.md`, `BACKLOGS.md`) are local unless the owner selected tracked operating docs at bootstrap.

## Backlog Operations

`BACKLOGS.md` is the compact current backlog and active-work queue, with the sections `## Active Focus`, `## Ready`, `## Later`, `## Blocked`. Keep each item to one actionable line. When an item needs acceptance criteria or investigation notes, create a detail file under `docs/backlogs/items/` and add a one-line pointer in `docs/backlogs/backlog-index.md`.

Do not use `TODO.md` for new work.

## Resume Packet Operations

`CONTEXT.md` carries a `## Resume Packet` section as the current resume state. There is no separate handoff file. Update it before pausing with unfinished work, handing over to another agent or model, or stopping after a blocker:

```text
## Resume Packet
- Goal:
- Changed files:
- Last verification:
- Next actions:
- Blockers:
```

Clear or trim it when the work completes. It is a checkpoint, not a log.

## Decision Log

`DECISIONS.md` is the one append-only accepted decision log for non-secret decisions. Entry format:

```text
## YYYY-MM-DD: <decision title>

- Status: Accepted | Superseded
- Context:
- Decision:
- Consequences:
- Supersedes:
```

Do not split decisions into current and old files; a superseding entry references the older one. Put non-public decisions in the sibling private repository and keep only a sanitized pointer here when needed.

## Public Changelog

`CHANGELOG.md` is the public release history. It follows Keep a Changelog: `## [Unreleased]` at the top, release headings like `## [1.2.3] - YYYY-MM-DD`, and only the sections `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`. `scripts/changelog-entry.sh <Section> <message>` adds a line. Do not use `CHANGELOG.md` as a private work log.
