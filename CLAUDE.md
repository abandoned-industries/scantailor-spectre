# CLAUDE.md - ScanTailor Spectre Architecture

## Overview

ScanTailor Spectre transforms raw scans into clean, publication-ready PDFs through an 8-stage processing pipeline. Fork of ScanTailor Advanced with PDF import/export, intelligent color detection, and native Apple Silicon support.

## Directory Structure

```
src/
├── app/                    # Main application (MainWindow, dialogs, startup)
├── core/                   # Core processing logic
│   └── filters/            # The 8 filter stages (see below)
├── imageproc/              # Image processing algorithms (binarize, despeckle, etc.)
├── dewarping/              # Book page flattening algorithms
├── math/                   # Splines, matrices, linear algebra
├── foundation/             # Utilities, data structures
└── acceleration/           # Metal GPU shaders (macOS)
```

## The 8 Filter Stages

Each filter lives in `src/core/filters/{stage}/` with consistent structure:

| Stage | Directory | Purpose |
|-------|-----------|---------|
| 1 | `fix_orientation/` | Rotate pages (90° increments) |
| 2 | `page_split/` | Separate two-page spreads |
| 3 | `deskew/` | Straighten tilted scans |
| 4 | `select_content/` | Define page/content boundaries |
| 5 | `page_layout/` | Set margins and alignment |
| 6 | `finalize/` | Choose color mode (B&W/Gray/Color) - **NEW** |
| 7 | `output/` | Apply image processing (binarize, despeckle, dewarp) |
| 8 | `export_/` | Create final PDF - **NEW** |

### Filter Architecture Pattern

Every filter follows the same structure:
```
filters/{stage}/
├── Filter.cpp/h          # Main filter class (extends AbstractFilter)
├── Task.cpp/h            # Background processing task
├── CacheDrivenTask.cpp/h # Fast path using cached results
├── Settings.cpp/h        # Parameter storage
├── OptionsWidget.cpp/h   # Qt UI controls
├── ImageView.cpp/h       # Image display with overlays
└── Thumbnail.cpp/h       # Preview for thumbnail strip
```

## Key Classes

### Core Infrastructure
- **AbstractFilter** (`src/core/AbstractFilter.h`) - Base class for all 8 filters
- **StageSequence** (`src/core/StageSequence.h`) - Orchestrates filter pipeline
- **ProjectPages** (`src/core/ProjectPages.h`) - Manages image collection
- **BackgroundExecutor** (`src/core/BackgroundExecutor.h`) - Thread pool for async tasks
- **MainWindow** (`src/app/MainWindow.cpp`) - Main UI, implements FilterUiInterface

### Image Processing
- **Binarize** (`src/imageproc/Binarize.h`) - B&W conversion (Sauvola, global threshold)
- **Despeckle** (`src/core/Despeckle.h`) - Noise removal
- **WhiteBalance** (`src/core/WhiteBalance.h`) - Color correction
- **CylindricalSurfaceDewarper** (`src/dewarping/`) - Page flattening

### I/O
- **PdfReader** (`src/core/PdfReader.h`) - PDF import
- **PdfExporter** (`src/core/PdfExporter.h`) - PDF export with quality presets
- **ProjectReader/Writer** (`src/core/`) - .ScanTailor project files (XML)

### Apple-Specific
- **AppleVisionDetector** (`src/core/AppleVisionDetector.h`) - Vision framework for auto color detection
- **MetalContext/MetalGaussBlur/MetalMorphology** (`src/acceleration/`) - GPU acceleration

## Processing Flow

```
Input (PDF/Images)
    ↓
ProjectPages (image collection)
    ↓
Stage 1-5: Geometry corrections
    ↓
Stage 6: Color mode decision (AppleVisionDetector)
    ↓
Stage 7: Image processing → TIFF/PNG output
    ↓
Stage 8: PdfExporter → Final PDF
```

Each page flows through filters independently. Results are cached in project folder.

## Coordinate Systems (ImageViewBase)

The image view manages transforms between:
1. **Image coords** - Original pixels
2. **Virtual coords** - After geometric transforms
3. **Widget coords** - Screen pixels

Key transforms: `m_imageToVirtual`, `m_virtualToWidget`

## Build Commands

```bash
# Install dependencies
brew install qt6 boost libtiff libpng jpeg cmake

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) ..
make -j$(sysctl -n hw.ncpu)

# Bundle Qt frameworks
macdeployqt "ScanTailor Spectre.app" -always-overwrite

# Sign
codesign --force --deep --options runtime --sign "Developer ID Application: ..." "ScanTailor Spectre.app"

# Notarize
xcrun notarytool submit app.zip --keychain-profile "notary" --wait
xcrun stapler staple "ScanTailor Spectre.app"

# Create DMG
hdiutil create -volname "ScanTailor Spectre" -srcfolder "ScanTailor Spectre.app" -ov -format UDZO ScanTailor-Spectre.dmg
```

## Adding a New Filter

1. Create directory `src/core/filters/myfilter/`
2. Implement: Filter, Task, CacheDrivenTask, Settings, OptionsWidget, ImageView, Thumbnail
3. Add to StageSequence in `src/core/StageSequence.cpp`
4. Update CMakeLists.txt in filters directory
5. Add UI integration in MainWindow

## Common Modifications

### Adjust binarization
`src/imageproc/Binarize.cpp` - Sauvola algorithm parameters

### Change color detection
`src/core/AppleVisionDetector.mm` - Vision framework analysis

### Modify PDF export
`src/core/PdfExporter.cpp` - Quality presets, compression

### Add image filter
`src/imageproc/` - Add new algorithm, integrate in output filter

## Dependencies

- Qt6 (UI framework)
- Boost (C++ utilities)
- libtiff, libpng, libjpeg (image formats)
- libharu (PDF generation)
- leptonica (image processing)
- Metal framework (GPU, macOS only)
- Vision framework (content detection, macOS only)
