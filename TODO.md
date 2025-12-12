# ScanTailor Advanced - Pending Tasks

Current version: 1.1.0a74

## ~~1. Fix App Icon for macOS~~ ✓ DONE

## ~~2. Add New Filter Stage After Margins~~ ✓ DONE

## ~~3. Simplify Output Stage to Export-Only~~ ✓ DONE

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

## ~~5. Margins Filter Requires Double-Click~~ ✓ DONE

## 6. Thumbnail Panel Not Refreshing After Output

Thumbnails don't always update after processing a page in Output filter.

**Symptom:** After processing a page, the thumbnail panel shows old/stale thumbnail

**Root cause:** Race conditions in `ThumbnailPixmapCache::recreateThumbnail()`:
- If a thumbnail load is IN_PROGRESS when `recreateThumbnail` runs, the old pixmap may be returned
- The cache invalidation doesn't properly synchronize with async loading

**Workarounds:**
- Switch to another filter and back
- Scroll thumbnail panel out of view and back
- Close/reopen project

**Key files:**
- `src/core/ThumbnailPixmapCache.cpp` - `recreateThumbnail()` has race condition at line 507-514
- `src/core/ThumbnailBase.cpp` - `handleLoadResult()` triggers update
- `src/app/ThumbnailSequence.cpp` - `invalidateThumbnailImpl()`

**Solution options:**
- Add explicit cache invalidation with retry logic
- Force synchronous load after `recreateThumbnail`
- Add version/timestamp to cache keys to detect stale entries

## 7. Metal Dilation Crash (Disabled)

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
