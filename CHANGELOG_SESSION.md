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
