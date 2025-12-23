# Session Changelog

## 2025-12-22

### Fix: CollapsibleGroupBox title collision with content

**File:** `src/core/CollapsibleGroupBox.cpp`

**Problem:** In Filter 1 (Fix Orientation), the "Rotate" title label was colliding with the rotation arrow buttons below it.

**Root Cause:** The `resizeEvent()` method was setting layout contents margins to `(0, 0, 0, 0)`, which removed all spacing including the necessary top margin between the group box title and its content.

**Fix:** Changed the top margin calculation to account for the title bar height:

```cpp
// Before:
layout()->setContentsMargins(0, 0, 0, 0);

// After:
int topMargin = titleRect.bottom() + 4;  // Space below title text
layout()->setContentsMargins(0, topMargin, 0, 0);
```

**Location:** Lines 132-136

**Note:** User indicated this fix caused other problems in different areas (to be addressed later).

### Fix: OptionsWidget content alignment across all 8 filters

**Problem:** Content inside CollapsibleGroupBox widgets was indented from the left edge instead of aligning flush with the filter stage list.

**Root Cause:** Qt's default layout margins (~9px) were being applied to:
1. Top-level QVBoxLayout in each OptionsWidget
2. Layouts directly inside CollapsibleGroupBox widgets

**Fix:** Added `leftMargin: 0` property to relevant layouts in all 8 filter OptionsWidget.ui files:

**Files modified:**
- `src/core/filters/fix_orientation/OptionsWidget.ui`
- `src/core/filters/page_split/OptionsWidget.ui`
- `src/core/filters/deskew/OptionsWidget.ui`
- `src/core/filters/select_content/OptionsWidget.ui`
- `src/core/filters/page_layout/OptionsWidget.ui`
- `src/core/filters/finalize/OptionsWidget.ui`
- `src/core/filters/output/OptionsWidget.ui`
- `src/core/filters/export/OptionsWidget.ui`

**Note:** Intentional 15px indentation for sub-options (e.g., radio buttons under checkboxes) was preserved.

### Version bump to 2.0a10

**File:** `version.h.in`

Changed VERSION from "2.0a9" to "2.0a10".

### Fix: CollapsibleGroupBox title alignment and vertical spacing

**Problem:**
1. CollapsibleGroupBox titles were offset to the right by the collapse button's space
2. Too much vertical space below titles compared to regular QGroupBox

**Fix:**
1. Moved collapse button from LEFT of title to RIGHT of title
   - `buttonX = titleRect.right() + 6` instead of `buttonX = 2`
2. Removed title padding-left (was 18px, now 0px)
3. Reduced padding-top from 20px to 14px in stylesheet
4. Reduced topMargin from `titleRect.bottom() + 4` to `titleRect.bottom() + 2`

**Files modified:**
- `src/core/CollapsibleGroupBox.cpp` - button positioning and margin calculation
- `src/resources/light_scheme/stylesheet/stylesheet.qss`
- `src/resources/dark_scheme/stylesheet/stylesheet.qss`

### Enhancement: Brightness/Contrast sliders with visible tick marks and center detent

**Problem:** Brightness and Contrast sliders needed visible tick marks and a center detent indicator for easier reset to neutral.

**Solution:** Created a custom `CenteredTickSlider` widget that:
1. Inherits from QSlider and paints custom tick marks at -100, -50, 0, 50, 100
2. Draws a prominent center (0) indicator with a taller, darker tick mark
3. Works with the existing snap-to-center behavior (snaps to 0 when within ±5)

**Files created:**
- `src/core/CenteredTickSlider.h` - Header for custom slider class
- `src/core/CenteredTickSlider.cpp` - Implementation with custom paint for tick marks

**Files modified:**
- `src/core/CMakeLists.txt` - Added new files to build
- `src/core/filters/output/OptionsWidget.ui`:
  - Changed brightnessSlider and contrastSlider to use CenteredTickSlider
  - Added CenteredTickSlider to customwidgets section
  - Moved "Pick Paper Color" section before sliders
- `src/resources/light_scheme/stylesheet/stylesheet.qss` - Adjusted slider height/margin for tick marks
- `src/resources/dark_scheme/stylesheet/stylesheet.qss` - Same adjustment for dark theme
