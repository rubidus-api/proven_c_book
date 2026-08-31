# Project Scripts

Reusable local automation for mechanical project work.

These are POSIX shell scripts targeting Linux only. Windows is out of scope for now.

Scripts must:

- avoid secrets and local absolute paths in output;
- check required tools before doing work;
- print a clear user action when a required tool is missing;
- keep generated output under `build/`, `docs/plans/`, or other declared project paths;
- avoid network access unless the user explicitly requests it.

## Releasing

The order matters, and the last step is the one that has actually been missed
twice — see `scripts/pages-build.sh` for that story.

1. Bump `#let book-version` in `book/main.typ` and `book-en/main.typ`, then
   `python3 scripts/sync-version.py` (it carries the number into `VERSION.md`
   and both READMEs).
2. Add the entry at the top of `CHANGELOG.md`.
3. `sh scripts/build-book.sh`, `sh scripts/build-book-en.sh`,
   `sh scripts/build-html.sh`. Mark any translation-sync warnings with
   `python3 scripts/sync-status.py --mark <files>` once you have checked the
   English side, then `--write`.
4. `python3 scripts/sync-metadata.py` — measures the built PDFs and the manuscript
   and carries page counts, the example count and the last-changed date into the
   READMEs and both `main.typ`. If it changes anything, build again (step 3) so the
   colophon carries the new date. `release.sh` checks these numbers and refuses to
   package when they disagree with the built PDFs.
5. `sh scripts/release.sh` — it runs every gate and refuses to package when one
   fails, then writes the three zips into `dist/`. It also cross-verifies every
   example with clang when clang is present; what that run skips, and why, is in
   `docs/example-cross-skip.tsv`.
6. Copy the two PDFs into `dist/` beside the zips.
7. Commit, tag `vX.Y.Z`, push with `--follow-tags`.
8. Create the GitHub release and upload the seven assets.
9. **`sh scripts/pages-build.sh`** — ask GitHub Pages to rebuild, wait for it,
   and confirm the *live* web edition serves the new version. Pushing alone has
   twice failed to trigger a build, leaving the site two releases behind while
   every file in the repository was correct.
10. In the landing-site repository: `scripts/build-site.sh` then
   `scripts/publish.sh`. Its `check-fresh.sh` refuses to publish links that
   point at anything but the newest release.

Steps 1–6 are local and reversible. From step 7 the work is public.
