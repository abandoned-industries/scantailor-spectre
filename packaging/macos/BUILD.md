# Building ScanTailor Spectre for macOS

ScanTailor Spectre targets macOS 15 or later on Apple Silicon. This guide
covers source builds, not approval of a distributable release.

## Source and prerequisites

Build from this public source snapshot. Use Xcode with the macOS SDK, CMake,
Python 3, pkg-config, Boost headers, Qt 6 (including WebEngine, WebChannel,
Positioning and SerialPort), and the image libraries listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Homebrew can supply development dependencies, but current Homebrew binaries
may require a newer macOS than the application's target. Setting
`CMAKE_OSX_DEPLOYMENT_TARGET` does not lower a prebuilt dependency's minimum.
For a macOS 15 distribution, use compatible Qt binaries and compile the native
image-library dependencies for arm64/macOS 15 in an isolated prefix. The
2.0b7 candidate uses official Qt 6.8.3; archive URLs and checksums are in the
adjacent provenance JSON files. Do not rewrite Mach-O deployment metadata.

## Configure and build

Run from the source root. Substitute your own absolute dependency prefixes;
these paths are examples, not an automatic dependency installation procedure:

```bash
QT_PREFIX="/path/to/Qt/6.8.3/macos"
DEPS_PREFIX="/path/to/macos15-dependencies"
PKG_CONFIG_PATH="$DEPS_PREFIX/lib/pkgconfig" cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_FIND_FRAMEWORK=LAST \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX;$DEPS_PREFIX" \
  -DSCANTAILOR_BUNDLE_DEPENDENCY_PREFIXES="$QT_PREFIX;$DEPS_PREFIX" \
  -DPORTABLE_VERSION=OFF
cmake --build build --target scantailor -j$(sysctl -n hw.ncpu)
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

Ensure CMake and pkg-config resolve headers and libraries from the same
compatible installation. Unrelated frameworks installed under
`/Library/Frameworks` can otherwise shadow image-library headers. The SDK's
system zlib header and link library must match; do not mix package-manager
zlib metadata with SDK headers.

## Staged bundle target

`cmake --build build --target scantailor_bundle` copies the incremental app to
`release-staging/ScanTailor Spectre.app` beside `build/`, then deploys Qt,
repairs the transitive dependency closure, copies resources and ad-hoc signs.
It replaces that staged copy. It must never deploy over the incremental app.
`SCANTAILOR_MAC_BUNDLE_POST_BUILD=ON` is intentionally rejected.

The semicolon-separated `SCANTAILOR_BUNDLE_DEPENDENCY_PREFIXES` is an explicit
trust list, not permission to search arbitrary host libraries. The closure
repair fails closed on incompatible minima, unresolved or external loads,
architectures, escaping symlinks and forbidden plugins. For a separately
staged copy, the equivalent repair entry point is:

```bash
packaging/macos/fix-bundle-libs.sh \
  "release-staging/ScanTailor Spectre.app" \
  --prefix "$QT_PREFIX" --prefix "$DEPS_PREFIX"
```

**Verification limit:** the repaired 2.0b7 candidate used a separate staging
procedure and the tested closure repair. The CMake `scantailor_bundle` target
has not yet been exercised end-to-end. Do not infer that every packaging
entry point is verified from the application build or unit-test results.

Never run `macdeployqt` on `build/ScanTailor Spectre.app`: stale bundled Qt
plugins can conflict with freshly linked development binaries.

## Distribution requirements

Ad-hoc signing is not Developer ID signing, notarization or Gatekeeper
acceptance. The legacy `create-dmg.sh` is not release-authoritative; do not
use its signing/notarization options for a release. A distributable release
requires component-by-component Developer ID signing, accepted app and DMG
notarization, validated stapled tickets, signature and Gatekeeper checks,
and matching final timestamped ZIP/DMG plus SHA-256 checksums. Generate the
release ZIP after app stapling; never distribute the submission ZIP.

Inspect every Mach-O architecture, minimum OS, load command, install-name ID
and rpath in the final package. Verify the read-only mounted DMG and extracted
ZIP, and perform real import/process/export smoke tests on the packaged app.
A successful launch is not a functional smoke test. macOS 15 runtime testing
has not been performed; targeting 15 is not proof of runtime compatibility.
See [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) for draft verification status.
