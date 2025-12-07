#!/bin/bash
#
# create-dmg.sh - Create a distributable DMG for ScanTailor Advanced
#
# Usage: ./create-dmg.sh [build_directory]
#
# If build_directory is not specified, assumes ../../build

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Get build directory from argument or use default
BUILD_DIR="${1:-$PROJECT_ROOT/build}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Usage: $0 [build_directory]"
    exit 1
fi

# Find the app bundle
APP_BUNDLE="$BUILD_DIR/ScanTailor Advanced.app"
if [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: App bundle not found: $APP_BUNDLE"
    echo "Please build the project first with: cmake --build $BUILD_DIR"
    exit 1
fi

# Get version from the app bundle
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || echo "unknown")
echo "Creating DMG for ScanTailor Advanced version $VERSION"

# Create a temporary directory for DMG contents
DMG_TEMP_DIR=$(mktemp -d)
trap "rm -rf $DMG_TEMP_DIR" EXIT

# Copy the app bundle
echo "Copying app bundle..."
cp -R "$APP_BUNDLE" "$DMG_TEMP_DIR/"

# Create a symbolic link to /Applications
ln -s /Applications "$DMG_TEMP_DIR/Applications"

# Create the DMG
DMG_NAME="ScanTailor-Advanced-${VERSION}.dmg"
DMG_PATH="$SCRIPT_DIR/$DMG_NAME"

echo "Creating DMG: $DMG_NAME"

# Remove existing DMG if present
rm -f "$DMG_PATH"

# Create the DMG
hdiutil create \
    -volname "ScanTailor Advanced" \
    -srcfolder "$DMG_TEMP_DIR" \
    -ov \
    -format UDZO \
    "$DMG_PATH"

echo ""
echo "DMG created successfully: $DMG_PATH"
echo ""
echo "To install:"
echo "  1. Open the DMG"
echo "  2. Drag 'ScanTailor Advanced.app' to the Applications folder"
echo ""

# Optional: Open the DMG location in Finder
if [ "$2" == "--open" ]; then
    open -R "$DMG_PATH"
fi
