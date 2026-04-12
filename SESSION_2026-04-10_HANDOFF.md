# Session Write-up — Page Split Auto-Detection (2026-04-10 → 2026-04-11)

Picks up from the Codex handoff at the start of 2026-04-11 (00:00 local).
Everything below was done on top of the 2.0a25 release commit `d98445a4`
on the `main` branch, uncommitted.

---

## Starting state (from Codex)

Codex had been iterating on `src/core/filters/page_split/SpineDarknessFinder.cpp`
trying to fix auto page-split detection on the test project
`/private/tmp/fix fix/fix fix.ScanTailor` (*The Lost Gods of England*,
Branston, Thames & Hudson, 1974). When I took over, the state was:

- Page 67: fixed
- Page 71: fixed (broad-plateau → normal-peak override)
- **Page 68: still bad** — unresolved
- Page 6: regression fixed
- Codex had added:
  - A `broadGutterRescue` flag through `findSpine`
  - A `kPageSplitDetectorVersion = 2` on `Dependencies` for cache invalidation
  - Temporary `qDebug` diagnostics in `page_split/Task.cpp` tagging
    `pdfPage`, `subPage`, and the stored `cutterX`
  - Broad-plateau acceptance paths in `SpineDarknessFinder::evaluateColumn`
- All changes uncommitted.

---

## Diagnosis — the wrong-page detour

My first mistake: I read the project XML, saw `<image id="66" fileImage="22">`
and `<page id="68" imageId="66" subPage="right"/>`, and concluded "page 68 =
PDF page 23 (the jewelry plate)". I then spent a build rendering PDF page
23, analyzing it offline, and fixing the wrong page entirely.

The user corrected me: **"page 68" in their vocabulary is the printed book
page number, which is at PDF page 68 of the source (image id 204,
fileImage=68)** — the FREY AND FREYA chapter. The XML `<page id="67">` vs
`<page id="68">` is a project-internal subpage numbering that happens to
overlap numerically but means something different.

Lesson carved into `ARCHITECTURE.md` §12: when a user says "page N", ask
whether they mean printed page, project subpage, or PDF page index.

---

## Iteration 1 — `findSpineByPaperGap` v1 (build `st-spectre14.log`)

Page 68 is an asymmetric text-text spread. The left page has text
extending to virt ~1491, the right page's chapter header + text begins at
virt ~1659. Between them is white paper with **no darkness signature at
all** — the binding fold is bright paper. The existing `findSpine` dark
detector correctly returns null (all dark candidates are photo edges that
fail gates).

**Fix v1**: add a static `SpineDarknessFinder::findSpineByPaperGap` method,
called from `PageLayoutEstimator::tryCutAtFoldingLine` when `findSpine`
returns null.

The algorithm:
1. Build a column-brightness profile from the bottom text band (rows 60–95%
   of dsH — intentionally excludes photos at the top).
2. Smooth with a 9-tap boxcar.
3. Gate each column on:
   - `smooth[x] ≥ 215` (paper-bright)
   - Left and right "flank stripes" (offsets 25–80 px) both contain a
     column with brightness ≤ 195 (text-darkened on both sides)
   - Peak prominence ≥ 18 vs the brighter flank
4. Among gate-passing columns, pick the one closest to `centerXDs`
   (Vision's anchor).

Detector version bumped 2 → 3. Cache invalidated, rebuilt, launched as
build 14.

**Result**: page 68 still used Vision center. The log showed
`[PAPER-GAP] no viable candidate`. Why? Because on page 68, the left text
edge is at virt ~1491 and any candidate inside the bright run is at virt
~1554+ — with `kFlankInner=25`, the left flank of a candidate at pix 1080
starts at pix 1055, which is **past** the left text edge at pix 994. The
flank misses the text entirely. Also the right side's "text body" only
smooths to ~218, never below the 195 text threshold, so the
"both-flanks-text" gate fails.

---

## Iteration 2 — longest-run selection (build `st-spectre15.log`)

Relaxed the gates and changed the selection rule:

- Dropped the 215 threshold to still-conservative but easier, kept 235
  as the "truly paper" threshold.
- Relaxed the "both flanks must be ≤195" to "at least one flank ≤195".
- Replaced "closest to anchor among viable columns" with "longest
  contiguous run of viable columns, return its center".

Detector version 3 → 4. Build 15.

**Result**: page 68 moved from virt 1654 to virt 1599. User: *"a little
better, not much"*. The improvement was ~55 px, but the geometric midpoint
of the empty paper region was at virt ~1575. My pick at 1599 was biased
right because the brightness ramp on the left (sharp: 255→170 in ~10 px)
is much steeper than on the right (gradual: 255→218 over ~50 px), and my
"≥235 run center" picked the midpoint of the ≥235 region, not of the
actual inter-text region.

---

## Iteration 3 — scan-from-center + half-max edge detection (build `st-spectre16.log`)

Third rewrite of the paper-gap algorithm:

1. Find the bright run (≥235) that **contains centerXDs**. Walk outward
   from the anchor until smoothed brightness drops below 235, including
   columns past `xLo`/`xHi` so the flank lookup can reach text just
   outside the leash.
2. Compute `runPeak` inside the run, and `leftFlankMin` / `rightFlankMin`
   in 100-px windows outside each run edge.
3. Gates: `runPeak − leftFlankMin ≥ 18`, `runPeak − rightFlankMin ≥ 18`,
   `min(leftFlankMin, rightFlankMin) ≥ 80`. Dropped the
   "at least one flank ≤195" gate entirely.
4. **Half-max edge detection**: find the first column on each side where
   `smooth[x]` has dropped to `0.5 * (runPeak + flankMin)`. This is the
   center of each brightness ramp, which approximates the text margin's
   physical location. Return the midpoint of the two crossings.

Half-max fixes the ramp asymmetry: a sharp left ramp and a gradual right
ramp produce crossings at roughly symmetric depths, so the midpoint tracks
the true gutter center instead of the 235 cutoff.

Detector version 4 → 5. Build 16.

**Result**: page 68 moved from virt 1599 to virt 1584. User response:
*"passable, but why is it not centered on the dark spine?"* — pointing at
a visible dark vertical strip in the gutter that the algorithm wasn't
landing on.

---

## Iteration 4 — "binding fold against text" acceptance path (build `st-spectre17.log`)

Went back to the log to see what `findSpine` thought of the dark feature
the user was pointing at:

```
SpineDarknessFinder: global-max reject xT= 499 mean= 91.0218
  left= 77.3208 right= 11.9286 peakProm= 13.701 < 14
```

The dark detector **found** the candidate at virt 1497 (brightness 164,
darkness 91), with a text-darkened left neighbor (77) and a paper-bright
right neighbor (12). It failed the `kMinPeakProminence ≥ 14` gate by
**0.3 points** because prominence was computed against `max(left, right)`
= 77, giving `91 - 77 = 14` (implemented as `13.701` due to a rounding
quirk).

This is the classic "binding fold sitting against the left page's text
margin" pattern: text on one side, paper on the other, moderate-dark
candidate in between. The peak-prominence gate (which requires darkness
exceeding the **brighter** of the two neighbors) is exactly the wrong test
for this signature.

**Fix**: add a narrow acceptance path inside `evaluateColumn` — accept
despite failing peakProm if all of:
- `mean ≥ 80` (moderate dark — 255 - brightness ~175)
- `drf ≥ 0.7` (consistent vertical line)
- `min(left, right) ≤ 25` (one neighbor is unambiguously paper)
- `mean − min(left, right) ≥ 60` (clear contrast vs the paper side)

Verified against the entire build-16 log: **only one rejected candidate in
the entire project fits this pattern** — the page 68 case. Every other
"lighter neighbor > 70" rejection has both neighbors ≥73, so they all fail
the `min ≤ 25` test. No regression risk.

Detector version 5 → 6. Build 17.

**Result**: user response *"much better"*. Page 68 now lands on the visible
binding fold directly.

Same session, user flagged: *"page 65 still catches to the right of the
gutter, but unless there is an easy win, we could stop"*.

---

## Page 65 — the aspect-1.41509 mystery

Page 65 (text-left, Oseberg-wagon photo-right) was also going through
paper-gap. The log showed **two recomputes** of PDF page 65 in the same
session:

1. First recompute: aspect 1.33441 (image 3308×2479, matches project XML).
   Paper-gap picked virt 1635 — the geometric midpoint of the empty paper
   region between the left text and the right photo.
2. Second recompute: aspect 1.41509 (image 3508×2479 — ~6% wider).
   Vision center shifted to 1754, paper-gap picked virt 1791 (much worse).

I don't fully understand **why** the same PDF page gets processed twice
with different dimensions within a single session. It's not a 90° rotation
(would swap W and H) and it's not a small deskew (deskew runs after
page_split). Logged as a footnote in `ARCHITECTURE.md` §12.

---

## Iteration 5 — full-height row band (build `st-spectre18.log`)

Diagnosed page 65 by analyzing the rendered PDF offline. The key finding:
the `kMinFlankBrightness` gate was never exercised because the
**row band [60%, 95%] misses the wagon photo**, which lives in the top
half of the page. Without the photo in the brightness profile, the right
flank stays paper-tinted (~225) and the "bright run" extends across the
entire right page margin.

With a full-height profile (rows 5–95%), the photo contributes its
~165 smoothed mean brightness on the right side, and a clean bright run
appears at downscaled pix 517–559 (virt 1611 center) — clearly bracketed
by left text (172) and the wagon photo (158).

**Fix**: change `findSpineByPaperGap`'s row band from `[0.60, 0.95]` to
`[0.05, 0.95]`. Philosophically inverted the earlier design decision:
photos *help* the flank detection on asymmetric spreads, they don't
pollute the profile.

Detector version 6 → 7. Build 18.

**Result**: user: *"65 will work, but it's not better. no other
regressions."* Page 65 moved from 1635 → 1611 (24 px left) — a subtle
improvement, roughly at the limit of what brightness analysis can do on a
spread with no symmetric text signal.

---

## Final state (uncommitted, on top of `d98445a4`)

### Files modified
- `src/core/filters/page_split/SpineDarknessFinder.h`:
  declaration of `findSpineByPaperGap`, optional `broadGutterRescue`
  out-param on `findSpine`.
- `src/core/filters/page_split/SpineDarknessFinder.cpp`:
  - `kMaxPaperNeighborDarkness` raised 55 → 70 (Codex)
  - Broad-dark-band / plateau rescue paths in `evaluateColumn` (Codex)
  - Plateau-edge-near-anchor path (Codex)
  - Anchor re-pick normal-peak override over broad plateau (Codex)
  - New: binding-fold-against-text acceptance path in `evaluateColumn`
  - New: `findSpineByPaperGap` implementation (three rewrites, current
    version is scan-from-center + half-max edge detection, full-height
    row band 5–95%)
- `src/core/filters/page_split/PageLayoutEstimator.cpp`:
  - Vision refinement calls `findSpine` with `broadGutterRescue` out-param
  - If `findSpine` returns null, falls back to `findSpineByPaperGap` with
    the same Vision anchor and a 10% leash check
- `src/core/filters/page_split/Dependencies.{h,cpp}`:
  - `m_detectorVersion` field serialized to `<detectorVersion>` attribute
  - `kPageSplitDetectorVersion = 7` (bumped from 2 through 7 during
    the session to invalidate caches at each algorithm change)
- `src/core/filters/page_split/Task.cpp`:
  - Temporary `qDebug` diagnostics tagging `pdfPage`, `subPage`,
    `cutterX`, and recompute reason (Codex, still uncleaned — task #31)

### Behavior, page by page (Branston test project)
| Page | Before tonight | After tonight | Status |
|---|---|---|---|
| 67 | broken in previous build, fixed by Codex via broad-plateau path | virt 1551 | ✓ fixed |
| 68 (FREY) | Vision center 1654 (user "bad") | virt 1497 (binding-fold-against-text path) | ✓ fixed |
| 71 | fixed by Codex (broad-plateau override) | virt 1548 | ✓ fixed |
| 65 | virt 1791 (second-recompute disaster) | virt 1611 (full-height row band) | ⚠ usable, not perfect |
| Other | various | unchanged | ✓ no regressions |

### Open items
- **Task #31**: Clean up `Task.cpp` temporary diagnostics. The
  `pdfPage=`/`subPage=` `qDebug` logs Codex added were useful while
  debugging but should be either gated behind a flag or removed before
  any release.
- **Page 65**: still slightly right of the visible binding fold. The
  remaining ~80 px error is at the limit of brightness-only detection on a
  text+photo asymmetric spread. Real fix would require treating the
  photo's left edge as the gutter's right boundary — non-trivial and
  out of scope for this session.
- **Aspect 1.41509 mystery**: investigate why PDF page 65 gets processed
  twice with different image dimensions in the same session. Possibly a
  deskew-parallel-race or a DPI-cache issue. Only observed on page 65.
- **Commit + release**: none of tonight's work is committed. Decision
  pending on whether to ship this as 2.0a26 or bundle with further fixes.

### Detector version history tonight
| Version | Algorithm change |
|---|---|
| 2 | Codex's starting state |
| 3 | Paper-gap v1 added (both-flanks-text gates, closest-to-anchor) |
| 4 | Paper-gap v2 (relaxed gates, longest-run center) |
| 5 | Paper-gap v3 (scan-from-center, half-max edges) |
| 6 | Binding-fold-against-text acceptance path in `findSpine` |
| 7 | Paper-gap row band changed 60–95% → 5–95% |

---

## Lessons (for the next person poking this code)

1. **Tag every diagnostic with the page id.** Tasks run in parallel and
   log lines interleave in arbitrary order. `Task.cpp`'s `pdfPage=N`
   tagging saved me several hours.
2. **Bump `kPageSplitDetectorVersion` after every algorithm change.**
   Otherwise cached results from the old code get served to the new
   code, and you'll chase ghosts wondering why your fix "doesn't work".
   Bit me twice in one night.
3. **Render the same PDF page offline in Python when the log isn't
   enough.** `pdftoppm -r 200 -f N -l N` plus PIL/numpy is faster than
   threading more `qDebug` statements.
4. **Prefer a narrow acceptance path over loosening a global gate.**
   The "binding fold against text" path for page 68 was specifically
   gated by `min(left,right) ≤ 25`, which I verified against the entire
   log — only one candidate project-wide matched that pattern.
5. **Ask what "page N" means** before you start analyzing. Printed page,
   project subpage id, and PDF page index can all coexist in the same
   project and are all different numbers.
6. **Asymmetric spreads are the hard case.** Text/photo spreads have no
   symmetric signal; the detector has to combine multiple heuristics
   (dark spine + paper gap + text/photo asymmetry) and you can't cover
   every layout perfectly. Accept that some pages need manual nudging.

---

*Written 2026-04-11, picks up from Codex handoff dated 2026-04-10 23:xx.
Build 18 was the last working build of the session; app binary at
`build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre`,
log at `/tmp/st-spectre18.log`.*
