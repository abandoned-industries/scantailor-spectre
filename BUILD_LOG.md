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
