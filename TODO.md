# ScanTailor Advanced - Pending Tasks

Current version: 1.1.0a74

## 1. Fix App Icon for macOS

The icon needs to follow Apple's Big Sur+ guidelines:
- Rounded rectangle (squircle) mask
- White/light background that shows properly in Dock and app switcher
- Current icon in `/private/tmp/icon_white/` has white background but not the rounded shape
- Source icon location: `packaging/macos/scantailor.icns`

Apple's icon template: https://developer.apple.com/design/resources/

## 2. Add New Filter Stage After Margins

Create a new processing stage that runs after "Margins" (Page Layout):

**Purpose:**
- Run color mode detection (Apple Vision)
- Apply second pass of margins adjustment
- Move processing logic out of Output stage

**Implementation:**
- Add new filter in `src/core/filters/` (similar to existing filters like `page_layout`, `output`)
- Register in `StageSequence.cpp`
- Create Settings, Filter, Task, OptionsWidget, etc.
- Stage name suggestion: "Finalize" or "Prepare"

**Current filter order:**
1. Fix Orientation
2. Split Pages
3. Deskew
4. Select Content
5. Margins (Page Layout)
6. **[NEW STAGE HERE]**
7. Output

## 3. Simplify Output Stage to Export-Only

After adding the new stage, Output becomes just file export:
- PDF as default output format
- Remove color mode detection from Output (moved to new stage)
- Keep output format options (TIFF, JPEG, PDF)
- Keep compression options

## 4. Select Content Clips Photo Edges

The content box finder clips photos that extend to the page edge.

**Problem:**
- `inPlaceRemoveAreasTouchingBorders()` in `ContentBoxFinder.cpp:475` removes content that touches borders
- This is designed to eliminate shadows on scanned pages, but clips photos extending to edge
- Algorithm spreads from border up to `min(width, height) / 4` pixels inward

**Solution options:**
- Detect if content is a photo (continuous tone) vs text before applying border removal
- Add setting to disable border content removal
- Make spread distance configurable

**Key file:** `src/core/filters/select_content/ContentBoxFinder.cpp`

## 5. Margins Filter Requires Double-Click

User has to click the Margins filter twice to apply it.

**To investigate:**
- Check signal/slot connections in `src/core/filters/page_layout/OptionsWidget.cpp`
- Verify filter switching logic in `MainWindow.cpp`

## 6. Metal Dilation Crash (Disabled)

Metal-accelerated dilation in `Morphology.cpp` causes assertion failures.

**Symptom:** Crash with `darkestNeighbor <= pixel` assertion in `mokjiThreshold`

**Current fix:** Disabled Metal dilation with `#if 0` in `Morphology.cpp:692`

**Proper fix:** Debug Metal compute shader returning incorrect values

**Key file:** `src/imageproc/Morphology.cpp`

## Notes

- Color detection logic is in `src/core/AppleVisionDetector.mm` (`suggestColorMode()`)
- Current detection prioritizes text coverage over colorfulness (fixed in a74)
- Color detection fixes applied: stricter variancePhoto, require colorfulness for continuous-tone, skip full-page non-text regions, strong indicators must beat document confidence by 20%, grayscale vs color for embedded images
- Settings for output are in `src/core/filters/output/Settings.cpp`
