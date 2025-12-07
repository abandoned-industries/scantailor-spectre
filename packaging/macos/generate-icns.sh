#!/bin/bash
#
# generate-icns.sh - Generate macOS .icns icon from source SVG
#
# This script converts the project's SVG icon to a macOS .icns file
# containing all required icon sizes.
#
# Usage: ./generate-icns.sh [output_path]
#
# Requires: Inkscape or rsvg-convert (from librsvg) for SVG to PNG conversion

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source SVG file
SOURCE_SVG="$PROJECT_ROOT/src/resources/appicon.svg"
OUTPUT_ICNS="${1:-$SCRIPT_DIR/scantailor.icns}"

if [ ! -f "$SOURCE_SVG" ]; then
    echo "Error: Source SVG not found: $SOURCE_SVG"
    exit 1
fi

# Create temporary directory for iconset
ICONSET_DIR=$(mktemp -d)/scantailor.iconset
mkdir -p "$ICONSET_DIR"
trap "rm -rf $(dirname $ICONSET_DIR)" EXIT

echo "Generating icon sizes from $SOURCE_SVG..."

# Function to convert SVG to PNG at specific size
convert_svg() {
    local size=$1
    local output=$2

    if command -v rsvg-convert &> /dev/null; then
        rsvg-convert -w "$size" -h "$size" "$SOURCE_SVG" -o "$output"
    elif command -v inkscape &> /dev/null; then
        inkscape -w "$size" -h "$size" "$SOURCE_SVG" -o "$output" 2>/dev/null
    elif command -v convert &> /dev/null; then
        # ImageMagick as fallback
        convert -background none -resize "${size}x${size}" "$SOURCE_SVG" "$output"
    elif command -v sips &> /dev/null && [ -f "${SOURCE_SVG%.svg}.png" ]; then
        # macOS sips can't handle SVG, but can resize PNG
        echo "Warning: No SVG converter found. Install librsvg: brew install librsvg"
        exit 1
    else
        echo "Error: No image converter found."
        echo "Please install one of: librsvg (recommended), inkscape, or imagemagick"
        echo "  brew install librsvg"
        exit 1
    fi
}

# Generate all required sizes for macOS iconset
# Standard sizes: 16, 32, 128, 256, 512
# @2x sizes: 32, 64, 256, 512, 1024
convert_svg 16 "$ICONSET_DIR/icon_16x16.png"
convert_svg 32 "$ICONSET_DIR/icon_16x16@2x.png"
convert_svg 32 "$ICONSET_DIR/icon_32x32.png"
convert_svg 64 "$ICONSET_DIR/icon_32x32@2x.png"
convert_svg 128 "$ICONSET_DIR/icon_128x128.png"
convert_svg 256 "$ICONSET_DIR/icon_128x128@2x.png"
convert_svg 256 "$ICONSET_DIR/icon_256x256.png"
convert_svg 512 "$ICONSET_DIR/icon_256x256@2x.png"
convert_svg 512 "$ICONSET_DIR/icon_512x512.png"
convert_svg 1024 "$ICONSET_DIR/icon_512x512@2x.png"

echo "Converting iconset to .icns..."

# Convert iconset to icns using iconutil (macOS built-in)
if command -v iconutil &> /dev/null; then
    iconutil -c icns -o "$OUTPUT_ICNS" "$ICONSET_DIR"
    echo "Icon created: $OUTPUT_ICNS"
else
    echo "Error: iconutil not found. This script must be run on macOS."
    exit 1
fi
