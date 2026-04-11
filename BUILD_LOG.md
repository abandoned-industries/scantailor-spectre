 PDF Orientation Fix Attempts - All Failures

  The Problem

  PDF pages render "upside down and backwards" when using CoreGraphics
  (CGPDFDocument/CGContextDrawPDFPage) instead of Qt::Pdf.

  What I Was Trying to Do

  Replace Qt::Pdf (requires macOS 12+) with CoreGraphics (available on macOS 11+) for PDF reading.

  ---
  Attempt 1: Direct buffer with mediaBox origin fix

  Code:
  CGContextTranslateCTM(context, -mediaBox.origin.x, -mediaBox.origin.y);
  Result: FAILED - pages still wrong

  Attempt 2: Using CGPDFPageGetDrawingTransform alone

  Code:
  CGAffineTransform drawTransform = CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, destRect, 0,
   true);
  CGContextConcatCTM(context, drawTransform);
  CGContextDrawPDFPage(context, page);
  Result: FAILED - made things worse

  Attempt 3: Two-stage render with CGImage intermediate

  Code: Rendered to CGContext, created CGImage, copied to QImage with manual Y-flip
  Result: FAILED - "did something unexpected"

  Attempt 4: PDFKit (PDFDocument/PDFPage)

  Code: Used Objective-C PDFKit framework instead of CoreGraphics
  Result: FAILED - "worse for sure"

  Attempt 5: CropBox + CGPDFPageGetDrawingTransform

  Code:
  CGContextTranslateCTM(context, 0, heightPx);
  CGContextScaleCTM(context, 1, -1);
  // Then applied CGPDFPageGetDrawingTransform with kCGPDFCropBox
  Result: FAILED - "better" but still upside down and backwards, thumbnails bad

  Attempt 6: Double Y-flip (cancelled out)

  Code:
  CGContextTranslateCTM(context, 0, heightPx);
  CGContextScaleCTM(context, 1, -1);
  CGContextTranslateCTM(context, 0, heightPx);
  CGContextScaleCTM(context, scale, -scale);
  Result: FAILED - The two flips cancelled each other out. No change.

  Attempt 7: Single Y-flip with DPI scale

  Code:
  CGContextTranslateCTM(context, 0, heightPx);
  CGContextScaleCTM(context, scale, -scale);
  Result: FAILED - Still upside down and backwards

  Attempt 8: Two-stage render (current, untested by user)

  Code:
  // Stage 1: Render PDF to CG-managed buffer
  CGContextRef context = CGBitmapContextCreate(nullptr, ...);
  CGContextScaleCTM(context, scale, scale);
  CGContextConcatCTM(context, CGPDFPageGetDrawingTransform(...));
  CGContextDrawPDFPage(context, page);
  CGImageRef cgImage = CGBitmapContextCreateImage(context);

  // Stage 2: Draw CGImage to QImage buffer with Y-flip
  CGContextRef destContext = CGBitmapContextCreate(image.bits(), ...);
  CGContextTranslateCTM(destContext, 0, heightPx);
  CGContextScaleCTM(destContext, 1, -1);
  CGContextDrawImage(destContext, rect, cgImage);
  Result: PENDING - just built, not tested by user yet

  ---
  Key Coordinate System Facts I've Established

  1. CGBitmapContext: device (0,0) = buffer byte 0 = buffer row 0
  2. CGContext drawing: Origin at bottom-left, Y goes up (standard Quartz)
  3. PDF coordinates: Origin at bottom-left, Y goes up (matches Quartz)
  4. QImage: Buffer row 0 = visual TOP of image

  The Core Problem

  When rendering to a buffer that QImage will interpret:
  - PDF (0,0) → device (0,0) → buffer row 0 → QImage TOP
  - But PDF (0,0) is the BOTTOM of page content
  - So page appears upside down without correction

  References I Found

  - https://github.com/vfr/Reader/blob/master/Sources/ReaderThumbRender.m - Uses
  CGBitmapContextCreateImage, no manual Y-flip
  - https://ryanbritton.com/2015/09/correctly-drawing-pdfs-in-cocoa/ - Use CropBox, handle
  rotation
  - Apple docs on CGPDFPageGetDrawingTransform

  ---
  File Modified

  /Users/kazys/Developer/scantailor-weasel/src/core/PdfReader.mm - readImage() function around
  lines 231-316

  Todos
  ☒ Rewrite readImage() with two-stage render approach
  ☐ Build and test the fix

---
2025-12-28 11:56 - Debug build (cmake + make)
- CMake: Debug config ok; warnings about CMP0167/WrapVulkanHeaders.
- First make timed out after 120s (sysctl -n hw.ncpu not permitted in sandbox); reran make -j4 and completed.
- Build succeeded with existing warnings (unused params, override, duplicate libs).
- App bundle: build/ScanTailor Spectre.app (ad-hoc signed).

---
2025-12-28 12:06 - Debug rebuild (make -j4)
- Updated PdfReader.mm: restore DPI scale before CGPDFPageGetDrawingTransform; destRect in points.
- Build succeeded; warnings include QImage::mirrored deprecation and existing unused params.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 12:12 - Debug rebuild (make -j4)
- Updated PdfReader.mm: flip context before drawing; removed QImage::mirrored.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 12:17 - Debug rebuild (make -j4)
- Updated PdfReader.mm: manual flip->scale->crop-origin translate; added rotation handling without CGPDFPageGetDrawingTransform.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 12:30 - Debug rebuild (make -j4)
- Added QPainter include and red top-left debug marker in PdfReader.mm.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 12:28 - Debug rebuild (make -j4) FAILED
- PdfReader.mm debug marker build failed: missing QPainter include.
- Fixed by adding <QPainter> and rebuilding.

---
2025-12-28 12:41 - Debug rebuild (make -j4)
- PdfReader.mm: replaced debug marker with L-shape + bottom-right square.
- LoadFileTask.cpp: force thumbnail recreation for debug visibility.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 13:00 - Debug rebuild (make -j4)
- PdfReader.mm: added 180° rotation correction after rotation metadata (TEMP).
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 13:06 - Debug rebuild (make -j4)
- PdfReader.mm: added horizontal mirror correction after 180° fix (TEMP).
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 13:38 - Debug rebuild (make -j4)
- PdfReader.mm: added per-page debug logging (media/crop box, rotation, display size).
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 13:55 - Debug rebuild (make -j4)
- PdfReader.mm: reverted to CGPDFPageGetDrawingTransform (crop/media), removed manual rotations, mirrors, and debug markers/logging.
- LoadFileTask.cpp: removed forced thumbnail recreation.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:16 - Debug rebuild (make -j4)
- PdfReader.mm: use CGPDFPageGetDrawingTransform with DPI scale, then vertical flip via QImage::mirrored.
- Build succeeded with existing warnings (deprecated QImage::mirrored).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:19 - Debug rebuild (make -j4)
- PdfReader.mm: moved Y-flip into CGContext; removed QImage::mirrored.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:28 - Debug rebuild (make -j4)
- PdfReader.mm: CGPDFPageGetDrawingTransform in pixel space; removed pre-scale and added QImage::mirrored.
- Build succeeded with existing warnings (deprecated QImage::mirrored).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:33 - Debug rebuild (make -j4)
- PdfReader.mm: CGPDFPageGetDrawingTransform with manual upscale around box center when DPI > 72; QImage::mirrored for Y-flip.
- Build succeeded with existing warnings (deprecated QImage::mirrored).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:49 - Debug rebuild (make -j4)
- PdfReader.mm: flip context, scale to DPI, CGPDFPageGetDrawingTransform in points (no extra scaling), no post-flip.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 14:58 - Debug rebuild (make -j4)
- PdfReader.mm: added 180° rotation after CGPDFPageGetDrawingTransform while keeping DPI scaling and top-left flip.
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 15:07 - Debug rebuild (make -j4)
- PdfReader.mm: replaced CGPDFPageGetDrawingTransform with manual crop+rotation transforms (after DPI scale + top-left flip).
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 15:20 - Code change (pending rebuild/test)
- PdfReader.mm: fixed 90° rotation translation to use box height (was width), to prevent tiny/cropped renders on rotated pages.

---
2025-12-28 15:23 - Debug rebuild (cmake --build . -j4)
- Build initially timed out at 10s, reran with longer timeout and succeeded.
- Warnings unchanged (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 17:00 - Debug rebuild (cmake --build . -j4)
- PdfReader.mm: added buffer-origin probe (1x1 red pixel at device origin, read back top-left vs bottom-left).
- Build succeeded with existing warnings.
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 17:32 - Code change (pending rebuild/test)
- PdfReader.mm: apply PDF UserUnit when computing DPI scale to avoid tiny renders on PDFs with non-1 UserUnit; removed buffer-origin probe.

---
2025-12-28 17:34 - Code change (pending rebuild/test)
- PdfReader.mm: switch to CGPDFPageGetDrawingTransform with DPI/UserUnit scale; remove manual flip/rotation and apply vertical QImage mirror after drawing.

---
2025-12-28 17:36 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 17:58 - Debug rebuild (cmake --build . -j4)
- PdfReader.mm: added per-page logging (rotation, media/crop box, displaySize, userUnit, dpi).
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 18:02 - Code change (pending rebuild/test)
- PdfReader.mm: use box size (not displaySize) for render dims, cancel page rotation in CGPDFPageGetDrawingTransform, then rotate QImage to match rotation; added boxSize to logs.

---
2025-12-28 18:04 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 18:14 - Code change (pending rebuild/test)
- PdfReader.mm: log CGPDFPageGetDrawingTransform matrix to diagnose tiny/cropped render scale.

---
2025-12-28 18:16 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 18:21 - Code change (pending rebuild/test)
- PdfReader.mm: size render buffer from displaySize again and let CGPDFPageGetDrawingTransform apply PDF rotation; removed QImage rotation step.

---
2025-12-28 18:23 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 18:46 - Code change (pending rebuild/test)
- PdfReader.mm: pass page rotation angle into CGPDFPageGetDrawingTransform to avoid identity transform on rotated pages.

---
2025-12-28 18:47 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (deprecated QImage::mirrored, unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 21:51 - Code change (pending rebuild/test)
- PdfReader.mm: removed post-draw vertical flip to avoid double-flip/mirror.

---
2025-12-28 21:53 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 21:55 - Code change (pending rebuild/test)
- ThumbnailPixmapCache.cpp: bump thumbnail cache key version to invalidate old tiny thumbnails.

---
2025-12-28 21:57 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 22:00 - Code change (pending rebuild/test)
- PdfReader.mm: invert rotation sign passed to CGPDFPageGetDrawingTransform to correct 90° direction.

---
2025-12-28 22:02 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 22:03 - Code change (pending rebuild/test)
- PdfReader.mm: drop CGPDFPageGetDrawingTransform and use manual translate+rotate without post-flip to respect PDF rotation.

---
2025-12-28 22:05 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 22:09 - Code change (pending rebuild/test)
- ThumbnailPixmapCache.cpp: bump thumbnail cache key version to v3 to refresh rotation after reader fix.

---
2025-12-28 22:11 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 22:23 - Code change (pending rebuild/test)
- PdfReader.mm: remove per-page debug logging now that orientation is fixed.

---
2025-12-28 22:27 - Debug rebuild (cmake --build . -j4)
- PdfReader.mm: apply UserUnit in readMetadata() sizing to match render scale.
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 22:28 - Code change (pending rebuild/test)
- PdfReader.mm: remove unused cropBox variable after manual transform switch.

---
2025-12-28 22:30 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 23:00 - Code change (pending rebuild/test)
- CMakeLists.txt: hard-fail non-macOS builds (macOS-only support).
- PdfReader.mm: remove non-macOS PDF stubs.
- AppleVisionDetector.mm: remove non-macOS stubs.

---
2025-12-28 23:05 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 23:12 - Code change (pending rebuild/test)
- CMakeLists.txt: enforce macOS-only by checking CMAKE_SYSTEM_NAME == Darwin (excludes iOS).

---
2025-12-28 23:14 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 23:31 - Code change (pending rebuild/test)
- PdfReader.mm: clip to effective box after rotation to prevent bleed outside crop/media bounds.

---
2025-12-28 23:32 - Debug rebuild (cmake --build . -j4)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-28 - Code review fixes (cmake --build . -j4)
- WhiteBalance.cpp: Replace rand() with std::mt19937 for deterministic sampling (fixed seed 42)
- AppleVisionDetector.mm: Add null-check for colorSpace, clarify CF reference counting, add zero-width check
- PdfReader.mm: Add null-check after ensureLoaded(), release mutex before callbacks to prevent deadlocks
- finalize/Task.cpp: Remove unused m_downscaledImage member, pass empty QImage (ImageViewBase auto-creates)
- Build succeeded with existing warnings (unused params, duplicate libs, missing override).
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-29 - Detection Settings for art books (make -j8)
- Settings.h/cpp: Added contentFillFactor and borderTolerance project-wide settings
- OptionsWidget.ui: Added Detection Settings collapsible group with Fill Factor and Border Tolerance sliders
- OptionsWidget.cpp: Connected sliders, clear cached params on settings change to force re-detection
- ContentBoxFinder.cpp: When Fill Factor ≥0.95, return full page as content (bypass text detection)
- ContentBoxFinder.cpp: Skip shadow removal when Fill Factor >0.85
- ContentBoxFinder.cpp: Reduce UEP (text pattern) requirement at high fill factor
- ContentBoxFinder.cpp: Fix Border Tolerance logic (higher = preserve more edge content)
- PdfReader.mm: Remove faulty clipping code that was cutting off pages
- Task.cpp: Pass Settings to ContentBoxFinder
- README.md: Document Detection Settings in Stage 4 section and What's New
- Build succeeded, committed and pushed to os-11-compatibility (3b6e7f3)

---
2025-12-29 - Fix color detection for pages with embedded photos (cmake --build . -j8)
- LeptonicaDetector.cpp: Always check for embedded images (high midtone regions) before declaring B&W
- Bug: Pages with small photos on large white backgrounds had <10% overall midtones, so region check was SKIPPED
- Fix: Now checks regions for ANY candidate B&W page with ≥1% midtones, catching embedded images
- This should fix pages 4, 5, 6 in krauss-sample.pdf being incorrectly classified as B&W
- Build succeeded with expected warnings (unused midtoneThreshold param in isPureBW - can be removed later)
- TESTED: Pages 4L (4.2% midtones, region had 42.3%), 6R, 8L, 8R, 9L, 9R, 10R all correctly detected as grayscale
- Text pages (1L, 1R, 3L, 3R, 7R) correctly remained B&W

---
2025-12-29 - Release build v2.0a12 (pending)
- Includes: Detection Settings for art books, color detection fix for embedded photos
- Building, signing, notarizing, creating DMG

---
2025-12-29 - Phase 3: Thread safety for MetalMorphology (make -j8)
- MetalMorphology.mm: Added serial dispatch queue (getMorphologyQueue) 
- MetalMorphology.mm: Wrapped performMorphOp GPU operations in dispatch_sync with @autoreleasepool
- Matches thread safety pattern from MetalGaussBlur.mm

---
2025-12-29 - Phase 3: Thread safety for MetalMorphology (make -j8)
- MetalMorphology.mm: Added serial dispatch queue (getMorphologyQueue) 
- MetalMorphology.mm: Wrapped performMorphOp GPU operations in dispatch_sync with @autoreleasepool
- Matches thread safety pattern from MetalGaussBlur.mm
- Build succeeded (rpath warnings are normal macdeployqt noise)
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-29 - Phase 1: App lifecycle monitoring (make -j8)
- NEW: MetalLifecycle.h/mm - Detects app foreground/background state via NSNotifications
- CMakeLists.txt (acceleration): Added MetalLifecycle files and AppKit framework
- main.cpp: Initialize metalLifecycleInit() at startup
- MetalGaussBlur.mm: Re-enabled! Now checks metalIsAppActive() before GPU operations
- MetalMorphology.mm: Added metalIsAppActive() check
- Build succeeded (existing warnings: unused params, missing override, reorder-ctor)
- App bundle refreshed: build/ScanTailor Spectre.app
- Metal GaussBlur is now ENABLED with lifecycle protection!

## 2025-12-29 12:45 - v2.0a12 RELEASE

- **Status**: ✅ Released
- **DMG**: ScanTailor-Spectre-2.0a12-20251229-1239.dmg (51MB)
- **Release**: https://github.com/abandoned-industries/scantailor-spectre/releases/tag/v2.0a12
- **Changes**:
  - Detection Settings (Fill Factor, Border Tolerance)
  - Improved color detection for embedded photographs
  - Updated README.md and README.pdf


---
2025-12-29 - Phase 11: Parallel thumbnail generation (make -j8)
- ThumbnailPixmapCache.cpp: Added QThreadPool for parallel loading
- ThumbnailPixmapCache.cpp: Modified backgroundProcessing() to dispatch ALL queued items to thread pool
- ThumbnailPixmapCache.cpp: Uses QThread::idealThreadCount() threads (16 on M4 Max)
- Previous: single QThread processed one thumbnail at a time
- Now: all pending thumbnails load in parallel across all CPU cores
- Build succeeded
- App bundle refreshed: build/ScanTailor Spectre.app

---
2025-12-29 - GPU-accelerate page split + re-enable dilation (make -j8)
- Morphology.cpp: Re-enabled GPU dilation (was #if 0 due to bug)
- Thread safety fix from Phase 3 may have resolved the dilation bug
- erodeGray and dilateGray now both use Metal GPU when available
- page_split/VertLineFinder already uses erodeGray → now GPU accelerated
- closeGray (dilate+erode) → both operations now GPU accelerated
- Build succeeded
- App bundle refreshed: build/ScanTailor Spectre.app
- NOTE: Test page split and output stages to verify dilation works correctly

---
2024-12-29 15:30 - Apple Silicon performance optimizations (make -j8)
- src/imageproc/Scale.cpp: Add vImage-accelerated scaling (SIMD Lanczos)
- src/imageproc/Binarize.cpp: Add dispatch_apply for parallel Sauvola
- src/imageproc/CMakeLists.txt: Link Accelerate framework

---
2025-12-29 15:45 - Fix PDF export finding output files with any extension (make -j8)
- OutputFileNameGenerator.h: Add findExistingOutputFile() declaration
- OutputFileNameGenerator.cpp: Implement findExistingOutputFile() to search .tif, .png, .jpg
- MainWindow.cpp: Update exportToPdf() to use findExistingOutputFile()
- MainWindow.cpp: Update exportToPdfFromFilter() to use findExistingOutputFile()
- MainWindow.cpp: Update OCR data collection to use findExistingOutputFile()

Bug: 447-page book was only exporting 1 page because export searched for files with
current format extension only. If format changed after processing, files weren't found.

---
2025-12-29 16:00 - Add debug logging to PDF export (make -j8)
- MainWindow.cpp: Add qDebug statements to see page count and which files are found

---
2025-12-29 16:15 - Implement relative path storage in project files (make -j8)
- ProjectReader.h: Added new constructor with project file path parameter, added m_projectDir member
- ProjectReader.cpp: New constructor resolves relative paths on load, processDirectories() handles both absolute and relative paths with fallback logic
- ProjectWriter.h: Updated processDirectories() signature to accept projectDir
- ProjectWriter.cpp: Added QDir include, write() converts output directory to relative, processDirectories() converts directory paths to relative
- ProjectOpeningContext.cpp: Updated to use new ProjectReader constructor with project file path

---
2025-12-29 16:50 - Fix PDF export for large TIFF files (make -j8)
- PdfExporter.cpp: Added TiffReader include
- PdfExporter.cpp: Updated both image loading locations (libharu and Qt PDF) to use TiffReader::readImage() for TIFF files instead of QImage - TiffReader uses libtiff directly which handles large files better than Qt's TIFF plugin

---
2025-12-29 19:25 - Performance optimizations (make -j8)
- MetalGaussBlur.mm: Lower MIN_GPU_DIMENSION from 64 to 32
- MetalMorphology.mm: Lower MIN_GPU_SIZE from 64 to 32
- Morphology.cpp: Add vImage SIMD acceleration for erosion/dilation CPU fallback

---
2025-12-30 19:40 - v2.0a15 RELEASE
- **Status**: ✅ Released
- **DMG**: ScanTailor-Spectre-2.0a15-20251230-1934.dmg
- **Release**: https://github.com/abandoned-industries/scantailor-spectre/releases/tag/v2.0a15
- **Notes**:
  - README updated to 2.0a15, PDF regenerated and bundled before signing
  - App notarized + stapled, DMG notarized + stapled
  - DMG creation: `hdiutil create -srcfolder` failed with `Operation not permitted` (likely /Volumes permission), so used `hdiutil makehybrid` to create `*.cdr.dmg` then `hdiutil convert -format UDZO` to produce the final DMG

---
2024-12-29 21:15 - Release build 2.0a14 (make -j16)
- Full release build for DMG
- Version 2.0a14 with performance optimizations

---
2024-12-29 21:50 - Fix Apply To B&W bug (make -j16)
- ApplyColorsDialog.h/cpp: Add finalize::Settings parameter
- Use finalize settings (stage 6) for color mode filtering instead of output settings
- OptionsWidget.cpp: Pass m_finalizeSettings to ApplyColorsDialog

---
2025-12-30 - PDF Import DPI Auto-Detection (cmake --build)
- src/core/PdfReader.h: Added PdfInfo struct, readPdfInfo(), setImportDpi(), getImportDpi()
- src/core/PdfReader.mm: Added detectEffectiveDpi() to scan embedded images, added DPI storage map
- src/core/ImageLoader.cpp: Use stored import DPI when loading PDF pages
- src/app/PdfImportDialog.h/cpp: New dialog to show detected DPI with dropdown override
- src/app/CMakeLists.txt: Added PdfImportDialog
- src/app/MainWindow.cpp: Show import dialog after PDF selection, store selected DPI

---
2025-12-30 00:27 - Add OCR progress logging (make -j16)
- src/core/filters/ocr/Task.cpp: Add page number log at start of OCR processing

---
2025-12-30 10:45 - Color detection tolerance for aged paper (make -j16)
- LeptonicaDetector.cpp: Changed diffthresh from 18 to 35 to tolerate aged/yellowed paper
  This fixes false "color" detection on old B&W books like "Common Landscape of America"

---
2025-12-30 10:46 - Version bump to 2.0a15 (make -j16)
- version.h.in: 2.0a14 → 2.0a15

---
2025-12-30 10:52 - Show DPI dialog when importing PDF from StartupWindow (make -j16)
- AppController.cpp: Added PdfImportDialog to onImportPdfRequested()
  Now importing PDF from welcome screen also shows the DPI selection dialog

---
2025-12-30 10:58 - Raise color detection threshold further (make -j16)
- LeptonicaDetector.cpp: Changed diffthresh from 35 to 50 for heavily yellowed paper

---
2025-12-30 11:08 - More aggressive B&W detection for old books (make -j16)
- LeptonicaDetector.cpp: Expanded "lights" zone from 150-255 to 130-255
  (grayish/yellowed paper pixels 130-149 now count as "paper" not "midtones")
- LeptonicaDetector.cpp: Raised region midtone threshold from 30% to 40%
  (more tolerant of stained/shadowed areas in old books)

---
2025-12-30 - Color mode thumbnail filters + DPI inheritance (make -j16)
- src/core/filters/output/Settings.h: Added m_defaultDpi with getter/setter for project-level output DPI
- src/core/filters/output/Settings.cpp: Modified getParams() to use default DPI for new pages
- src/app/MainWindow.cpp: Set output default DPI from PDF import DPI
- src/app/MainWindow.ui: Added filterBwBtn, filterGrayBtn, filterColorBtn toggle buttons
- src/app/MainWindow.cpp: Connect filter buttons to refresh thumbnail sequence
- src/app/MainWindow.cpp: Modified currentPageSequence() to filter by color mode

---
2025-12-30 17:10 - Fix thumbnail not updating after output regeneration (make -j8)
- ThumbnailPixmapCache.cpp: Modified recreateThumbnail() to immediately cache the new thumbnail pixmap
  - Previously only removed old item from cache, relying on background loader to re-read
  - Now directly inserts new thumbnail into cache after writing to disk
  - Handles all states: LOADED, LOAD_FAILED, QUEUED - removes old and inserts new
  - IN_PROGRESS still sets needsReload flag (background thread may have stale data)
  - Also handles case where item was not in cache at all - inserts directly

---
2024-12-30 09:05 - Fix thumbnail filename length exceeding 255 char limit (make -j8)
- ThumbnailPixmapCache.cpp: Truncate base name to 180 chars max in getThumbFilePath()
- Bump cache version to v4 to regenerate thumbnails with new shorter names
- Root cause: QTemporaryFile adds 6 chars, pushing 253-char filenames over 255 limit
- Removed debug logging, cleaned up code

---
2025-12-30 10:07 - Output normalization downscale (cmake --build build -j8)
- OutputGenerator.cpp: render PolynomialSurface at capped size, upscale to full size

---
2025-12-30 10:14 - Grayscale fast path for RGB32 (cmake --build build -j8)
- Grayscale.cpp: avoid QImage::pixel in grayscale/histogram loops via scanline access

---
2025-12-30 10:28 - TIFF strip writes (cmake --build build -j8)
- TiffWriter.cpp: write RGB/ARGB via encoded strips to reduce per-scanline overhead

---
2025-12-30 10:35 - Transform identity fast path (cmake --build build -j8)
- Transform.cpp: early return for identity transforms to avoid per-pixel mapping

---
2026-01-01 - Fix fillMarginsInPlace exception during dewarping (make -j8)
- OutputGenerator.cpp: Changed fillMarginsInPlace to clip polygon to image bounds instead of throwing
  exception when content area exceeds image rect. This handles edge cases from dewarping where the
  polygon may slightly extend beyond boundaries. Two overloads fixed (QImage and BinaryImage versions).

---
2026-01-01 - Add L/R suffix for split pages in thumbnail labels (make -j8)
- ThumbnailSequence.cpp: Append "L" or "R" to thumbnail label text for split pages
- StatusBarPanel.cpp: Removed redundant page info display from status bar (now shown in thumbnails)

---
2026-01-02 13:47 - Harden thumbnail write failures (no build)
- src/core/ThumbnailPixmapCache.cpp: ensure thumbnail directory exists before writes
- src/core/ThumbnailPixmapCache.cpp: log and continue caching when disk write fails
- src/core/ThumbnailPixmapCache.cpp: log failed thumbnail writes during background load

---
2026-01-02 13:47 - Add thumbnail filename fallback (no build)
- src/core/ThumbnailPixmapCache.cpp: add shorter fallback filename and retry save
- src/core/ThumbnailPixmapCache.cpp: load from fallback if primary missing

---
2026-01-02 13:47 - Build after thumbnail fallback (make -j8)
- built after ThumbnailPixmapCache fallback change

---
2026-01-02 13:47 - Build after thumbnail breadcrumb logging (make -j8)
- built after ThumbnailPixmapCache failure logs

---
2026-01-12 21:30 - Noted intermittent crash after long runtime
- Crash in worker thread 44 after ~29 hours uptime
- EXC_BAD_ACCESS at invalid address 0x79ed9866a290
- Likely use-after-free or dangling pointer in background processing
- Not reproducible on demand - needs investigation

---
2026-01-18 - Fix OCR thumbnails showing pre-output image
- src/core/filters/ocr/CacheDrivenTask.h: add OutputFileNameGenerator and output::Settings members
- src/core/filters/ocr/CacheDrivenTask.cpp: use output file path for thumbnail instead of source image
- src/core/filters/ocr/Filter.h: update createCacheDrivenTask signature
- src/core/filters/ocr/Filter.cpp: update createCacheDrivenTask implementation
- src/app/MainWindow.cpp: pass OutputFileNameGenerator and output::Settings when creating OCR cache task
- src/core/filters/ocr/CMakeLists.txt: added dewarping to link libraries for DistortionModel.h include
- Build successful

---
2026-01-18 - Add keyboard shortcuts for Output filter color mode and page filters
- src/app/MainWindow.h: add keyPressEvent override
- src/app/MainWindow.cpp: implement c/g/b for color mode, Shift+C/G/B for filter toggles
- README.md: document new keyboard shortcuts
- src/app/CMakeLists.txt: added dewarping include path for Settings.h dependency
- Build successful

---
2026-01-18 - Fix keyboard shortcuts: layout support, finalize stage, focus issues
- src/app/MainWindow.cpp: use event->text() for Dvorak/alternate layouts
- src/app/MainWindow.cpp: add Finalize stage support for c/g/b shortcuts
- src/app/MainWindow.cpp: handle key events in eventFilter for proper focus handling
- Used QShortcut instead of keyPressEvent for proper focus handling
- Build successful - timestamp 20260118.1201

---
2026-01-18 12:15 - Finalize sync fix and pass-through shortcut (make -j8)
- MainWindow.cpp: Updated setColorModeForSelectedPages to update BOTH Output and Finalize settings
- MainWindow.cpp: Added 'p' shortcut for MIXED (pass-through) mode

---
2026-01-18 12:20 - Debug P shortcut (make -j8)
- MainWindow.cpp: Added qDebug output to P shortcut to verify it fires

---
2026-01-18 12:22 - White fill default + P shortcut debug (make -j8)
- ColorCommonOptions.cpp: Changed default fill margins from FILL_BACKGROUND to FILL_WHITE
- MainWindow.cpp: Added qDebug output to P shortcut

---
2026-01-18 12:28 - Fix P shortcut to toggle pass-through (make -j8)
- MainWindow.cpp: Changed 'p' shortcut to toggle pass-through mode instead of MIXED color mode
- MainWindow.cpp: Added #include for OutputProcessingParams.h
- README.md: Updated 'P' shortcut description to "Toggle Pass-through mode"

---
2026-01-18 12:35 - Fix P shortcut stage change bug (make -j8)
- MainWindow.cpp: Moved 'p' handling to keyPressEvent to properly accept the event
- MainWindow.cpp: Removed QShortcut for 'p' (was causing event to fall through)

---
2026-01-18 13:05 - Fix P key fallthrough in all stages (make -j8)
- MainWindow.cpp: Accept 'p' key in both Finalize and Output stages to prevent event fallthrough
- The action only runs in Output stage, but the key is consumed in both

---
2026-01-18 - Fix P key shortcut conflict for pass-through toggle (make -j8)
- MainWindow.ui: Changed actionSwitchFilter6 shortcut from "P" to "F"
- Root cause: QAction shortcuts have higher priority than keyPressEvent, so the Finalize stage shortcut was intercepting 'P' before the pass-through handler could see it
- Now: P = Toggle pass-through mode, F = Switch to Finalize stage

---
2026-01-18 - Add left/right arrow keys for page navigation (make -j8)
- MainWindow.ui: Added actionPrevPageLeft (Left arrow) for Previous Page
- MainWindow.ui: Added actionNextPageRight (Right arrow) for Next Page
- MainWindow.cpp: Connected new actions to goToPrevPage/goToNextPage slots

---
2026-01-18 - Fix brightness/contrast to work with pass-through mode (make -j8)
- OptionsWidget.ui: Moved passThroughCheckBox into commonOptions (Options panel)
- OptionsWidget.ui: Created new adjustmentsPanel CollapsibleGroupBox for brightness/contrast/autoLevels
- OptionsWidget.ui: Updated tooltip to clarify that adjustments still apply with pass-through
- Task.cpp: Added brightness/contrast and auto levels application in pass-through mode
- Task.cpp: Added <algorithm> and <cmath> includes for std::clamp and std::pow

---
2026-01-18 23:25 - Fix crash on quit (make -j8)
- MainWindow.cpp: Release m_tabbedDebugImages unique_ptr instead of deleting
- Bug: Widget was double-freed (unique_ptr + Qt parent-child cleanup)

---
2026-01-18 - Fix missing center detent and tick marks on brightness/contrast sliders (make -j8)
- src/core/filters/output/OptionsWidget.ui: Restored margin-top: 8px stylesheet on brightnessLabel to provide vertical space for CenteredTickSlider tick marks

---
2026-01-18 - Remove debug logging from showPageSizeWarning() (make -j8)
- src/app/MainWindow.cpp: Removed file-based debug logging to /tmp/st_pagesizewarning.log

---
2026-02-01 11:40 - Version 2.0a18 release build (make -j8)
- version.h.in: Bumped VERSION from 2.0a17 to 2.0a18
- Includes: crash-on-quit fix, debug logging removal, slider tick marks fix

---
2026-02-07 - Implement Auto Mode (smart multi-stage processing) (make -j8)
- src/core/filters/deskew/Filter.h: Added public settings() accessor
- src/app/MainWindow.h: Added AutoModeStage enum, m_autoModeStage member, startAutoMode slot, auto mode helper methods
- src/app/MainWindow.ui: Added actionAutoMode action and menu entry in Tools menu
- src/app/MainWindow.cpp: Added includes, action connect, enable/disable, filterResult intercept, startAutoMode, autoModeAdvance, autoAcceptPageSplit, autoSetDeskewZero, autoAcceptContentOutliers, autoAcceptPageSizeOutliers

---
2026-02-07 - Add OCR variant to Auto Mode (make -j8)
- MainWindow.h: Added AUTO_OCR enum value, m_autoModeIncludeOcr member
- MainWindow.cpp: Lambda connects for Alt detection, QShortcut for Ctrl+Shift+O, AUTO_OCR stage in autoModeAdvance, save/restore m_autoModeIncludeOcr in filterResult intercept
- MainWindow.ui: Updated tooltip with OCR shortcut info

---
2026-02-07 - Fix Auto Mode shortcuts and add second menu item (make -j8)
- Changed shortcuts from Ctrl+Shift to Cmd+Shift (Meta+ in Qt = Cmd on macOS)
- Added actionAutoModeOcr as separate visible menu item with Cmd+Shift+O
- Removed QShortcut hack, both actions now proper QActions in .ui

---
2026-02-07 - Release build with Auto Mode feature (cmake Release + make -j8)
- Full release build

---
2026-02-07 - Version bump to 2.0a19, README update for Auto Mode (cmake Release + make -j8)
- version.h.in: 2.0a18 → 2.0a19
- README.md: Added Auto Mode to features, Quick Start, and keyboard shortcuts

---
2026-02-12 - Version bump 2.0a20, fix auto mode save/restore fragility (make -j8)
- version.h.in: 2.0a19 → 2.0a20
- MainWindow.h: Add resetAutoMode param to stopBatchProcessing()
- MainWindow.cpp: Move auto mode reset behind resetAutoMode flag, pass false from filterResult auto mode path

---
2026-03-29 - Add "Apply To..." button for Pass Through option in Output stage
- src/core/filters/output/OptionsWidget.ui: Added applyPassThroughButton next to passThroughCheckBox
- src/core/filters/output/OptionsWidget.h: Added applyPassThroughButtonClicked and applyPassThroughConfirmed slots
- src/core/filters/output/OptionsWidget.cpp: Implemented apply-to-multiple-pages for Pass Through using ApplyColorsDialog pattern

---
2026-03-30 - Fix PDF export failing on large documents (384+ pages) due to libharu memory exhaustion
- src/core/PdfExporter.cpp: Chunked export - splits large documents into 100-page chunks, exports each separately, then merges using CoreGraphics CGPDFContext
- Error was: LibHaru error 0x1050 (HPDF_FAILD_TO_ALLOC_MEM) around page 211, followed by 0x1025 (HPDF_INVALID_DOCUMENT) for all remaining pages

---
2026-03-30 - Reduce PDF chunk size from 100 to 50 pages
- Memory from freed chunks isn't fully reclaimed by OS, causing chunk 3 to fail
- 50 pages keeps each chunk well within memory limits

---
2026-03-30 - Replace libharu PDF exporter with CGPDFContext (macOS native)
- Root cause: libharu accumulates ALL image data in memory before writing. Chunking doesn't help because malloc doesn't return memory to the OS.
- Fix: CGPDFContext writes incrementally per-page via CGContextEndPage(). O(1) memory.
- src/core/PdfExporter.cpp -> PdfExporter.mm (Objective-C++ for CoreText/Foundation)
- src/core/CMakeLists.txt: Updated filename, added CoreText framework, ObjC++ flags
- New exportWithCoreGraphics() function:
  - B&W: 1-bit CGImage from raw mono data
  - Grayscale: 8-bit CGImage (Flate compressed by CG)
  - Color/JPEG gray: CGImageCreateWithJPEGDataProvider for JPEG passthrough
  - OCR: kCGTextInvisible + CoreText CTLineDraw (invisible but searchable)
- Removed all libharu chunking code (exportPdfChunk, mergePdfChunks, exportWithLibHaru)

---
2026-03-31 - Fix pass-through to auto-apply to all selected pages
- src/core/filters/output/OptionsWidget.cpp: passThroughToggled() now applies to all selected pages automatically (like colorModeChanged does)
- Removed the "Apply To..." button and dialog approach - checkbox itself handles multi-page
- src/core/filters/output/OptionsWidget.h: Removed applyPassThroughButtonClicked/applyPassThroughConfirmed slots
- src/core/filters/output/OptionsWidget.ui: Removed applyPassThroughButton, reverted to simple checkbox

---
2026-04-02 14:00 - Version 2.0a21 release build (cmake --build -j)
- version.h.in: 2.0a20 → 2.0a21 (already done)
- README.md: Updated version to 2.0a21
- src/core/PdfExporter.cpp → PdfExporter.mm: Replaced libharu with native CoreGraphics CGPDFContext (fixes OOM on large PDFs)
- src/core/CMakeLists.txt: Updated for .mm file, added CoreText framework
- src/core/filters/output/OptionsWidget.cpp: Pass-through auto-applies to all selected pages
- src/core/filters/output/OptionsWidget.ui: Updated tooltip for pass-through

---
2026-04-03 - Fix pass-through mode ignoring page split/deskew transforms (make -j)
- src/core/filters/output/Task.cpp: Replace origImage.scaled() with imageproc::transform() using newXform
  - Bug: pass-through used raw origImage (full spread) and just scaled it, ignoring all geometric corrections
  - Fix: apply accumulated transform (page split crop, rotation, deskew) via imageproc::transform()
  - Added #include <Transform.h>

---
2026-04-06 - Fix pass-through page-split clipping (make -j)
- src/core/filters/output/Task.cpp: Clip transform output to resultingPreCropArea
  - Bug: resultingRect() includes margins beyond page-split boundary, so other page bleeds in
  - Fix: intersect output rect with preCropArea bounding rect, transform into that, paste onto white canvas
  - Added #include <QPainter>

---
2026-04-06 - Grey out inapplicable controls in pass-through mode (make -j)
- src/core/filters/output/OptionsWidget.cpp: Disable fill margins, fill offcut, equalize illumination,
  white balance, smoothing, thresholds, despeckle, splitting, picture shape, dewarping controls when
  pass-through is checked. Brightness/contrast/auto-levels and DPI remain enabled.

---
2026-04-07 - Version 2.0a22 release build (cmake --build -j)
- version.h.in: 2.0a21 → 2.0a22
- README.md: Updated version to 2.0a22
- src/core/filters/output/Task.cpp: Fix pass-through ignoring page split/deskew transforms;
  clip output to pre-crop area so other page doesn't bleed into margins
- src/core/filters/output/OptionsWidget.cpp: Grey out inapplicable controls in pass-through mode;
  fix controls staying greyed out when pass-through is toggled off

---
2026-04-07 - Step 1: Add page_box filter skeleton (cmake --build -j)
- Created src/core/filters/page_box/ with: Filter, Task, CacheDrivenTask, Settings, Params, Dependencies, Thumbnail, OptionsWidget, CMakeLists
- Updated StageSequence.h/cpp to include page_box between deskew and select_content
- Updated MainWindow.cpp createCompositeTask/createCompositeCacheDrivenTask for page_box
- Updated deskew filter to chain to page_box instead of select_content
- Updated src/core/CMakeLists.txt with page_box subdirectory and library

---
2026-04-07 - Step 2: Move page box detection into page_box filter (cmake --build -j)
- Copied PageFinder.h/cpp to page_box namespace
- Updated page_box::Task to use PageFinder for actual page box detection
- Added page_box::Settings connection to select_content::Filter via setPageBoxSettings()
- Updated select_content::Task to read pageRect from page_box::Settings instead of detecting it
- Updated StageSequence to connect page_box settings to select_content

---
2026-04-07 - Step 3: Page Box UI (ImageView, OptionsWidget with .ui) (cmake --build -j)
- Created page_box::ImageView with page rect manipulation only (corners, edges, whole-box drag)
- Created page_box::OptionsWidget with Disable/Auto/Manual modes, Fine Tune, Width/Height spinboxes
- Created OptionsWidget.ui form
- Updated Task UiUpdater to use new ImageView with signal/slot connections
- Updated CMakeLists for new files

---
2026-04-07 - Step 4: Simplify select_content (remove page box UI) (cmake --build -j)
- Removed pageBoxGroup from OptionsWidget.ui
- Removed page box signals/slots/methods from OptionsWidget.h/cpp
- Removed page box signal connections from Task.cpp UiUpdater
- Page rect now shown as read-only context in ImageView (not editable)

---
2026-04-07 - Step 6: Auto mode integration (cmake --build -j)
- Added AUTO_PAGE_BOX to AutoModeStage enum in MainWindow.h
- Added AUTO_PAGE_BOX case in autoModeAdvance() between DESKEW and SELECT_CONTENT

---
2026-04-07 - Step 7: Polish - invalidation, README/CLAUDE.md, lint fix (cmake --build -j)
- Fixed pageOrderOptions override warning in page_box::Filter.h
- Added page rect change detection in select_content::Task to force content re-detection
- Updated README.md: 10-stage table, new Page Box section, renumbered stages 6-10
- Updated CLAUDE.md: 10 stages, processing flow, PdfExporter.mm reference

---
2026-04-07 - Add Page Box batch summary dialog (cmake --build -j)
- Created PageBoxSummaryDialog.h/cpp: shows page width outliers after Page Box batch processing
- Added showPageBoxSummary() to MainWindow with jump-to-page and disable-page-box actions
- Added wasPageBoxBatch flag in filterResult() batch completion handling
- Added to app CMakeLists.txt

---
2026-04-07 - Version 2.0a23 release build (cmake --build -j)
- version.h.in: 2.0a22 → 2.0a23
- README.md: Updated version to 2.0a23, 10-stage pipeline, new Page Box section
- CLAUDE.md: Updated to 10 stages
- NEW: Page Box stage (stage 4) - separate page boundary detection from content detection
  - page_box filter with PageFinder, ImageView, OptionsWidget, Settings, Params, Dependencies
  - Disable/Auto/Manual modes, Fine Tune Corners, Width/Height spinboxes
  - Sort by page width, height, deviation
  - Batch summary dialog showing page size outliers
- CHANGED: Select Content (now stage 5) - content box only, reads page rect from Page Box
  - Removed page box UI controls
  - Page rect shown as read-only context
  - Content re-detection triggered when page box changes
- Auto mode updated for 10 stages
- Project file backward compatibility (reads page box data from old <select-content> tag)

---
2026-04-08 - Version 2.0a24 release build (cmake --build -j)
- version.h.in: 2.0a23 → 2.0a24
- README.md: Updated version to 2.0a24, corrected 9-stage to 10-stage
- cmake/GenerateVersionH.cmake: Fix BUILD_TIMESTAMP showing literal @BUILD_TIMESTAMP@ (CMP0053 policy)
- NEW: src/core/filters/page_box/ApplyDialog.h/cpp/ui: Proper Apply To dialog with all/followers/every-other/selected options
- src/core/filters/page_box/OptionsWidget.cpp: Replace stub showApplyToDialog() with proper dialog launch
- src/core/filters/page_box/OptionsWidget.h: Add applySelection slot
- src/core/filters/page_box/CMakeLists.txt: Add ApplyDialog files
- Bug fix: Page Box "Apply to..." button was not clickable (stub silently returned when only 1 page selected)
- Build fixes for updated toolchain:
  - src/core/CMakeLists.txt: Fix leptonica include path for homebrew
  - src/dewarping/Curve.cpp: Fix QIODeviceBase → QIODevice + add include
  - src/dewarping/DistortionModelBuilder.cpp: Remove stale Qt5 #ifdef blocks

---
2026-04-10 - SpineDarknessFinder fallback for two-page split detection (make -j16)
- New file src/core/filters/page_split/SpineDarknessFinder.h/.cpp: scans columns within ±10% of horizontal center on the existing 100dpi downscaled gray image, with a small ±2° tilt sweep, and returns the column with the highest mean darkness if it passes prominence + dark-row-fraction quality gates. Acts as fallback only.
- src/core/filters/page_split/PageLayoutEstimator.cpp: now always captures grayDownscaled/outToDownscaled from VertLineFinder (previously only for numPages==1). In tryCutAtFoldingLine numPages==2 branch, calls SpineDarknessFinder when VertLineFinder produces no usable lines, before falling through to autoDetectTwoPageLayout. Added SpineDarknessFinder.h include.
- src/core/filters/page_split/CMakeLists.txt: added SpineDarknessFinder.cpp/.h to sources.
- Branch: spine-darkness-fallback (off main).

---
2026-04-10 - Page split detection: relax Vision short-circuit + central validation + center fallback (make -j16)
- src/core/filters/page_split/PageLayoutEstimator.cpp:
  - Added kCentralLo=0.20 / kCentralHi=0.80 splitFractionIsCentral lambda. Vision-returned splits are only accepted if their normalized X is in this central window — catches the page 15 case where Vision returned a split jammed near the left edge.
  - Forced TWO_PAGES Vision branch: previously trusted *any* positive splitLineX from Vision. Now only uses it if central; otherwise falls through to VertLineFinder + SpineDarknessFinder + center fallback.
  - "Vision saw text but no clear spread" branch: previously always returned single page. Now distinguishes (a) symmetric two-column single page (still short-circuit) from (b) asymmetric spread where one side has ≤25% the text regions of the other AND numPages==2 (fall through). Fixes pages 2, 4 (chapter title / title spread).
  - Two-page fallback chain: after SpineDarknessFinder also fails, return a geometric center split rather than null. Fixes page 11 (two-photo spread, no detectable spine).
- Branch: spine-darkness-fallback.

---
2026-04-10 - SpineDarknessFinder line clipping fix (make -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp: fixed correctness bug in the final line-clipping step. The pointAtY lambda captured spineVirt by reference; the first setP1() call mutated p1 before the second setP2() call read it back, producing wrong-slope geometry. Snapshot endpoints into origP1/origP2 before mutating.

---
2026-04-10 - Asymmetric threshold fix + tagged debug logging (make -j16)
- src/core/filters/page_split/PageLayoutEstimator.cpp:
  - Removed maxCount >= 4 floor on the asymmetric Vision check. Was preventing chapter title spreads (which have only 1-2 text regions on the right) from falling through. Now (max>=1, min*4<=max, min<max) qualifies.
  - Tagged the relevant qDebug lines with [SPINE-FALLBACK] so we can grep them and see exactly which path each page took.

---
2026-04-10 - Stop trying to be clever about asymmetry — trust geometry (make -j16)
- src/core/filters/page_split/PageLayoutEstimator.cpp: replaced the asymmetric-text heuristic in the Vision-text-uncertain branch with a simple geometry-trumps-Vision rule. When numPages==2 (geometry advise concluded "spread"), always fall through to traditional detection regardless of how Vision's text regions are distributed. Only short-circuit to single page when numPages==1 (geometry agrees). This is the fix that should finally split pages 2, 4, and similar chapter-title / asymmetric spreads.

---
2026-04-10 - Force re-detection of stale auto SINGLE_PAGE_UNCUT cache (make -j16)
- src/core/filters/page_split/Task.cpp: added staleAutoSingleUncut bool in Task::process. When the cached Params are non-null, the per-image layoutType is AUTO_LAYOUT_TYPE, and the cached pageLayout type is SINGLE_PAGE_UNCUT, the if at line 101 now also takes the re-estimate branch. This is the actual fix for pages 2, 4, 11 in the user's project — those pages have cached Apr-8 single-uncut Params with dependencies that match exactly (size 3308x2479, rotation 0, layoutType auto-detect), so Dependencies::compatibleWith returned true and the estimator was never being called regardless of how I edited PageLayoutEstimator.cpp. Manually-pinned singles and cached two-page layouts are unaffected. Plan: ~/.claude/plans/misty-fluttering-panda.md

---
2026-04-10 - Vision split refinement via SpineDarknessFinder (make -j16)
- src/core/filters/page_split/VertLineFinder.h/.cpp: extracted buildGrayDownscaled() public static helper. findLines() now calls it internally so the two paths produce byte-identical gray images.
- src/core/filters/page_split/SpineDarknessFinder.h/.cpp: added centerXOverride parameter to findSpine(). When finite, the column search window centers on this virtual-X coordinate instead of virtualImageRect.center().x(). Defaults to NaN (use geometric center). All quality gates unchanged.
- src/core/filters/page_split/PageLayoutEstimator.cpp: in the high-confidence Vision branch, after computing visionSplitX, call buildGrayDownscaled + SpineDarknessFinder::findSpine with a narrow ±6% window centered on Vision's position. If a strong dark column is found nearby, use the refined spine; otherwise use Vision's exact result. Tagged with [SPINE-REFINE] for debugging. This addresses the mode-B issue (pages 5/7/9/10/12/15) where Vision returns confident-but-skewed splits.

---
2026-04-10 - Widen Vision-refinement window + relax SpineDarknessFinder gates (make -j16)
- src/core/filters/page_split/PageLayoutEstimator.cpp: refinement window widened from ±6% to ±15% of width. The gutter can be 10-15% from where Vision/VertLineFinder lands when there's a strong vertical edge inside an illustration on the spread (e.g. a column inside a carved frieze on page 9). The dark-row-fraction and prominence gates inside SpineDarknessFinder discriminate gutters from image edges, so a wider window is safe.
- src/core/filters/page_split/SpineDarknessFinder.cpp: kMinDarkRowFraction 0.70 -> 0.55 (some real gutters break across captions/page numbers, and image edges still typically span <40% of height); kPerRowDarknessThreshold 30 -> 18 (soft gutter shadows on bright pages were being missed).

---
2026-04-10 - Local neighbor contrast gate in SpineDarknessFinder (make -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp: added meanDarknessOfStripe() helper and a new "valley" gate after the existing prominence check. The chosen column must be at least kMinNeighborContrast (=14) darker than min(leftNeighborMean, rightNeighborMean), where the neighbor stripes are sampled 4-8 px off the candidate's center on each side. Distinguishes a real gutter (bright page content on at least one side) from a column inside a uniformly dark photo (both sides also dark). Fixes false positives on pages 23 (jewelry plates) and 32 (cathedral interior) without re-breaking page 9.

---
2026-04-10 - "Reset all auto pages" button in page_split OptionsWidget (make -j16)
- src/core/filters/page_split/Settings.h/.cpp: added clearAllAutoParams() method. Iterates m_perPageRecords and clears Params for every record whose effective layoutType (per-image override OR project default) is AUTO_LAYOUT_TYPE. Erases now-empty records. Manually-pinned single/two-pages layouts are left intact.
- src/core/filters/page_split/OptionsWidget.ui: new "Reset all auto pages" QPushButton (resetAllAutoBtn) with tooltip, placed below the Change... button in the Page Layout group.
- src/core/filters/page_split/OptionsWidget.h/.cpp: new resetAllAutoPages() slot. Calls Settings::clearAllAutoParams(), then emits invalidateAllThumbnails() and reloadRequested() so MainWindow refreshes the thumb strip and reprocesses the current page. Wired to resetAllAutoBtn::clicked() in setupUiConnections().
- This is the user-facing answer to "the reset isn't working" — clicking it forces every auto-detected page to re-run with the current PageLayoutEstimator code.

---
2026-04-10 - SpineDarknessFinder: paper-side gate + Vision-refinement leash (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Added kMaxPaperNeighborDarkness=55 constant and a new absolute paper-side
  gate in findSpine(). The existing relative neighbor-contrast gate
  (bestMean - min(left,right) >= 14) was being trivially satisfied by
  candidate columns deep inside dark photographs (where both neighbor
  stripes are dark but still slightly lighter than the candidate). The
  new gate additionally requires the lighter of the two neighbor stripes
  to look like actual paper (mean darkness <= 55), which is the property
  unique to a real gutter.
- src/core/filters/page_split/PageLayoutEstimator.cpp:
  Added a 5%-of-page-width "leash" to the Vision-anchored refinement
  pass. If the SpineDarknessFinder refinement returns a position more
  than 5% of virtualImageRect.width() away from Vision's splitLineX,
  discard it and fall through to the existing Vision-only path. The
  refinement is meant for sub-percent precision; anything bigger means
  the search wandered into a neighboring photograph.
- Targets pages 9, 32, 35, 67, 69, 71 in *The Lost Gods of England*
  sample project, which were splitting through dark photographic plates
  in the previous build.

---
2026-04-10 - SpineDarknessFinder: peak-prominence gate (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Added kMinPeakProminence=14 constant and a new "local-maximum" gate in
  findSpine(). Diagnosis from /tmp/st-spectre.log on the user's project:
  the previous paper-side gate was rejecting ZERO candidates, while the
  refinement was happily returning columns with meanDarkness=169..228
  (real gutter shadows are 50..100; anything above ~150 is almost
  certainly inside a photographic region). The chosen columns were all
  sitting at the LEFT EDGE of a dark photograph, where the left neighbor
  stripe is paper (text-page margin) and the right neighbor is inside
  the dark image. The previous gate only required the *lighter* side to
  be paper, so photo edges passed.

  The new gate requires the candidate to be a local maximum of darkness:
  bestMean - max(leftNeighbor, rightNeighbor) >= 14. A real spine shadow
  is a thin dark line surrounded by paper on BOTH sides, so the candidate
  is much darker than both neighbors. A photo edge is a transition where
  the candidate sits mid-gradient and is *brighter* than the dark side.
- Targets pages 32, 48 (and similar) in The Lost Gods of England, where
  the previous build was still landing the split inside the dark photo.

---
2026-04-10 - SpineDarknessFinder: fix one-sided peak gate bug (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Diagnostic logging revealed that meanDarknessOfStripe() had a bogus
  early-exit check `if (stripeXLo < 0 || stripeXHi >= w) return 0.0;`
  that fired immediately for every LEFT neighbor call, because stripeXLo
  is a *relative offset* from xCenter (negative for the left neighbor)
  and was being compared as if it were an absolute coordinate. The
  function silently returned 0 every time, forcing the peak-prominence
  gate to be one-sided: max(leftNbr, rightNbr) was effectively
  max(0, rightNbr) = rightNbr. Real photo-edge candidates with paper
  on the left (where the bug was) were getting through because only
  the right side of the candidate was being checked.
  Removed the bogus early-exit; the per-row bounds check inside the
  sampling loop already handles real out-of-image cases correctly.
- This bug had been present in every previous build since
  meanDarknessOfStripe was added; the peak gate was structurally
  incapable of catching photo-edge candidates until now.

---
2026-04-10 - PageLayoutEstimator: widen refinement leash 5% → 10% (cmake --build build -j16)
- src/core/filters/page_split/PageLayoutEstimator.cpp:
  Diagnostic logs from /tmp/st-spectre4.log on the user's project show
  page 67 generating a refinement candidate with a near-perfect quality
  signature: meanDarkness=174.7, leftNbr=42, rightNbr=61, peakProm=113.
  All three signals say "real spine shadow with paper on both sides".
  But it sat 280 px (~8.5%) left of Vision's anchor at 1654 and the
  5% leash discarded it, falling back to Vision's wrong split.
  The original 5% leash was a safety net for when the SpineDarknessFinder
  gates had the leftNbr=0 bug and were one-sided. With the gates now
  properly two-sided (paper-side absolute + peak-prominence on both
  neighbors), refinements that pass the gates are reliable, and we can
  trust the search across a wider radius without inviting photo-edge
  false positives. 10% lets pages like 67 with Vision off-center
  through, while still discarding refinements that wander >10% off.

---
2026-04-10 - SpineDarknessFinder: top-N candidate diagnostic logging (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Pages 14, 21, 55, 63, 92, 95 in The Lost Gods of England are still
  landing the split too far right of the actual gutter on asymmetric
  spreads (image on left page, text on right page). The accepted spine
  log entries show meanDarkness ~120-150 with leftNbr ~70-90 (not paper,
  not photo) and rightNbr ~20 (paper) — patterns that pass all current
  gates but are clearly the wrong column.
  This change does NOT modify behavior. It tracks the top 8 candidate
  columns by meanDarkness during the search loop (instead of just the
  global maximum), and after the chosen winner is logged, dumps all 8
  candidates with their virtual-x position, mean, drf, leftNbr, rightNbr,
  and peakProm. The winner (topCandidates[0]) is still selected by the
  same criterion (max bestMean) so no current page should regress.
  Goal: see whether the real gutter is actually one of the runners-up
  on the broken pages. If yes → switch the selection rule to prefer
  symmetric (high-peakProm) candidates over slightly-darker asymmetric
  ones. If no → the underlying signal is the problem and we need a
  different approach.

---
2026-04-10 - SpineDarknessFinder: spatial-NMS top-N diagnostic (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Replaces the per-tilt "top 8 by meanDarkness" tracking with a post-loop
  spatial-NMS pass over the existing zeroTiltMeans vector. The previous
  diagnostic was useless for finding alternatives — on every page, the
  top 8 collapsed onto adjacent columns of the same local feature
  (typically within ±10 px of each other). NMS picks the global max,
  masks out a ±25-px region around it, finds the next-highest column
  outside the mask, and repeats — yielding up to 8 spatially-distinct
  candidates that represent truly different features in the search window.
  No selection-rule change: the chosen spine is still the global max
  from the existing tilted sweep. This is logging only — no page should
  regress.
- Goal: on the next test run, see whether the actual gutter on pages
  67, 92, 95 appears as one of the spatially-separated runners-up.
  - If yes → next round switches the selection rule to prefer it.
  - If no → next round needs a different signal (Vision-anchor bias,
    narrower window, or geometric-center fallback).
- New <numeric> include for std::iota.

---
2026-04-10 - SpineDarknessFinder: anchor re-pick selection rule (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Implements the new selection rule based on the previous round's NMS
  diagnostic findings. From /tmp/st-spectre9.log, page 67 produced
  TWO viable gate-passing NMS candidates: the dark relief edge at
  xVirt=1371 (mean=172, peakProm=115, distFromAnchor=283) which won
  by raw meanDarkness, and the actual gutter at xVirt=1611 (mean=97,
  peakProm=23, distFromAnchor=43) which was the runner-up. The new
  rule picks the closest-to-anchor among gate-passing NMS candidates,
  flipping page 67 from the relief to the real gutter.
- Refactor: gate sequence factored into an `evaluateColumn` lambda so
  it can be applied to both the global-max winner from the tilt sweep
  AND each spatially-distinct NMS candidate. The lambda matches the
  existing gate sequence exactly (kMinMeanDarkness, drf, prominence,
  paper-side, neighbor-contrast, peak-prominence).
- After gates pass on global max, build a `viable` list:
  global max + each NMS candidate that passes the gates and doesn't
  spatially overlap the global max. Pick the one closest to centerXDs
  (Vision's anchor when supplied, geometric center otherwise).
- If anchor pick != global max: re-run a tilt sub-sweep at the picked
  column to recover its best xTop/xBottom, recompute neighbors, log
  an `[ANCHOR-PICK]` override line, then update bestMean/bestXTop/etc
  before line construction.
- Cleanly-resolved pages (single viable candidate) are unchanged
  because the viable list has size 1 and "closest to anchor" trivially
  picks it. Pages 92/95 (no viable runner-up) are also unchanged. Only
  pages with multiple viable candidates and a closer-to-anchor runner-up
  flip — the explicit target is page 67's relief→gutter flip.
- Diagnostic dump kept; runners-up that fail the gates get a
  "[gate-fail]" annotation, viable runners-up get "[viable]", and the
  final pick is "[CHOSEN]".

---
2026-04-10 - SpineDarknessFinder: anchor-pick override guard (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Test of the previous build's anchor-pick rule revealed two override
  events on the user's project. One was correct (page 67's relief edge
  → real gutter, big improvement). The other was a regression (page 65,
  where the photo-edge gutter at xVirt=1449 with left=139 right=4 got
  wrongly flipped to a text-edge at xVirt=1608 with left=63 right=15).
  
  Diagnosis: the two cases need opposite handling. Page 67's wrong
  global-max is a "thin dark line inside a relief image" — both
  neighbors are paper-like (left=32, right=58). Page 65's correct
  global-max is a "photo edge with paper margin on one side, photo
  interior on the other" — strongly asymmetric neighbors (left=139,
  right=4). The anchor-pick override should only fire on the former.
  
  Adds kMaxBothPaperDarkness=65 guard: the override only fires when
  the global max has BOTH leftNbr and rightNbr ≤ 65 (i.e. it's a
  "suspiciously clean isolated peak"). Page 67 passes (max=58 ≤ 65,
  override fires, gutter at xVirt=1611). Page 65 fails (max=139 > 65,
  override suppressed, photo-edge at xVirt=1449 retained).
  
  Logs `[ANCHOR-PICK] override SUPPRESSED` on the suppressed cases so
  we can see which pages were saved by the guard.

---
2026-04-10 - SpineDarknessFinder: revert override guard (cmake --build build -j16)
- src/core/filters/page_split/SpineDarknessFinder.cpp:
  Test of the previous build's override guard (kMaxBothPaperDarkness=65)
  showed it broke page 95. Page 95 (skeleton photo on left, text on right)
  has its global max at xVirt=1449 with left=139 right=4 — asymmetric —
  and the previous build with the guard skipped the override and stuck
  at xVirt 1449 ("bad again"). Without the guard, the override fires
  and flips to xVirt 1608 ("100%" per the user in the previous test).
  
  The "clean isolated peak vs photo edge" theory I used to motivate the
  guard turned out wrong: the photo-edge case ALSO benefits from the
  override on this book. Removing the guard restores page 95's correct
  position. Page 67 still flips correctly (relief feature is a clean
  isolated peak with closer-to-anchor runner-up). Page 65 might revert
  to "not great" — accept that tradeoff for now since the user judged
  95 the more important fix.

---
2026-04-10 - RELEASE 2.0a25: anchor re-pick selection rule shipped (cmake --build build -j16)
- version.h.in: 2.0a24 → 2.0a25
- README.md: version line updated
- Ships the anchor-pick selection rule in SpineDarknessFinder. Auto-detection
  now correctly handles asymmetric image-on-one-side / text-on-the-other
  spreads on test project The Lost Gods of England:
  - page 67 (sarcophagus relief): flips relief edge → real gutter
  - pages 92, 95: now correctly placed at the gutter, were previously
    landing in the right page text body
  - page 65 (text-left/wagon-right): still requires manual nudge — no
    spine signal at the actual binding fold (the wagon photo's left
    edge is what the algorithm finds, ~50 px right of the visible
    binding strip). Documented in this build log as a known limit.
- The full mechanism: gates → spatially-separated NMS top-N → among
  gate-passing viable candidates, pick the one closest to Vision's
  anchor (or geometric center). Override fires unconditionally — the
  earlier "clean isolated peak" guard regressed page 95 and was
  removed.
