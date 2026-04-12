# TODO

## Current Task

**Re-enable Metal GPU acceleration for concurrent page processing**

## Problem Summary

Metal GPU acceleration is currently disabled due to two issues:
1. **Gaussian Blur crash** (`MetalGaussBlur.mm:56-60`): GPU resources are purged when app is backgrounded, causing crashes on resume
2. **Morphology dilation bug** (`Morphology.cpp:692-709`): Produces incorrect results (`dilated > original`), causing assertion failures in `mokjiThreshold`

Additionally, `MetalMorphology.mm` lacks thread safety protections that `MetalGaussBlur.mm` has (no dispatch queue serialization, no autoreleasepool).

---

## Implementation Plan

### Phase 1: App Lifecycle Monitoring

**Goal**: Detect when app is backgrounded/foregrounded to pause GPU operations

1. Create `MetalLifecycleObserver` class (Objective-C++) in `src/acceleration/`
   - Subscribe to `NSApplicationDidResignActiveNotification` and `NSApplicationDidBecomeActiveNotification`
   - Expose atomic `bool isAppActive()` function
   - On resign: set flag, wait for in-flight Metal commands to complete
   - On activate: set flag, allow new GPU work

2. Integrate with `MetalContext`:
   - Add `isAppActive()` check before any GPU dispatch
   - Return early (fall through to CPU) if app is backgrounded
   - Use `std::atomic<bool>` for lock-free checking in hot path

### Phase 2: Metal Resource Resilience

**Goal**: Handle GPU resource loss gracefully

1. Add command buffer error handling in `MetalGaussBlur.mm` and `MetalMorphology.mm`:
   - Check `commandBuffer.status` after `waitUntilCompleted`
   - If `MTLCommandBufferStatusError`, log and return false (trigger CPU fallback)
   - Check `commandBuffer.error.code` for `MTLCommandBufferErrorDeviceRemoved` or `MTLCommandBufferErrorNotPermitted`

2. Add texture/buffer creation validation:
   - Check return values from `newTextureWithDescriptor:` and `newBufferWithBytes:`
   - Return false on allocation failure (GPU memory pressure)

3. Consider lazy resource recreation:
   - Cache pipeline states already exists in `MetalContext`
   - Add ability to invalidate and recreate device/queue if needed (nuclear option)

### Phase 3: Thread Safety for MetalMorphology

**Goal**: Match MetalGaussBlur's thread safety model

1. Add serial dispatch queue to `MetalMorphology.mm`:
   ```objc
   static dispatch_queue_t getMorphologyQueue() {
       static dispatch_queue_t queue = dispatch_queue_create(
           "com.scantailor.metalmorphology", DISPATCH_QUEUE_SERIAL);
       return queue;
   }
   ```

2. Wrap all GPU operations in `dispatch_sync(getMorphologyQueue(), ^{ @autoreleasepool { ... } })`

3. Ensure texture/buffer lifecycle is contained within autoreleasepool

### Phase 4: Fix Dilation Bug

**Goal**: Debug and fix the incorrect dilation results

1. Create test harness:
   - Write unit test with known input/output for dilation
   - Compare Metal vs CPU results pixel-by-pixel
   - Log divergence locations

2. Potential root causes to investigate:
   - Shader border handling (`borderValue` parameter)
   - In-place algorithm issue (source modified before read)
   - Thread synchronization between horizontal/vertical passes
   - Integer overflow in index calculations

3. If shader is correct, issue is likely the host-side algorithm:
   - Check if intermediate texture is being reused incorrectly
   - Ensure proper barriers between passes

### Phase 5: Gradual Rollout

**Goal**: Re-enable safely with opt-out capability

1. Add user preference in Settings:
   - "Use GPU acceleration" checkbox (default: ON)
   - Stored in QSettings

2. Re-enable in stages:
   - First: Gaussian blur only (already has better safety)
   - Test extensively with batch processing
   - Then: Erosion (currently not disabled, just needs lifecycle check)
   - Finally: Dilation (after bug is fixed)

3. Add telemetry/logging:
   - Log GPU vs CPU path taken
   - Log any fallback triggers (app background, allocation failure)

### Phase 6: Testing & Validation

1. Manual testing scenarios:
   - Start batch process, immediately Cmd+Tab away
   - Process large project (100+ pages) with GPU enabled
   - Force low memory conditions (open other GPU-heavy apps)
   - Test on various macOS versions (11, 12, 13, 14, 15)

2. Performance comparison:
   - Benchmark GPU vs CPU for Gaussian blur on typical scans
   - Measure if GPU overhead is worth it for small images
   - Consider raising `MIN_GPU_DIMENSION` threshold

---

## Acceptance Criteria

- [ ] App can be backgrounded during batch processing without crash
- [ ] GPU operations fall back to CPU gracefully on any error
- [ ] Dilation produces identical results to CPU implementation
- [ ] User can disable GPU acceleration in preferences
- [ ] No memory leaks during long batch operations
- [ ] Performance improvement measurable on large blur operations

---

## Key Files

| File | Purpose |
|------|---------|
| `src/acceleration/MetalContext.mm` | Device/queue/pipeline management |
| `src/acceleration/MetalGaussBlur.mm` | Blur - has safety, disabled |
| `src/acceleration/MetalMorphology.mm` | Erode/dilate - missing safety |
| `src/imageproc/GaussBlur.cpp:82-115` | CPU fallback integration |
| `src/imageproc/Morphology.cpp:692-770` | CPU fallback, dilation disabled |
| `src/acceleration/shaders/*.metal` | GPU kernels |

## Notes

- CPU fallback is "fast enough for most use cases" per existing comment
- Metal works fine when app stays in foreground; issue is lifecycle only
- Qt6 has `QGuiApplication::applicationStateChanged` signal we could use instead of NSNotifications

---

## Additional Apple Silicon Optimizations

Target: macOS 11+, Apple Silicon only (M1/M2/M3/M4)

### Already Implemented
- ✅ `MTLStorageModeShared` - unified memory, zero-copy GPU access
- ✅ Configurable thread pool size via settings
- ✅ IIR filter for Gaussian blur - O(1) per-pixel regardless of radius

### Phase 7: Accelerate Framework for CPU Fallback

**Goal**: When GPU unavailable, use Apple's SIMD-optimized CPU routines

1. Replace scalar CPU blur with vImage:
   ```cpp
   #include <Accelerate/Accelerate.h>
   vImageConvolve_Planar8(&src, &dst, NULL, 0, 0, kernel, kW, kH,
                          0, kvImageEdgeExtend);
   ```

2. Replace scalar morphology with vImage:
   - `vImageDilate_Planar8()` - optimized dilation
   - `vImageErode_Planar8()` - optimized erosion
   - Uses NEON SIMD on Apple Silicon

3. Benchmark vImage vs current CPU vs Metal:
   - vImage may be faster than Metal for small images (no GPU dispatch overhead)
   - Could set smarter `MIN_GPU_DIMENSION` threshold based on profiling

### Phase 8: Parallel Row Processing

**Goal**: Use GCD for row-parallel CPU operations

1. Replace sequential row loops with `dispatch_apply`:
   ```cpp
   dispatch_apply(height, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
       ^(size_t row) {
           // Process row independently
       });
   ```

2. Good candidates for parallelization:
   - `GaussBlur.cpp` row processing (IIR is sequential within row, but rows are independent)
   - `Morphology.cpp` when CPU fallback is used
   - `Binarize.cpp` Sauvola algorithm
   - `WhiteBalance.cpp` sampling and adjustment

3. Use `QOS_CLASS_USER_INITIATED` for proper scheduler priority

### Phase 9: Async GPU Pipeline

**Goal**: Overlap GPU work across pages

1. Instead of `waitUntilCompleted` blocking:
   - Submit command buffer for page N
   - Start CPU prep work for page N+1
   - Wait for page N completion only when result is needed

2. Use `MTLSharedEvent` for signaling:
   ```objc
   id<MTLSharedEvent> event = [device newSharedEvent];
   [commandBuffer encodeSignalEvent:event value:pageNumber];
   // Later: check event.signaledValue >= pageNumber
   ```

3. Double-buffer textures to avoid stalls:
   - While GPU processes page N in texture A
   - CPU uploads page N+1 to texture B

### Phase 10: Memory Optimization

**Goal**: Reduce memory pressure during large batch operations

1. Memory-map large TIFFs instead of loading entirely:
   - libtiff supports memory-mapped access
   - Process in tiles/strips rather than full image
   - Critical for 600dpi scans (can be 100MB+ per page)

2. Image pool/recycling:
   - Reuse `GrayImage` allocations between pages
   - Avoid malloc/free churn during batch processing

3. Reduce `MIN_GPU_DIMENSION` threshold:
   - Currently 64px - conservative for discrete GPUs
   - Apple Silicon unified memory has near-zero transfer cost
   - Could lower to 32px or even 16px - profile to find sweet spot

### Performance Profiling Plan

Before implementing, measure current bottlenecks:

```bash
# Run with Instruments Time Profiler
xcrun xctrace record --template 'Time Profiler' --launch -- \
    "/path/to/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre"
```

Key questions:
1. What % of batch time is blur vs morphology vs I/O?
2. Where does CPU fallback spend most time - algorithm or memory?
3. Is thread pool saturated or waiting on I/O?

### Phase 11: Parallel Thumbnail Generation

**Goal**: Use all CPU cores for thumbnail loading (currently single-threaded!)

Current: `ThumbnailPixmapCache` uses single `QThread` - major bottleneck on M4 Max (16 cores idle)

1. Replace single QThread with QThreadPool:
   ```cpp
   // In ThumbnailPixmapCache::Impl
   QThreadPool m_thumbPool;
   m_thumbPool.setMaxThreadCount(QThread::idealThreadCount());
   ```

2. Submit thumbnail load requests as QRunnable:
   ```cpp
   QtConcurrent::run(&m_thumbPool, [=]() {
       QImage thumb = loadSaveThumbnail(imageId, thumbDir, maxThumbSize);
       // Post result back to main thread
   });
   ```

3. Batch thumbnail requests:
   - When scrolling thumbnail strip, many requests arrive at once
   - Submit all visible thumbnails in parallel
   - Cancel off-screen requests to avoid wasted work

4. Consider separate pool from page processing:
   - Thumbnails are I/O-bound (reading small files)
   - Page processing is CPU-bound (image algorithms)
   - Could run both pools simultaneously without contention

### Quick Wins (Can Implement Immediately)

1. **Lower MIN_GPU_DIMENSION to 32** - trivial change, may help
2. **Add QoS to WorkerThreadPool** - `dispatch_set_target_queue` with `QOS_CLASS_USER_INITIATED`
3. **Enable erosion** (already works, just needs lifecycle check from Phase 1)
4. **Parallel thumbnails** - swap single QThread for QThreadPool in ThumbnailPixmapCache

---

## Priority Order

| Priority | Phase | Effort | Impact |
|----------|-------|--------|--------|
| 1 | Phase 3 - Thread safety | Low | Likely fixes dilation |
| 2 | Phase 1 - Lifecycle | Medium | Enables safe GPU |
| 3 | Enable GaussBlur | Low | Immediate speed gain |
| 4 | **Phase 11 - Parallel thumbnails** | Medium | **16x faster thumbnail loading on M4 Max** |
| 5 | Phase 7 - vImage | Medium | Fast CPU fallback |
| 6 | Phase 8 - dispatch_apply | Medium | Parallel CPU |
| 7 | Phase 9 - Async GPU | High | Pipeline overlapping |
| 8 | Phase 10 - Memory | High | Large project support |

---

## Current Concurrency Summary

| Component | Current | Optimal (M4 Max) |
|-----------|---------|------------------|
| Page processing (WorkerThreadPool) | 16 threads ✅ | 16 threads |
| Thumbnail loading | **1 thread ❌** | 16 threads |
| Metal GPU (disabled) | 0 | 40 GPU cores |
| Color detection (QtConcurrent) | Global pool ✅ | Works |

---

## Upcoming Features

### Output DPI Inheritance

**Problem**: Output DPI defaults to 300x300 regardless of PDF import resolution. If user imports at 600 DPI, they get downsampled output by default.

**Location**: `src/core/filters/output/Params.cpp:13`
```cpp
Params::Params() : m_dpi(300, 300), m_despeckleLevel(1.0), m_blackOnWhite(true) {}
```

**Solution**:
1. Store selected import DPI in project settings (already in `PdfReader::setImportDpi()`)
2. When creating new project, pass import DPI to output filter default params
3. Change `Params` default constructor or add factory that accepts DPI

**Affected files**:
- `src/core/filters/output/Params.cpp` - accept DPI in constructor
- `src/core/filters/output/Settings.cpp` - use project-level default
- `src/app/MainWindow.cpp` - pass import DPI when creating project

---

### Thumbnail Bar Color Mode Filters

**Feature**: Add toggle icons in thumbnail toolbar to filter pages by color mode (B&W, grayscale, color)

**Purpose**: In mixed-content books (textbooks, art books), quickly identify pages with incorrect color detection by hiding correctly-detected pages.

**UI Design**:
- Add 3 toggle buttons in thumbnail toolbar (near "Keep in View", zoom icons)
- Icons: B&W icon, Grayscale icon, Color icon
- Each toggles visibility of that color mode in thumbnail strip
- Default: all visible
- Example use: Toggle B&W off → only see grayscale/color pages → spot misdetections

**Implementation**:
1. Add filter buttons to `ThumbnailSequence` toolbar area
2. Each page's color mode stored in Finalize filter's `Settings` (already tracked via `ColorMode`)
3. Filter `ThumbnailSequence::Item` list based on active filters
4. Update when filter toggles change

**Key files**:
- `src/core/ThumbnailSequence.cpp/h` - add filtering logic
- `src/app/MainWindow.cpp` - add toolbar buttons
- `src/core/filters/finalize/Settings.cpp` - query color mode per page

**Alternative location**: Move output resolution setting from Output stage to Finalize stage (less crowded UI)

---
 