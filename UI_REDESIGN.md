# UI Redesign Spec — ScanTailor Spectre

## Design Philosophy

Max Bill / Swiss International Style. No decoration. Hierarchy through typography (weight, size) and spacing only. No rounded corners. No color accents. No gradients. No shadows. Monochrome. The user explicitly hates: liquid glass, dark themes, rounded corners, toggle switches, colored UI elements (green/blue state indicators).

## Reference Mockup

See `/tmp/options_mockup2.png` (or regenerate from `/tmp/options_mockup2.html`).

## Scope

The changes are almost entirely in the QSS stylesheets:
- `/src/resources/light_scheme/stylesheet/stylesheet.qss` (primary — user uses light theme)
- `/src/resources/dark_scheme/stylesheet/stylesheet.qss` (apply equivalent changes for consistency)

Some layout adjustments may require changes to `.ui` files or `CollapsibleGroupBox`.

---

## 1. Sliders (highest impact)

**Current**: 6px thick tracks, 20px round handles with drop shadow, heavy gray fill.

**Target**:
- Track height: 1px
- Track color: `#ccc`
- Fill (left of handle): `#555`
- Handle: 8px diameter solid circle, `#333` fill, no border, no shadow
- Groove margin: 0
- Total slider row height: 20px (was ~40px)

**QSS targets**: `QSlider::groove:horizontal`, `QSlider::handle:horizontal`, `QSlider::sub-page:horizontal`, `QSlider::add-page:horizontal`

```css
QSlider::groove:horizontal {
    height: 1px;
    background: #ccc;
    margin: 0;
}
QSlider::handle:horizontal {
    width: 8px;
    height: 8px;
    background: #333;
    border-radius: 4px;
    border: none;
    margin: -4px 0;
}
QSlider::sub-page:horizontal {
    height: 1px;
    background: #555;
}
QSlider::add-page:horizontal {
    height: 1px;
    background: #ccc;
}
```

## 2. Slider Layout (label + slider + value on one line)

**Current**: Label on its own line above the slider. Slider on its own line. No value readout next to slider.

**Target**: `[Label 58px] [——●——————] [value 28px]` all on one 20px-tall row.

This requires layout changes in the `.ui` files for the Photo Adjustments section (`src/core/filters/output/OptionsWidget.ui` and the weasel PhotoAdjustments widget). Each slider label, QSlider, and value QLabel should be in a QHBoxLayout instead of stacked vertically.

## 3. Checkboxes

**Current**: macOS system checkboxes — 16px, blue fill when checked, rounded corners.

**Target**:
- Size: 11px square
- Unchecked: white fill, 1.5px `#999` border
- Checked: `#333` fill, `#333` border, white checkmark
- No rounded corners

```css
QCheckBox::indicator {
    width: 11px;
    height: 11px;
}
QCheckBox::indicator:unchecked {
    border: 1.5px solid #999;
    background: #fff;
}
QCheckBox::indicator:checked {
    border: 1.5px solid #333;
    background: #333;
    image: url(:/icons/check-white-small.svg);  /* need small white check icon */
}
```

Note: May need a custom SVG icon for the checkmark, or use the existing one scaled down.

## 4. Collapsible Section Headers

**Current**: `CollapsibleGroupBox` with a boxed border, background fill, and a tiny square minus/plus button. Looks like Windows XP.

**Target**:
- No box border, no background
- Section header: 10px, font-weight 500, color `#333`
- Small triangle indicator on the right (▾ when open, ▸ when closed), color `#aaa`
- Hairline divider (1px `#ddd`) between sections instead of box borders
- Padding: 10px top, 4px bottom on header; 8px bottom on body

```css
CollapsibleGroupBox {
    border: none;
    background: transparent;
    margin: 0;
    padding: 0;
}
CollapsibleGroupBox::title {
    font-size: 10px;
    font-weight: 500;
    color: #333;
    padding: 10px 0 4px 0;
    border: none;
    background: transparent;
}
```

The collapse/expand indicator icon needs to change from the current boxed minus/plus to a simple triangle. This may require a C++ change in `CollapsibleGroupBox.cpp` or an icon swap via `IconProvider`.

Add a 1px `#ddd` horizontal line between sections. This can be done with a `QFrame` with `frameShape: HLine` or via bottom-border on each section.

## 5. Buttons

**Current**: Rounded, gray fill, border, system-styled.

**Target**:
- No fill (transparent background)
- 1px `#ccc` border
- Square corners (0px radius)
- Font: 9-10px, color `#666`
- Padding: 2px 10px
- Hover: border `#999`
- Pressed: background `#e8e8e8`

```css
QPushButton {
    background: transparent;
    border: 1px solid #ccc;
    border-radius: 0;
    padding: 2px 10px;
    font-size: 10px;
    color: #666;
}
QPushButton:hover {
    border-color: #999;
}
QPushButton:pressed {
    background: #e8e8e8;
}
```

## 6. Spin Boxes

**Current**: System-styled with up/down arrows, takes too much space.

**Target**:
- Minimal border: 1px `#ccc`
- No rounded corners
- Smaller arrows or hide arrows (use scroll-to-change instead)
- Font: 10px
- Height: 20px

```css
QAbstractSpinBox {
    border: 1px solid #ccc;
    border-radius: 0;
    padding: 1px 4px;
    font-size: 10px;
    min-height: 18px;
}
```

## 7. Combo Boxes (dropdowns)

**Current**: System-styled, rounded.

**Target**:
- 1px `#ccc` border, square corners
- Font: 11px, color `#333`
- Small dropdown arrow
- Padding: 2px 8px

```css
QComboBox {
    border: 1px solid #ccc;
    border-radius: 0;
    padding: 2px 8px;
    font-size: 11px;
    color: #333;
    background: #fff;
}
```

## 8. Radio Buttons

**Current**: macOS system radio buttons — large, blue fill.

**Target**:
- 11px diameter circle
- Unchecked: 1.5px `#999` border, white fill
- Checked: `#333` fill with small white dot center

## 9. Section Spacing

**Current**: Inconsistent margins, too much vertical space between controls.

**Target**:
- Between sections: 1px hairline divider
- Section header top padding: 10px
- Section body bottom padding: 8px
- Row height for checkbox rows: 22px
- Row height for slider rows: 20px
- Gap between label and control: 6-8px

## 10. Typography

**Current**: Mixed sizes and weights with no system.

**Target hierarchy**:
- Section header: 10px, weight 500, color `#333`, uppercase optional
- Sub-header (e.g. "White Balance", "Tone"): 10px, weight 600, color `#333`
- Control label: 10-11px, weight 400, color `#888` (for slider labels) or `#555` (for checkbox labels)
- Value readout: 9px, weight 400, color `#aaa`, tabular-nums
- Button text: 9-10px, weight 400, color `#666`

## 11. Scrollbars

**Current**: 17px wide, heavy handles.

**Target**:
- Width: 8px
- Handle: `#bbb`, no border
- Track: transparent
- On hover: handle `#999`

```css
QScrollBar:vertical {
    width: 8px;
    background: transparent;
}
QScrollBar::handle:vertical {
    background: #bbb;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: #999;
}
QScrollBar::add-line, QScrollBar::sub-line {
    height: 0;
}
```

## What NOT to Change

- Stage list (filter table on top right) — keep as-is, it's fine structurally
- Thumbnail strip — keep as-is
- Image view canvas — keep as-is
- Menu bar — native macOS, don't touch
- Dock widget structure — keep the three-panel layout
- Any functional behavior

## Implementation Notes

1. Start with the light theme QSS (`light_scheme/stylesheet/stylesheet.qss`)
2. Test each change visually before moving to the next
3. The slider inline layout (label+slider+value on one line) requires `.ui` file changes, not just QSS
4. The CollapsibleGroupBox indicator change may require a C++ tweak or icon swap
5. Custom checkbox/radio SVG icons may need to be added to `src/resources/icons/`
6. After light theme is done, port the equivalent changes to dark theme QSS with inverted values
