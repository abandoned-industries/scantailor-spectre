#!/bin/bash
# Fix missing transitive dependencies in macOS app bundle
# Usage: fix-bundle-libs.sh <path-to-app-bundle>
set -euo pipefail

APP_BUNDLE="${1:-}"
if [ -z "$APP_BUNDLE" ]; then
    echo "Usage: $0 <path-to-app-bundle>"
    exit 1
fi

FRAMEWORKS_DIR="$APP_BUNDLE/Contents/Frameworks"
INFO_PLIST="$APP_BUNDLE/Contents/Info.plist"

version_gt() {
    [ "$1" = "$2" ] && return 1

    local IFS=.
    local i
    local -a lhs=($1) rhs=($2)
    local len=${#lhs[@]}
    if [ ${#rhs[@]} -gt $len ]; then
        len=${#rhs[@]}
    fi

    for ((i=0; i<len; i++)); do
        local lv=${lhs[i]:-0}
        local rv=${rhs[i]:-0}
        if (( lv > rv )); then
            return 0
        fi
        if (( lv < rv )); then
            return 1
        fi
    done

    return 1
}

retarget_newer_dylibs() {
    local target_minos=""
    local vtool_bin=""

    if [ -f "$INFO_PLIST" ]; then
        target_minos=$(/usr/libexec/PlistBuddy -c "Print :LSMinimumSystemVersion" "$INFO_PLIST" 2>/dev/null || true)
    fi
    if [ -z "$target_minos" ]; then
        echo "LSMinimumSystemVersion not found - skipping dylib deployment target normalization."
        return
    fi

    vtool_bin=$(xcrun -find vtool 2>/dev/null || true)
    if [ -z "$vtool_bin" ]; then
        echo "vtool not available - skipping dylib deployment target normalization."
        return
    fi

    while IFS= read -r dylib; do
        [ -z "$dylib" ] && continue

        local current_minos=""
        local sdk_version=""
        current_minos=$(otool -l "$dylib" | awk '/minos / { print $2; exit }')
        sdk_version=$(otool -l "$dylib" | awk '/sdk / { print $2; exit }')

        if [ -z "$current_minos" ] || [ -z "$sdk_version" ]; then
            continue
        fi

        if version_gt "$current_minos" "$target_minos"; then
            echo "Retargeting $(basename "$dylib") from macOS $current_minos to $target_minos..."
            local tmp_output
            tmp_output="${dylib}.retargeted"
            rm -f "$tmp_output"
            "$vtool_bin" -set-build-version macos "$target_minos" "$sdk_version" -replace -output "$tmp_output" "$dylib"
            mv "$tmp_output" "$dylib"
            chmod 755 "$dylib"
        fi
    done < <(find "$FRAMEWORKS_DIR" -maxdepth 1 -type f -name '*.dylib' -print)
}

bundle_qtdbus() {
    # QtGui transitively loads QtDBus on macOS, but macdeployqt doesn't copy it.
    # Without this, the app either loads Homebrew's QtDBus (pulling in Homebrew's
    # QtCore as a second set of Qt binaries -> "objc duplicate class" warnings
    # and crashes) or fails outright with "Library not loaded: @rpath/QtDBus".
    local qtdbus_target="$FRAMEWORKS_DIR/QtDBus.framework"
    if [ -d "$qtdbus_target" ]; then
        echo "QtDBus.framework already bundled."
        return
    fi

    local qtdbus_src=""
    local candidate
    for candidate in \
        "/opt/homebrew/opt/qt/lib/QtDBus.framework" \
        "/opt/homebrew/lib/QtDBus.framework" \
        "/usr/local/opt/qt/lib/QtDBus.framework" \
        "/usr/local/lib/QtDBus.framework"; do
        if [ -d "$candidate" ]; then
            qtdbus_src="$candidate"
            break
        fi
    done

    if [ -z "$qtdbus_src" ]; then
        echo "Warning: QtDBus.framework not found; app may fail to launch on clean machines."
        return
    fi

    echo "Bundling QtDBus.framework from $qtdbus_src..."
    ditto "$qtdbus_src" "$qtdbus_target"

    local qtdbus_bin="$qtdbus_target/Versions/A/QtDBus"
    chmod 755 "$qtdbus_bin"
    install_name_tool -id "@rpath/QtDBus.framework/Versions/A/QtDBus" "$qtdbus_bin"

    local libdbus_dep
    libdbus_dep=$(otool -L "$qtdbus_bin" | awk '/libdbus-1\.[0-9]+\.dylib/ { print $1; exit }') || true
    if [ -n "$libdbus_dep" ] && [[ "$libdbus_dep" != @rpath/* ]]; then
        install_name_tool -change "$libdbus_dep" "@rpath/libdbus-1.3.dylib" "$qtdbus_bin"
    fi
}

bundle_libdbus() {
    local libdbus_target="$FRAMEWORKS_DIR/libdbus-1.3.dylib"
    if [ -f "$libdbus_target" ]; then
        echo "libdbus-1.3.dylib already bundled."
        return
    fi

    local libdbus_src=""
    local candidate
    for candidate in \
        "/opt/homebrew/opt/dbus/lib/libdbus-1.3.dylib" \
        "/opt/homebrew/lib/libdbus-1.3.dylib" \
        "/usr/local/opt/dbus/lib/libdbus-1.3.dylib" \
        "/usr/local/lib/libdbus-1.3.dylib"; do
        if [ -f "$candidate" ]; then
            libdbus_src="$candidate"
            break
        fi
    done

    if [ -z "$libdbus_src" ]; then
        echo "Warning: libdbus-1.3.dylib not found on system."
        return
    fi

    echo "Bundling libdbus-1.3.dylib from $libdbus_src..."
    cp -L "$libdbus_src" "$libdbus_target"
    chmod 755 "$libdbus_target"
    install_name_tool -id "@rpath/libdbus-1.3.dylib" "$libdbus_target"
}

strip_homebrew_rpaths() {
    # Remove LC_RPATH entries pointing at Homebrew/Macports. If any leak through,
    # the dynamic linker will find Homebrew's Qt before the bundled copy, causing
    # "two sets of Qt binaries" warnings + QObject thread-affinity errors.
    local binary="$1"
    [ -f "$binary" ] || return 0

    local rpaths
    rpaths=$(otool -l "$binary" 2>/dev/null | awk '
        /LC_RPATH/ { want=1; next }
        want && $1=="path" { print $2; want=0 }
    ') || true

    local rp
    while IFS= read -r rp; do
        [ -z "$rp" ] && continue
        case "$rp" in
            /opt/homebrew/*|/opt/local/*|/usr/local/Cellar/*|/usr/local/opt/*)
                echo "  Stripping $rp from $(basename "$binary")"
                install_name_tool -delete_rpath "$rp" "$binary" 2>/dev/null || true
                ;;
        esac
    done <<< "$rpaths"
}

strip_homebrew_rpaths_from_bundle() {
    local exec_name
    exec_name=$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$INFO_PLIST" 2>/dev/null || echo "")
    if [ -n "$exec_name" ]; then
        strip_homebrew_rpaths "$APP_BUNDLE/Contents/MacOS/$exec_name"
    fi

    local fw
    while IFS= read -r fw; do
        [ -z "$fw" ] && continue
        local fw_name fw_binary
        fw_name=$(basename "$fw" .framework)
        fw_binary="$fw/Versions/A/$fw_name"
        strip_homebrew_rpaths "$fw_binary"
    done < <(find "$FRAMEWORKS_DIR" -maxdepth 1 -type d -name '*.framework')

    local dylib
    while IFS= read -r dylib; do
        [ -z "$dylib" ] && continue
        strip_homebrew_rpaths "$dylib"
    done < <(find "$FRAMEWORKS_DIR" -maxdepth 1 -type f -name '*.dylib')
}

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

bundle_qtdbus
bundle_libdbus

fix_webengine_helper_frameworks

strip_homebrew_rpaths_from_bundle

retarget_newer_dylibs

echo "Bundle library fix complete."
