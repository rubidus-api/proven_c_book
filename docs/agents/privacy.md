# Privacy And Secrets

Never commit, push, print, log, document, or paste:

- credentials, tokens, API keys, SSH keys, cookies, or `*.pem` files;
- personal information, private accounts, or private email addresses;
- local absolute paths;
- AI server names, build server names, hostnames, IPs, ports, usernames, SSH key names, mount names, share names, private remote URLs, or token filenames.

Keep sensitive files in a dedicated ignored directory rather than relying on
broad filename rules. `.gitignore` ignores these by convention:

```text
*_private/   e.g. app_private/ — local private material kept beside the project
*_internal/
secrets/     keys, tokens, certificates, credentials
.env         (commit a .env.example template instead)
```

This lets the rest of the project stay fully git-tracked with no exposure risk,
without enumerating secret filenames. Do not add broad globs such as `*token*`
or `*.key`; they also hide normal source files (a tokenizer, a Keynote `.key`).

For private knowledge you also want versioned, use a sibling private git
repository under the project parent:

```text
../<project-name>_private/
```

Raw secrets should remain encrypted or untracked even there, unless the user explicitly accepts the risk.

Use project `DECISIONS.md` only for non-secret accepted decisions. Put private decisions, private requirements, incidents, non-public runbooks, and private research notes in the sibling private repository when they need versioned backup.

Before git operations or public output, run `git status`, review `git diff`, and scan for private data.

## Commit Identity

Every commit — in every repository, public, private, or local-only — must be authored **and**
committed as the owner's single configured git identity.

- Use the identity from the global git config. Never override `user.name` / `user.email` per commit
  or per repository, and never invent a per-project author name.
- Never add `Co-Authored-By:` trailers (AI assistants included). No second name may appear in the
  history or in the hosting service's contributor list.
- Before the first push of a new repository, check
  `git log --pretty='%an <%ae> | %cn <%ce>'`. If a wrong identity slipped in, rewrite the history
  before pushing.
- If a wrong identity was already pushed, ask the owner before force-pushing, then rewrite with
  `git filter-branch --env-filter` (or `git-filter-repo`) and force-push.

An identity is also personal data: keep the concrete name and address in the owner's local
configuration, not in project documentation.

Apply the same privacy rules to `BACKLOGS.md`, the `CONTEXT.md` resume packet, detailed backlog items, and migration notes. These files often capture recent work and must not contain secrets, local absolute paths, private host details, token filenames, or private account data.

## Public Repository Surface

A public repository publishes exactly what its users need, and nothing else. Decide that surface
deliberately, and enforce it with `.gitignore` rather than with care.

Typically public:

- the sources, resources and build scripts needed to build the product;
- the README, and the images it embeds;
- the license.

Typically not public: working notes and operating docs (SPEC, REQUIREMENTS, CONTEXT, MEMORY,
BACKLOGS, LESSONS, WORKLOG, and legacy CHECKLIST/HANDOFF/TESTS files), plans, agent
instructions, internal scripts, private material, build output, and release artifacts (publish
those on a releases page instead of committing them).

Prefer an allowlist `.gitignore`: ignore everything, then re-allow the public surface. A blocklist
has to be extended for every new kind of file, and the failure mode is publication; an allowlist
fails the safe way, by keeping a new file local until someone allows it on purpose.

```gitignore
/*
!/.gitignore
!/README.md
!/LICENSE
!/src/
!/scripts/
/scripts/*
!/scripts/build.sh
```

After changing the surface, verify it: `git ls-files` must list only what should be public, and a
fresh clone of the repository must still build the product. Files that stop being tracked stay on
disk; `git rm --cached` removes them from the index without deleting local work.
