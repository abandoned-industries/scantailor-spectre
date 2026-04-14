#!/bin/bash
# Fix missing transitive dependencies in macOS app bundle
# Usage: fix-bundle-libs.sh <path-to-app-bundle>

APP_BUNDLE="$1"
if [ -z "$APP_BUNDLE" ]; then
    echo "Usage: $0 <path-to-app-bundle>"
    exit 1
fi

FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"

fix_webengine_helper_frameworks() {
    local helper_app="$FRAMEWORKS_DIR/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
    local helper_bin="$helper_app/Contents/MacOS/QtWebEngineProcess"
    local helper_frameworks="$helper_app/Contents/Frameworks"

    if [ ! -x "$helper_bin" ]; then
        echo "QtWebEngineProcess helper not found - skipping WebEngine helper fix."
        return
    fi

    echo "Fixing QtWebEngineProcess framework paths..."
    mkdir -p "$helper_frameworks"

    # macdeployqt leaves several helper dependencies pointing at the Homebrew
    # Qt install. The helper's @rpath already reaches the main app Frameworks
    # directory for QtWebEngineCore; expose the other copied Qt frameworks at
    # @executable_path/../Frameworks/ so the helper is self-contained too.
    local deps
    deps=$(otool -L "$helper_bin" | awk '/(\/opt\/homebrew\/|@executable_path\/\.\.\/Frameworks\/).*\.framework\/Versions\/A\// { print $1 }')

    if [ -z "$deps" ]; then
        echo "QtWebEngineProcess helper has no extra framework dependencies to patch."
        return
    fi

    while IFS= read -r dep; do
        [ -z "$dep" ] && continue

        local fw_name
        fw_name=$(echo "$dep" | sed -n 's#^.*/\([^/]*\.framework\)/Versions/A/.*#\1#p')
        if [ -z "$fw_name" ]; then
            echo "Warning: could not parse framework name from $dep"
            continue
        fi

        local binary_name="${fw_name%.framework}"
        local main_fw="$FRAMEWORKS_DIR/$fw_name"
        local helper_fw="$helper_frameworks/$fw_name"
        local new_dep="@executable_path/../Frameworks/$fw_name/Versions/A/$binary_name"

        if [ ! -e "$main_fw" ]; then
            echo "Warning: $fw_name is not present in the main bundle Frameworks directory."
            continue
        fi

        if [ -L "$helper_fw" ]; then
            ln -sfn "../../../../../../../$fw_name" "$helper_fw"
        elif [ ! -e "$helper_fw" ]; then
            ln -s "../../../../../../../$fw_name" "$helper_fw"
        fi

        case "$dep" in
            /opt/homebrew/*)
                install_name_tool -change "$dep" "$new_dep" "$helper_bin" 2>/dev/null || true
                ;;
        esac
    done <<< "$deps"

    # The helper resolves @executable_path/../Frameworks relative to its own
    # Contents/MacOS directory. Qt frameworks and their Homebrew dylib
    # dependencies have already been made portable for the main app, so mirror
    # those dylibs into the helper with symlinks. This completes transitive
    # chains such as QtGui -> libglib -> libintl.
    while IFS= read -r main_dylib; do
        [ -z "$main_dylib" ] && continue

        local dylib_name
        dylib_name=$(basename "$main_dylib")
        local helper_dylib="$helper_frameworks/$dylib_name"

        if [ -L "$helper_dylib" ]; then
            ln -sfn "../../../../../../../$dylib_name" "$helper_dylib"
        elif [ ! -e "$helper_dylib" ]; then
            ln -s "../../../../../../../$dylib_name" "$helper_dylib"
        fi
    done < <(find "$FRAMEWORKS_DIR" -maxdepth 1 -name '*.dylib' -type f -print)
}

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

fix_webengine_helper_frameworks

echo "Bundle library fix complete."
