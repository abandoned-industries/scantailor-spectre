# Building ScanTailor Advanced for macOS

This guide covers building ScanTailor Advanced on macOS, specifically targeting Apple Silicon (ARM64) Macs.

## Prerequisites

### Install Xcode Command Line Tools

```bash
xcode-select --install
```

### Install Homebrew

If you don't have Homebrew installed:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Install Dependencies

```bash
brew install cmake qt6 boost libtiff libpng jpeg zlib libharu
```

**Note:** `libharu` is optional but highly recommended - it enables optimized PDF compression that can reduce file sizes by 10-20x compared to the fallback Qt-based export.

## Building

### 1. Clone the Repository

```bash
git clone https://github.com/4lex4/scantailor-advanced.git
cd scantailor-advanced
```

### 2. Create Build Directory

```bash
mkdir build && cd build
```

### 3. Configure with CMake

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) \
      ..
```

### 4. Build

```bash
make -j$(sysctl -n hw.ncpu)
```

After building, you'll find `ScanTailor Advanced.app` in the build directory.

## Creating a DMG for Distribution

Use the provided script to create a distributable DMG:

```bash
cd packaging/macos
./create-dmg.sh /path/to/build/directory
```

This will create `ScanTailor-Advanced-X.Y.Z.dmg` in the current directory.

## Code Signing

### For Local Use (Ad-hoc Signing)

The build process automatically applies ad-hoc signing, which is sufficient for running on your own machine.

### For Distribution

To distribute the app to other users, you'll need an Apple Developer ID certificate:

```bash
codesign --force --deep --sign "Developer ID Application: Your Name (TEAM_ID)" "ScanTailor Advanced.app"
```

## Universal Binary (ARM64 + x86_64)

To build a universal binary that runs on both Apple Silicon and Intel Macs:

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      ..
make -j$(sysctl -n hw.ncpu)
```

Note: All dependencies must also be universal binaries for this to work.

## Troubleshooting

### Qt not found

Make sure Qt6 is installed and CMAKE_PREFIX_PATH is set correctly:

```bash
brew reinstall qt6
cmake -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) ..
```

### Library not loaded errors

If you get "Library not loaded" errors when running the app, the bundle may not have been properly created. Ensure macdeployqt ran successfully during the build.

### Code signing errors

For ad-hoc signing issues:

```bash
codesign --force --deep --sign - "ScanTailor Advanced.app"
```

## Development Notes

### Project Structure

- `src/app/` - Main application source code
- `src/core/` - Core processing library
- `packaging/macos/` - macOS-specific packaging files

### Useful CMake Options

- `-DCMAKE_BUILD_TYPE=Debug` - Build with debug symbols
- `-DPORTABLE_VERSION=ON` - Build portable version (default)
- `-DDEVELOPER_VERSION=ON` - Enable debug features

### Running from Build Directory

After building, you can run the app directly:

```bash
open "ScanTailor Advanced.app"
```

Or from the command line:

```bash
"./ScanTailor Advanced.app/Contents/MacOS/scantailor"
```
