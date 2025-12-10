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
# macdeployqt copies libbrotlidec but misses libbrotlicommon
if [ -f "$FRAMEWORKS_DIR/libbrotlidec.1.dylib" ] && [ ! -f "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib" ]; then
    echo "Fixing missing libbrotlicommon..."

    # Try to find libbrotlicommon from Homebrew
    BROTLI_COMMON=""
    for path in "/opt/homebrew/opt/brotli/lib/libbrotlicommon.1.dylib" \
                "/opt/homebrew/lib/libbrotlicommon.1.dylib" \
                "/usr/local/opt/brotli/lib/libbrotlicommon.1.dylib" \
                "/usr/local/lib/libbrotlicommon.1.dylib"; do
        if [ -f "$path" ]; then
            BROTLI_COMMON="$path"
            break
        fi
    done

    if [ -n "$BROTLI_COMMON" ]; then
        echo "Copying $BROTLI_COMMON to bundle..."
        cp "$BROTLI_COMMON" "$FRAMEWORKS_DIR/"
        chmod 755 "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib"

        # Set the id of the copied library
        install_name_tool -id "@rpath/libbrotlicommon.1.dylib" "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib"

        echo "Fixed libbrotlicommon dependency."
    else
        echo "Warning: Could not find libbrotlicommon in standard Homebrew locations."
        echo "You may need to manually copy it to $FRAMEWORKS_DIR/"
    fi
else
    if [ -f "$FRAMEWORKS_DIR/libbrotlicommon.1.dylib" ]; then
        echo "libbrotlicommon.1.dylib already present."
    else
        echo "libbrotlidec.1.dylib not found - skipping brotli fix."
    fi
fi

# Fix libsharpyuv (dependency of libwebp, needed by leptonica)
if [ -f "$FRAMEWORKS_DIR/libwebp.7.dylib" ]; then
    if [ ! -f "$FRAMEWORKS_DIR/libsharpyuv.0.dylib" ]; then
        echo "Fixing missing libsharpyuv..."

        SHARPYUV=""
        for path in "/opt/homebrew/opt/webp/lib/libsharpyuv.0.dylib" \
                    "/opt/homebrew/lib/libsharpyuv.0.dylib" \
                    "/usr/local/opt/webp/lib/libsharpyuv.0.dylib" \
                    "/usr/local/lib/libsharpyuv.0.dylib"; do
            if [ -f "$path" ]; then
                SHARPYUV="$path"
                break
            fi
        done

        if [ -n "$SHARPYUV" ]; then
            echo "Copying $SHARPYUV to bundle..."
            cp -L "$SHARPYUV" "$FRAMEWORKS_DIR/"
            chmod 755 "$FRAMEWORKS_DIR/libsharpyuv.0.dylib"
            install_name_tool -id "@executable_path/../Frameworks/libsharpyuv.0.dylib" "$FRAMEWORKS_DIR/libsharpyuv.0.dylib"
        fi
    else
        echo "libsharpyuv.0.dylib already present."
    fi

    # Fix the @rpath references in webp libraries to use @executable_path
    echo "Fixing webp library paths..."
    install_name_tool -change "@rpath/libsharpyuv.0.dylib" "@executable_path/../Frameworks/libsharpyuv.0.dylib" "$FRAMEWORKS_DIR/libwebp.7.dylib" 2>/dev/null || true

    if [ -f "$FRAMEWORKS_DIR/libwebpmux.3.dylib" ]; then
        install_name_tool -change "@rpath/libwebp.7.dylib" "@executable_path/../Frameworks/libwebp.7.dylib" "$FRAMEWORKS_DIR/libwebpmux.3.dylib" 2>/dev/null || true
        install_name_tool -change "@rpath/libsharpyuv.0.dylib" "@executable_path/../Frameworks/libsharpyuv.0.dylib" "$FRAMEWORKS_DIR/libwebpmux.3.dylib" 2>/dev/null || true
    fi
fi

echo "Bundle library fix complete."
