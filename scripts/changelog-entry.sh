#!/bin/sh
set -eu

section="${1:-}"
shift || true
message="$*"

case "$section" in
  Added|Changed|Deprecated|Removed|Fixed|Security) ;;
  *)
    printf '%s\n' "usage: scripts/changelog-entry.sh Added|Changed|Deprecated|Removed|Fixed|Security <message>" >&2
    exit 1
    ;;
esac

if [ -z "$message" ]; then
  printf '%s\n' "message is required" >&2
  exit 1
fi

test -f CHANGELOG.md || cat > CHANGELOG.md <<'EOF'
# Changelog

All notable changes to this project will be documented in this file.

This project follows Keep a Changelog.

## [Unreleased]

### Added

### Changed

### Deprecated

### Removed

### Fixed

### Security
EOF

tmp="${TMPDIR:-/tmp}/changelog-entry.$$"
awk -v section="$section" -v message="$message" '
  BEGIN { inserted = 0; in_unreleased = 0; in_section = 0 }
  /^## \[Unreleased\]/ { in_unreleased = 1 }
  in_unreleased && $0 == "### " section { in_section = 1; print; next }
  in_section && /^### / && !inserted {
    print "- " message
    inserted = 1
    in_section = 0
  }
  in_section && /^## / && !inserted {
    print "- " message
    inserted = 1
    in_section = 0
  }
  { print }
  END {
    if (!inserted) {
      print "- " message
    }
  }
' CHANGELOG.md > "$tmp"
mv "$tmp" CHANGELOG.md

printf '%s\n' "changelog-entry: ok"
