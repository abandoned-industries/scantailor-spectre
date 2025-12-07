#!/bin/bash
# Fix missing transitive dependencies in macOS app bundle
# Usage: fix-bundle-libs.sh <path-to-app-bundle>

APP_BUNDLE="$1"
if [ -z "$APP_BUNDLE" ]; then
    echo "Usage: $0 <path-to-app-bundle>"
    exit 1
fi

FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"

# Fix missing libbrotlicommon (dependency of libbrotlidec)
if [ -f "$FRAMEWORKS_DIR/libbrotlidec.1.dylib" ] && [ ! -f "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib" ]; then
    echo "Fixing missing libbrotlicommon..."

    # Find the path to libbrotlicommon from libbrotlidec's dependencies
    BROTLI_COMMON=$(otool -L "$FRAMEWORKS_DIR/libbrotlidec.1.dylib" | grep brotlicommon | awk '{print $1}' | head -1)

    if [ -n "$BROTLI_COMMON" ] && [ -f "$BROTLI_COMMON" ]; then
        echo "Copying $BROTLI_COMMON to bundle..."
        cp "$BROTLI_COMMON" "$FRAMEWORKS_DIR/"
        chmod 755 "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib"

        # Fix the reference in libbrotlidec
        install_name_tool -change "$BROTLI_COMMON" "@rpath/libbrotlicommon.1.dylib" "$FRAMEWORKS_DIR/libbrotlidec.1.dylib"

        # Set the id of the copied library
        install_name_tool -id "@rpath/libbrotlicommon.1.dylib" "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib"

        echo "Fixed libbrotlicommon dependency."
    else
        echo "Warning: Could not find libbrotlicommon source library."
    fi
else
    echo "No missing brotli libraries to fix."
fi

echo "Bundle library fix complete."
