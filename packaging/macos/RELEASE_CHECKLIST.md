# 2.0b7 release verification

## Candidate and documentation-only repack

- [x] Native arm64 build targeting macOS 15; candidate CTest: 12/12 suites.
- [x] Candidate manually tested and accepted by the user on macOS 27.
- [x] Reviewed README leads with the more modern interface; distribution PDF
  regenerated with Pandoc HTML and Chrome, inspected as text and rendered pages.
- [x] Both app resources (`README.pdf` and `ScanTailor Spectre Readme.pdf`) and
  the DMG-root PDF are byte-identical to the public README PDF.
- [x] All 56 Mach-O payloads preserved from the tested candidate. The main
  executable's enclosing code signature changed; its unsigned payload matches.
  All nested libraries, plugins and helper signatures remain unchanged.
- [x] Updated app signed, separately notarized, stapled and Gatekeeper accepted.
  App submission: `d3ccb8d3-9b59-4906-ba49-976ee99f5b75` (Accepted).
- [x] Timestamp-matched final ZIP and DMG made from the stapled app.
- [x] DMG signed, notarized and stapled; signature, ticket and Gatekeeper passed.
  DMG submission: `90d79d0e-5084-41ba-a21d-3d5155781058` (Accepted).
- [x] Read-only mounted DMG and extracted ZIP: matching 56-Mach-O hashes,
  arm64-only minima at most 15.0, no unresolved/external loads or rpaths,
  valid contained symlinks, signatures, tickets and documentation verified.
- [x] Mounted package launched through native Cocoa with isolated preferences;
  live event loop sampled, then only the test process terminated.

The functional acceptance is the user's report for the tested candidate, not
an independent exhaustive import/process/export retest of the documentation-only
repack. No existing user documents were opened or closed by package verification.
The isolated release build's 12/12 suites were not rerun for documentation edits.
The public snapshot omits private image fixtures; their tests announce skips.
The CMake `scantailor_bundle` target remains unexercised end-to-end.
macOS 15 runtime testing was unavailable and waived, not represented as passed.

## Distribution

Artifact basename: `ScanTailor-Spectre-2.0b7-20261021-1743`.

- DMG SHA-256: `592d7241daa91a52ee41858614435084dc6d01eb7fa60a1b0a14f5b0f03e674f`
- ZIP SHA-256: `f6c6f06494b764847897e65ea2760902dddc061b01ee08d8d88be73306552bbc`

The publication procedure requires a freshly fetched public-main sole parent,
a sanitized snapshot (never private ancestry), and a new public version tag.
Only the final DMG, ZIP and checksum file are uploaded. Completion is recorded
by the public release and its assets after fresh-download hash, mounted-package,
signature, ticket, Gatekeeper, dependency, documentation and launch checks.
