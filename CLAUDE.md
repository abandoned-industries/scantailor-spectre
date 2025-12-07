# CLAUDE.md - ScanTailor Advanced macOS Port

## Project Goal

Add native Apple Silicon (ARM64) macOS support to ScanTailor Advanced, producing a self-contained `.app` bundle that can be distributed as a DMG without requiring Homebrew or any other dependencies for end users.

## Current State

- ScanTailor Advanced v1.0.19 is the latest release (GPL-3.0 license)
- CMakeLists.txt has Windows packaging via CPack/NSIS but no macOS bundle support
- The existing third-party macOS build (yb85/scantailor-advanced-osx) only produces x64 binaries and has a broken Fish shell bundler script
- The app builds fine on macOS but produces only a command-line binary, not an .app bundle

## Tasks

### 1. Add macOS Bundle Support to CMakeLists.txt

Modify the main `CMakeLists.txt` to:
- Set `MACOSX_BUNDLE TRUE` for the scantailor target when building on Apple
- Configure bundle metadata (name, identifier, icon, version)
- Run `macdeployqt` as a post-build step to copy Qt frameworks into the bundle
- Handle code signing (ad-hoc for local builds)

Key CMake variables/properties needed:
```cmake
MACOSX_BUNDLE
MACOSX_BUNDLE_BUNDLE_NAME
MACOSX_BUNDLE_GUI_IDENTIFIER
MACOSX_BUNDLE_ICON_FILE
MACOSX_BUNDLE_INFO_PLIST
```

### 2. Create Packaging Directory Structure

```
packaging/
└── macos/
    ├── BUILD.md           # Build instructions for developers
    ├── Info.plist.in      # Bundle metadata template (CMake configures this)
    ├── scantailor.icns    # App icon (convert from existing or create)
    └── create-dmg.sh      # Script to create distributable DMG
```

### 3. Write BUILD.md

Document the build process for Apple Silicon Macs:
- Required dependencies (Qt6, Boost, libtiff, libpng, libjpeg, zlib)
- How to install them via Homebrew (for developers only)
- CMake configuration and build commands
- How to create a release DMG

### 4. Optional: GitHub Actions Workflow

Add `.github/workflows/macos-build.yml` to automatically build ARM64 releases.

## Build Dependencies

These are needed at compile time (via Homebrew for developers):
- qt6 (Qt6 framework - UI)
- boost (C++ libraries)
- libtiff
- libpng  
- jpeg
- zlib (usually included with macOS)
- cmake (build system)

## Key Files to Examine

- `CMakeLists.txt` - main build configuration
- `src/app/` - likely contains main() and app initialization
- `version.h.in` - version template
- `resources/` - icons and other assets (if exists)

## Technical Notes

### macdeployqt
Qt provides `macdeployqt` tool that:
- Copies Qt frameworks into the .app bundle
- Rewrites library paths using `install_name_tool`
- Makes the bundle self-contained

Typical usage:
```bash
macdeployqt ScanTailor.app -always-overwrite
```

### Code Signing
For distribution outside the App Store:
- Ad-hoc signing works for local use: `codesign --force --deep --sign - ScanTailor.app`
- Proper distribution needs a Developer ID certificate

### Universal Binary (optional future work)
Could build for both ARM64 and x86_64:
```cmake
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
```

## Future Enhancement: PDF Import

ScanTailor expects individual image files as input. A useful addition would be a preprocessing step to split PDFs into images. This could be:
- A separate helper tool bundled with the app
- Integration into ScanTailor's file loading (more complex, requires adding PDF library dependency like poppler or PDFium)

For now, focus on getting the basic macOS build working first.

## Commands Reference

### Building (once dependencies installed)
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) ..
make -j$(sysctl -n hw.ncpu)
```

### Creating DMG
```bash
hdiutil create -volname "ScanTailor Advanced" -srcfolder "ScanTailor Advanced.app" -ov -format UDZO ScanTailor-Advanced.dmg
```
