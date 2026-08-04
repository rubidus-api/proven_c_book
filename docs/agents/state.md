# Project State

Current accepted behavior lives in `SPEC.md`. Current accepted requirements live in `REQUIREMENTS.md`.

This file is the single source of truth for root file roles and update rules. `AGENTS.md` routes here; do not duplicate this table in other documents.

Read history files only when history, conflict analysis, or prior work context is relevant.

## Root File Operations

| File | Role | Update when | Do not use for |
|---|---|---|---|
| `README.md` | Public GitHub-facing introduction page. | Public purpose, install, usage, public API, project structure, license, or visible behavior changes. | Private notes, local paths, internal run logs, secrets. |
| `CHANGELOG.md` | Public release history following Keep a Changelog. | Public release entries or notable user-visible changes need recording. Keep `Unreleased` current. | Private/local work traces, raw task logs, internal-only implementation notes, secrets. |
| `AGENTS.md` | Permanent agent router and operating rules. Read first. | Agent routing, operating rules, required structure, or default context policy changes. | Task notes or project history. |
| `CONTEXT.md` | Current state, blockers, next steps, and the resume packet for session transitions. Read by default. | Bootstrap completion, significant work, blockers, interruption or handoff, or next-step changes. | Accepted requirements, long history, detailed test cases. |
| `MEMORY.md` | Stable facts, preferences, reusable patterns, and durable project knowledge (standard profile; light profile keeps a `## Facts` section in `LESSONS.md`). | Stable reusable facts, user preferences, durable non-decision knowledge, or project patterns are discovered. | Accepted decisions, chronological work logs, requirement history, temporary plans. |
| `SPEC.md` | Current accepted behavior, architecture, design, and operating contract. | Accepted behavior, architecture, workflow contract, design, public behavior, or project structure changes. | Discussion history, rejected alternatives, unaccepted ideas, chronological requirement changes. |
| `REQUIREMENTS.md` | Current accepted user requirements, constraints, and success criteria. | Current accepted requirements, constraints, acceptance criteria, or success criteria change. | Chronological requirement changes, contradictions, design rationale, implementation notes. |
| `DECISIONS.md` | One append-only accepted decision log. | A stable design/workflow choice is accepted or superseded. | Secrets, private-only decisions, routine facts, lessons, backlogs. |
| `LESSONS.md` | Lessons, severe bugs, hazards, and prohibited actions, plus the `## Checks` section of short repeatable verification steps (both profiles; there is no separate `CHECKLIST.md`). Search before risky changes, bug fixes, releases, or repeated failures. | A severe bug, hazard, prohibited action, or reusable lesson is found, or a short repeatable check is promoted from current risk. | General memory (standard profile), accepted decisions, public release notes. |
| `BACKLOGS.md` | Compact backlog and active-work queue. | New actionable pending work appears, priorities change, or work moves between active/ready/later. | Decisions, requirements, accepted truth, or broad unactionable wishlists. |
| `WORKLOG.md` | Private/local AI work history. | Notable local AI work completes or a useful private work trace is needed. | Public changelog entries or current state. |
| `docs/tests/test-index.md` | Compact authoritative TDD catalog. | A requirement needs verification, changes, or gains regression coverage; a command, detail file, or status changes. | Full test procedures, long fixtures, logs, red/green/refactor notes. |

Legacy files from older kit versions are migration input only: merge `TODO.md` into `BACKLOGS.md`, `HANDOFF.md`/`TESTS.md` into `CONTEXT.md`, and `CHECKLIST.md` into the `## Checks` section of `LESSONS.md`, then stop updating the legacy file.

Operating docs may live in the sibling private directory `../<project>_private/` when the project was bootstrapped with `Operating docs location: private-sibling`; the `## Operating Docs Location` section of `AGENTS.md` records this. All roles and rules in this file apply to the files wherever they live.

## Backlog Operations

Use `BACKLOGS.md` as the compact current backlog and active-work queue.

Recommended sections:

```text
## Active Focus
## Ready
## Later
## Blocked
```

Keep each item short and actionable. If an item needs acceptance criteria, dependencies, investigation notes, or a multi-step implementation plan, create a detail file under `docs/backlogs/items/` and add a one-line pointer in `docs/backlogs/backlog-index.md`.

Do not use `TODO.md` for new work. If an older project has `TODO.md`, migrate actionable entries to `BACKLOGS.md` and keep `TODO.md` only as a temporary legacy note until it is removed or ignored.

## Resume Packet Operations

`CONTEXT.md` carries a `## Resume Packet` section as the current resume state. There is no separate handoff file.

Update it before:

- pausing with unfinished work;
- handing the project to another agent or model;
- leaving a long-running goal active;
- stopping after a blocker;
- finishing a substantial step where the next action is non-obvious.

Keep it compact:

```text
## Resume Packet
- Goal:
- Changed files:
- Last verification:
- Next actions:
- Blockers:
```

Clear or trim the section when the work completes. Do not use it as a permanent work log. Move durable work history to `WORKLOG.md`, accepted behavior to `SPEC.md`, accepted requirements to `REQUIREMENTS.md`, and accepted decisions to `DECISIONS.md`.

## Test Index Operations

`docs/tests/test-index.md` is the compact authoritative TDD catalog. Read it before implementation or behavior changes.

Update it when:

- a new requirement needs verification;
- an existing requirement changes;
- a test command, detail case, or status changes;
- a bug fix adds regression coverage.

Each entry should identify the test id, requirement, purpose, command, detail file, and status.

Do not put full test procedures, long fixtures, logs, or red/green/refactor notes in `docs/tests/test-index.md`. Put detailed cases under `docs/tests/cases/` and executable tests under `tests/`, with compiled test artifacts under `build/tests/`. Track the active red/green/refactor state of the current task in `CONTEXT.md` (current state or resume packet), not in a separate root file.

## Decision Log

`DECISIONS.md` is the one append-only accepted decision log for non-secret project decisions.

Use it when:

- the user accepts a design or workflow choice that should remain stable;
- a prior decision affects the current task;
- a decision is superseded and the replacement must be traceable.

Entry format:

```text
## YYYY-MM-DD: <decision title>

- Status: Accepted | Superseded
- Context:
- Decision:
- Consequences:
- Supersedes:
```

Do not split decisions into current and old files. Keep superseded decisions in the same file and append a newer entry that references the older one.

Do not store secrets, private infrastructure details, personal data, private remote URLs, or private-only business context in `DECISIONS.md`. Put non-public decisions in the sibling private repository when one is used, and keep only a sanitized public decision or pointer in this project when needed.

## Public Changelog

`CHANGELOG.md` is the public release history. It follows Keep a Changelog and must stay safe for a public repository page.

Use:

- `## [Unreleased]` at the top.
- release headings like `## [1.2.3] - YYYY-MM-DD`.
- only these change sections: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`.

Use `scripts/changelog-entry.sh <Section> <message>` for simple entries.

Do not use `CHANGELOG.md` as a private work log. Put local AI work history in `WORKLOG.md`.

If the user asks to refresh the changelog rules, fetch `https://keepachangelog.com/` and update this local guidance before changing changelog behavior.
