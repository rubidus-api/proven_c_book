# Project Scripts

Reusable local automation for mechanical project work.

These are POSIX shell scripts targeting Linux only. Windows is out of scope for now.

Scripts must:

- avoid secrets and local absolute paths in output;
- check required tools before doing work;
- print a clear user action when a required tool is missing;
- keep generated output under `build/`, `docs/plans/`, or other declared project paths;
- avoid network access unless the user explicitly requests it.
