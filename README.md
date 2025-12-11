# ScanTailor Spectre

**Version 2.0b1** | macOS (Apple Silicon)

ScanTailor Spectre transforms raw scans into clean, publication-ready pages. Import a PDF or folder of images, process through an 8-stage workflow, and export a polished PDF.

## What's New in Spectre

- **PDF Import** - Open PDFs directly, no need to extract pages first
- **Finalize Stage** - New stage for color mode selection and output format
- **Export Stage** - New dedicated PDF export with quality presets 
- **Redesigned UI** - Clean, focused interface
- **Apple Silicon Native** 
- **Intelligent Color Detection** - Auto-detects B&W vs grayscale vs color pages

---

## Quick Start

### 1. Import Your Scans

From the startup screen:
- **Import PDF** - Extract pages from an existing PDF
- **Import Folder** - Load a folder of scanned images
- **Import Project** - Load a folder of scanned images

### 2. Work Through the Filters

Each filter stage refines your pages. Work top to bottom:

| # | Stage | Purpose |
|---|-------|---------|
| 1 | **Fix Orientation** | Rotate pages right-side up |
| 2 | **Split Pages** | Separate two-page spreads |
| 3 | **Deskew** | Straighten tilted scans |
| 4 | **Select Content** | Crop to content area |
| 5 | **Margins** | Set page margins |
| 6 | **Finalize** | Choose B&W, Grayscale, or Color |
| 7 | **Output** | Apply image processing |
| 8 | **Export** | Create final PDF |\

To run a stage, click on the little triangular play button next to the stage you are at. Stage 8 is special. You don’t need to run it, you simply click on Export to PDF. 

### 3. Batch Process

You may want to try doing everything automatically if your scan is clean. If you need to split spreads to pages, run stage 2. If not, or if you have run stage 2 already go to stage 5, Margins, and run each of the next stages. 
---

## The Workflow Explained

### Stage 1: Fix Orientation

Corrects pages that are upside-down or rotated. Auto-detection handles most cases; use the rotate buttons for manual override. This is not the same as Stage 3, Deskew. 

### Stage 2: Split Pages

Separates book spreads (two pages per scan) into individual pages. The split line is auto-detected but can be adjusted manually. A helpful dialog tells you the results. You can force ScanTailor to split pages it didn’t identify as split at this point and you can also inspect the pages by clicking on them. 

### Stage 3: Deskew

Straightens pages that were slightly tilted during scanning. Even 1-2° of tilt is noticeable in output.

**Tip:** Sort by "decreasing deviation" to review the most-skewed pages first. The sort order is located at the bottom of the thumbnail panel on the right. 

### Stage 4: Select Content

Defines the content area by drawing a box around it. Everything outside the box is cropped. Drag corners and edges to adjust.

This excludes:
- Scanner bed edges
- Book margins you want to remove
- Fingers or page holders

This is not always perfect. If you have a lot of images, you should inspect this by hand. 

### Stage 5: Margins

Sets white space around content in the final output.

- **Top/Bottom/Left/Right** - Individual margin sizes
- **Alignment** - Where content sits within the page

### Stage 6: Finalize

**New in Spectre.** This stage determines how each page will be processed:

- **Color Mode**: Black & White, Grayscale, or Color
- **Output Format**: TIFF or PNG
- **Output Location**: Where processed files are saved

The app auto-detects the appropriate color mode. The **Midtone Threshold** slider adjusts detection sensitivity, useful for smaller images (lower = more pages will be inspected and detection will be slower). You can override the determination after inspection.

Previous versions of Scantailor exported to images, the workflow in this one is images or PDF to PDF. Images will be discarded at the end of the process unless you choose to preserve them at this point by ticking the box to “Preserve Output Images.”

### Stage 7: Output

Applies image processing to generate the output files:

- **Dewarping** - Flattens curved pages from book spines
- **Despeckle** - Removes small dots and noise
- **Picture Zones** - Areas to treat as photographs
- **Fill Zones** - Areas to fill with solid color

You can choose various filters for processing pages depending on the mode you are in (color, grayscale, or black and white). Once you tune a page 

### Stage 8: Export

**New in Spectre.** Creates the final PDF.

**Quality Levels:**
No DPI limit
This 

**Max DPI:** Limits output resolution. 400 DPI is the Library of Congress recommendation for most documents.

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1` - `8` | Jump to filter stage |
| `Page Up/Down` | Previous/next page |
| `Home` / `End` | First/last page |
| `+` / `-` | Zoom in/out |
| `Ctrl+S` | Save project |

---

## Tips

### Scanning
- **300-400 DPI** for text documents
- **600 DPI** for fine detail or small text
- Scan in **color** even for B&W content - better conversion results

### Processing
- Work through stages **in order** - each depends on the previous
- Configure **one page well**, then batch apply to similar pages
- Use **page ordering** options to find problem pages
- **Save frequently** - all settings are preserved in the project file

### Color Mode Selection
- **B&W (1-bit)**: Smallest files, pure black and white, ideal for text-only
- **Grayscale**: Better for pages with photos or illustrations
- **Color**: Only when color information matters

### Typical File Sizes (100-page book)
- B&W PDF: 5-15 MB
- Grayscale PDF: 20-50 MB
- Color PDF: 50-150 MB

---

## Project Files

Projects are saved as `.ScanTailor` files containing:
- References to source images (not copies)
- All settings for every stage
- Processing state

**Important:** Keep source images in place - projects reference them by path.

---

## Building from Source

### Requirements
- macOS 12+ (Apple Silicon)
- Xcode Command Line Tools
- Homebrew

### Install Dependencies
```bash
brew install cmake qt6 boost libtiff libpng jpeg leptonica libharu
```

### Build
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) ..
cmake --build . -j$(sysctl -n hw.ncpu)
```

### Create DMG
```bash
./packaging/macos/create-dmg.sh
```

---

## Differences from ScanTailor Advanced

| Feature | ScanTailor Advanced | ScanTailor Spectre |
|---------|--------------------|--------------------|
| Workflow stages | 6 | 8 (adds Finalize + Export) |
| PDF import | No | Yes |
| PDF export | Menu action | Dedicated stage with presets |
| Color detection | Manual | Automatic with manual override |
| macOS support | Limited | Native Apple Silicon |
| UI design | Traditional | Ulm design system |

ScanTailor Spectre includes all features from ScanTailor Advanced (dewarping, despeckle, picture zones, fill zones, etc.) plus the new workflow stages.

---

## Credits

ScanTailor Spectre is based on:
- [ScanTailor Advanced](https://github.com/ScanTailor-Advanced/scantailor-advanced) by 4lex4
- [ScanTailor](https://scantailor.org/) by Joseph Artsimovich

## License

GPL-3.0 - See [LICENSE](LICENSE) for details.
