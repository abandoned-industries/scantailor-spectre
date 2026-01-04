# Permissions Reference

This file summarizes the command allowlist from `.claude/settings.local.json` so contributors can audit what the local Codex configuration permits.

## Scope

Permissions are scoped to `/Users/kazys/Developer/scantailor-weasel` and a curated set of shell commands and file paths.

## Allowed Command Patterns (Summary)

- Git read/inspect: `git ... log`, `git ... diff --stat`, `git ... show`, `git ... status`
- Git write: requires permission (moved to ask list)
- Build tools: `cmake`, `cmake --build`, `make`
- Packaging/signing: `codesign`, `ditto`, `xcrun notarytool`, `hdiutil`, `xattr`
- Misc tooling: `ls`, `cat`, `grep`, `find`, `pgrep`, `pkill`, `open`
- Network helpers: `WebSearch`, `WebFetch(domain:raw.githubusercontent.com)`

## Allowed File Targets (Examples)

- Core sources: `src/core/*`, `src/core/filters/*`, `src/core/ThumbnailPixmapCache.cpp`
- App sources: `src/app/*`
- Resources: `src/resources/*`
- Build/version: `version.h.in`, `CHANGELOG_SESSION.md`, `README.md`
- Build output: `build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre`

## Commands Requiring Permission

- Git write/delete operations (e.g., `git add`, `git commit`, `git push`, `git merge`, `git checkout`, `git rm`, `git reset`, `git stash`)
- File mutation utilities: `rm`, `mv`, `cp`

## Notes

- This file is a human-readable summary; the source of truth is `.claude/settings.local.json`.
- If you update the allowlist, update this summary accordingly.
