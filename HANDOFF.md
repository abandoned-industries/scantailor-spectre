# Handoff — 2026-04-16 (post v2.0a29 release)

## Release status

**v2.0a29 is shipped.** Signed (Developer ID), notarized, stapled, on
GitHub Releases with DMG. To verify: `gh release list` and
`xcrun stapler validate "build/ScanTailor Spectre.app"`.

## What changed in v2.0a29

Slider-UX work for the Output stage Photo Adjustments panel.

### Files

| File | Change |
|---|---|
| `src/core/CenteredTickSlider.cpp` | Install a `QProxyStyle` that returns `Qt::LeftButton` for `SH_Slider_AbsoluteSetButtons` so left-click on the track jumps the handle to the clicked position (Lightroom-style), preserving normal drag behaviour. |
| `src/core/filters/output/OptionsWidget.ui` | Cap `maximumSize` on the 8 photo-adjustment `QLineEdit` value boxes (44 px, 48 px for exposure) so they no longer expand to fill the grid column. |
| `src/core/filters/output/OptionsWidget.cpp` | Debounce reloads via `m_delayedReloadRequest.start(300)`; remove `snapSliderToZero` mid-drag; move `emit invalidateAllThumbnails()` out of every per-tick handler into `sendReloadRequested()` so thumbnails redraw once when the user pauses, not on every drag tick. |
| `src/core/weasel/webui/photo_adjustments.html` | Numeric values are `<input type="number">` (web panel). |
| `src/core/weasel/webui/shared/panel.js` | `setupSlider` handles bidirectional slider↔input with clamping, Enter-to-blur, and `displayTimes100` for wienerCoef/exposure. |
| `src/core/weasel/webui/shared/panel.css` | Compact styling for the new numeric inputs in the web panel. |

## Notes for next session

- The web panel only loads in `colorMode == COLOR || COLOR_GRAYSCALE`.
  In B&W / GRAYSCALE / MIXED modes the user sees the **native Qt panel**
  built from `OptionsWidget.ui` — both panels need to stay in sync.
- The earlier "build/ScanTailor Spectre.app won't launch" issue was an
  install-name problem fixed by running `cmake --build . --target
  scantailor_bundle` (not just `--target scantailor`). The `_bundle`
  target runs `macdeployqt` and `packaging/macos/fix-bundle-libs.sh`.
- `build-clean-package/` is leftover from the v2.0a28 release prep and
  is not tracked.

## Don't

- Don't redo the v2.0a29 release work — it's on GitHub.
- Don't run `make` or `cmake --build` without appending to BUILD_LOG.md
  first (project CLAUDE.md prime directive).
