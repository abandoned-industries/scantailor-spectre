# ScanTailor Startup Screen Specification

This document describes the implemented startup screen design for ScanTailor Spectre.

## Layout

The startup screen displays a simple typographic interface anchored to the top-left of the content area.

### Structure

```
[window title bar]

[top margin]

    Import PDF          ← action, bold weight
    Import Folder       ← action, bold weight
    Open Project        ← action, bold weight

    [vertical space]

    Recent              ← section label, regular weight, secondary color
    No recent projects  ← empty state text, regular weight, tertiary color

```

### Content Groups

**Actions group:**
- Import PDF
- Import Folder
- Open Project

All actions use bold/semibold weight in primary text color. Line spacing is consistent within the group.

**Recent projects group:**
- "Recent" label in secondary (gray) color, regular weight
- Project list below, or "No recent projects" in tertiary (lighter gray) color when empty

### Positioning

- Content anchored to top-left corner of the content area
- Fixed margins from window edges (not centered, not responsive to window size)
- Content does not reposition when window is resized

### Typography

- System font (San Francisco on macOS)
- Actions: bold/semibold weight, primary label color
- Section label ("Recent"): regular weight, secondary label color
- Empty state / project names: regular weight, secondary or tertiary color
- All text left-aligned

### Visual Treatment

- No borders
- No divider lines
- No boxes around groups
- No icons
- No underlines
- No hyperlink styling
- Grouping achieved through spacing and typography only

### Interaction

- Action items are clickable
- Hover state: color change only (to accent color), no other effects
- Recent project items are clickable when present
- Cursor changes to pointer on interactive elements

### Empty State

When no recent projects exist, display "No recent projects" in tertiary/disabled color. The "Recent" label remains visible. Structure does not change between empty and populated states.

## Implementation Notes

This is a Qt-based application. The layout uses:
- QVBoxLayout for vertical stacking
- QLabel widgets for text (or QPushButton styled as flat text for actions)
- Fixed margins via setContentsMargins() or explicit positioning
- System palette colors for text hierarchy
- No custom stylesheets beyond removing default widget chrome
