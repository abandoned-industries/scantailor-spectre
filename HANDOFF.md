# Page Split Auto Detection Handoff

## 2026-04-12 Update: Current State After Failed v21/v22 Attempts

This section supersedes older notes below where they conflict.

Working tree/app state:

- Current detector version is **23** in `src/core/filters/page_split/Dependencies.cpp`.
- Version 23 is a rollback of the failed v21/v22 early overrides. It was built and launched.
- Current launched weasel app PID after rollback was `90138` at:
  `/Users/kazys/Developer/scantailor-weasel/build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre`

Important repo/process reminders:

- Before every build, append a `BUILD_LOG.md` entry first.
- Bump `kPageSplitDetectorVersion` for any page-split detector behavior change, otherwise cached project params may hide the result.
- Always launch the rebuilt app after building.
- The user is frustrated. Ask clarifying questions before making speculative detector changes.

### User's Correct Model Of The Problem

Do **not** frame this as:

- "pick the darkest column",
- "pick the midpoint of a pale/blank corridor",
- "pick the geometric midpoint",
- "the shadow is on one side."

The user clarified:

- There are very few dark **vertical or near-vertical** lines in the middle of these spreads.
- The gutter/fold manifestation varies:
  - simple black line in B/W-like pages,
  - sharp vertical transition,
  - blurry gradient,
  - soft crease/fold.
- The correct target is the visible book fold / gutter mark near the spread center, not arbitrary content edges and not the center of blank paper.

A future detector should search a tight center band for **near-vertical fold evidence** that is persistent through enough page height. It must distinguish fold evidence from image/text content edges.

### Known Bad / Regressed Pages From Latest User Feedback

After v22:

- **35**: no change, still bad.
- **41**: a little better, split moved between gutter and image, but still not correct.
- **65**: bad.
- **71**: bad.
- **72**: bad.
- **78**: bad.
- **83**: improved under v22.
- **87**: improved under v22.
- **95**: regressed under v22; line moved onto the right side of the image.

Because v22 improved only 83/87 and regressed 95 while leaving the main set unchanged, it was removed in v23.

### Failed Attempt v21: Pale Central Corridor

Change:

- Added early `[CENTRAL-PAPER-CORRIDOR]` override in `SpineDarknessFinder.cpp`.
- If the anchor/midpoint column was pale and darker flanks existed on both sides, it returned the anchor/midpoint as the split.

Result:

- 35: no change.
- 41: somewhat changed but still wrong.
- 65/71: still bad.

Why it was wrong:

- It assumed the center of a pale area was meaningful.
- The user explicitly pushed back: the target is the visible fold/gutter mark, not the geometric midpoint or a pale corridor center.

Status:

- Removed in v22/v23.

### Failed Attempt v22: Central Fold-Mark Early Override

Change:

- Replaced pale-corridor logic with `[CENTRAL-FOLD-MARK]`.
- Scored full-height column brightness profile for sharp gradients, blurry gradients, and dark-line prominence.
- Picked the closest sufficiently strong near-center mark and returned early before the existing dark-spine detector.

Result:

- 83 and 87 improved.
- 35/41/65/71/72/78 mostly unchanged.
- 95 regressed: split moved onto the right side of the image.

Why it was not safe:

- It still treated vertical edge strength too generically.
- Content/photo boundaries can be strong vertical marks near center.
- Returning early bypassed the existing more constrained candidate gates and caused regressions.

Status:

- Removed in v23.
- Do **not** re-add as an early override without additional constraints and user confirmation.

### Current Rollback v23

Current active state:

- v21/v22 early overrides removed.
- Detector version bumped to 23 specifically to invalidate cached v22 bad splits.
- Build succeeded with existing warnings.

Expected behavior:

- Should be roughly back to the pre-v21/v22 state, meaning page 95 regression should be cleared after recompute.
- It does **not** solve the main bad set.

### Better Next Directions

Ask the user before implementing one of these.

1. **Build an offline diagnostic tool first**

   Write a small local analyzer (preferably a script under `/tmp` or a throwaway tool, not production code at first) that renders selected PDF pages and outputs:

   - candidate x positions in virtual coords,
   - vertical persistence score,
   - dark-line score,
   - sharp edge score,
   - blurry gradient score,
   - distance from expected center,
   - whether the candidate overlaps likely photo/text content edge.

   Test pages:

   - bad: 35, 41, 65, 71, 72, 78, 95
   - improved by v22 but useful: 83, 87
   - regression checks: 19, 43, 72

   Do not patch production detector again until the diagnostic ranking puts the right line above the wrong line for most of these.

2. **Use existing detector candidates, but improve ranking**

   The existing detector already logs NMS candidates and some viable/gate-fail details. A safer next step may be to add a diagnostic-only dump of more columns around center, then adjust ranking among already plausible candidates.

   Avoid an unconditional early override.

3. **Represent "fold mark" as persistence, not just profile strength**

   The failed v22 scorer used full-height column profile averages. That can be fooled by a photo edge that is strong but not actually a fold.

   A better fold-mark candidate should count rows where a local vertical feature exists near the same x:

   - dark local minimum rows,
   - steep local gradient rows,
   - soft monotonic transition rows,
   - allow missing rows where captions/images interrupt the crease,
   - require enough vertical persistence and x stability.

4. **Penalize content boundaries explicitly**

   Content edges often have one side filled with photo/text texture for a large horizontal span. A fold mark should be near the boundary between page surfaces and should not simply be the edge of an illustration/text block.

   Potential signals to examine offline:

   - texture variance on each side,
   - whether the "dark" side continues as a large photo region,
   - whether the candidate lies at the edge of a known OCR/text/photo bounding area,
   - whether another weaker but more center-stable vertical mark exists closer to the expected fold.

5. **Ask the user to mark one page if ambiguity remains**

   If the diagnostic rankings remain ambiguous, ask the user for one explicit pixel/line target on a representative page, e.g. "on page 35, should the split be at x≈1533, x≈1545, x≈1605, etc.?" Do not infer silently.

### Practical Notes

Project image-id mapping:

- User page 35 corresponds to image id 33 / PDF file image 11.
- User page 41 corresponds to image id 39 / PDF file image 13.
- User page 65 corresponds to image id 63 / PDF file image 21.
- User page 71 corresponds to image id 69 / PDF file image 23.
- User page 72 corresponds to image id 72 / PDF file image 24.
- User page 78 corresponds to image id 78 / PDF file image 26.
- User page 83 likely corresponds to image id 81 or 84 depending on left/right displayed page; inspect project `pages` mapping before assuming.
- User page 87 corresponds to image id 87 / PDF file image 29.
- User page 95 corresponds to image id 93 or 96 depending on left/right displayed page; inspect project `pages` mapping.

Useful PDF path:

`/private/tmp/fix fix/originals/The lost gods of England_ [2d ed -- Brian Branston -- [2d ed_], London, England, 1974 -- Thames and Hudson -- 9780500110133 -- c796c21dacff93238daef6a517bacfa9 -- Anna’s Archive.pdf`

The project file sometimes uses an ASCII apostrophe in old notes, but the actual path has `Anna’s Archive.pdf` with a curly apostrophe.

### What Not To Do Next

- Do not revive paper-gap midpointing.
- Do not revive pale-corridor midpointing.
- Do not add a global "strongest vertical edge near center wins" override.
- Do not assume geometric midpoint is acceptable.
- Do not make another production change without first checking candidate rankings against the known page set.

---

Working directory:

`/Users/kazys/Developer/scantailor-weasel`

Test project:

`/private/tmp/fix fix/fix fix.ScanTailor`

Source PDF:

`/private/tmp/fix fix/originals/The lost gods of England_ [2d ed -- Brian Branston -- [2d ed_], London, England, 1974 -- Thames and Hudson -- 9780500110133 -- c796c21dacff93238daef6a517bacfa9 -- Anna's Archive.pdf`

Rebuilt app path:

`/Users/kazys/Developer/scantailor-weasel/build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre`

Always launch the app after rebuilding. The user explicitly asked for this.

Check correct app process:

```bash
ps -axo pid,command | rg 'scantailor-weasel/build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre'
```

## User Goal

Fix page-split auto detection on asymmetric photo/text spreads.

Important user feedback:

- Page 1 is impossible; ignore it.
- **Page 6: GOOD** in current build (detector version 14).
- **Page 21: GOOD** in current build.
- **Page 32: BAD** — split at photo edge (virtual 1761), should be at gutter further left (~virtual 1620).
- **Page 65: GOOD** in current build.
- **Page 69: GOOD** in current build.
- **Page 72: BAD** — split at virtual 1797, inside the right-page plaque grid. Should be at the gutter between idol photo and plaque grid.
- **Page 7: slightly off** — the user says there's a clear gutter but it's not quite right.
- **Page 19: BAD** — new regression, split into the right-page illustration.

The user's key design correction (from earlier sessions):

- If there is a visible gutter/spine/boundary line, USE IT.
- Do NOT use "midpoint" or "paper-gap midpoint" reasoning when a gutter is visible.
- The midpoint fallback is only needed when there is genuinely no visible gutter/spine.
- The user can see the gutter clearly on these pages. The algorithm can't find it. Fix the algorithm.

Communication note:

- Explain more, not less. The user understands things the model does not.
- Do not hand-wave. Tie claims to actual screenshots, logs, pixel evidence, and code paths.
- **Ask the user before making changes.** Do not speculatively build and hope.
- The user does NOT live in AI-land. Ground everything in what the actual scanned pages look like.

## Current Code State (Detector Version 14)

Relevant modified files:

- `BUILD_LOG.md`
- `src/core/filters/page_split/Dependencies.cpp` — `kPageSplitDetectorVersion = 14`
- `src/core/filters/page_split/SpineDarknessFinder.cpp` — per-row paper-adjacent rescue, per-x scan opens viable to all gate-passing columns (not boundary-only), page-tag logging
- `src/core/filters/page_split/SpineDarknessFinder.h` — `setLogPageTag()`, `logPageTag()` static methods
- `src/core/filters/page_split/PageLayoutEstimator.cpp` — page-tag prefixes on qDebug
- `src/core/filters/page_split/Task.cpp` — sets/clears page tag around estimatePageLayout call

Must update `BUILD_LOG.md` before any new build.

## Diagnostic Logging

All SpineDarknessFinder and PageLayoutEstimator qDebug lines are now prefixed with `[pdf=N sub=X]`. To get logs for a specific ScanTailor project page (e.g. page 32):

```bash
grep '\[pdf=32 ' output.log
```

The output is written to the background task output file. After launching the app:

```bash
cat /private/tmp/claude-502/-Users-kazys-Developer-scantailor-weasel/e0140e66-e7eb-404e-b4c7-7d153a414c06/tasks/<task-id>.output | grep '\[pdf=32 '
```

ScanTailor project page numbers = PDF page numbers for this project.

## Page 32 — Concrete Diagnosis

Layout: text on LEFT page, dark barn-interior photo on RIGHT page. Gutter between them at approximately virtual 1620. Photo left edge at approximately virtual 1761.

### What the detector does (from tagged logs):

```
[pdf=32] Vision uncertain (left: 45 right: 3) → falls through to traditional detection
[pdf=32] global-max reject xT=629.595 mean=196 left=172 right=181 (no paper-like side; both >70)
[pdf=32] [BOUNDARY-PICK] using visible gutter boundary: xCenter=587 mean=108.7 (distFromAnchor=35.7) anchor=551.3
[pdf=32] spine at 1761 meanDark=108.7 leftNbr=1.6 rightNbr=150.5 peakProm=-41.8
```

### NMS candidates for page 32:

```
#0 xVirt=1953 mean=195 drf=0.81 left=181 right=167 peakProm=14  [gate-fail]
#1 xVirt=1878 mean=179 drf=0.81 left=165 right=183 peakProm=-4  [gate-fail]
#2 xVirt=1782 mean=161 drf=0.79 left=90  right=150 peakProm=11  [CHOSEN]
#3 xVirt=1617 mean=94  drf=0.51 left=81  right=21  peakProm=12  [gate-fail]
#4 xVirt=1446 mean=51  drf=0.28 left=25  right=31  peakProm=19  [gate-fail]
#5 xVirt=1371 mean=45  drf=0.24 left=26  right=29  peakProm=16  [gate-fail]
#6 xVirt=1542 mean=9   drf=0    left=1   right=27  peakProm=-18 [gate-fail]
#7 xVirt=1692 mean=4   drf=0    left=6   right=2   peakProm=-3  [CHOSEN]
```

### Key observations:

1. **NMS candidate #3 at xVirt=1617** is the closest to the real gutter (~1620). It has mean=94, drf=0.51, left=81, right=21, peakProm=12. It fails gates because **drf=0.51 < kMinDarkRowFraction (0.55)**. Only 51% of rows are dark enough. The gutter crease is subtle on this page.

2. **NMS candidate #7 at xVirt=1692** has mean=4, drf=0. That's pure paper. It passed as [CHOSEN] probably via the per-x scan (not NMS) — but with mean=4 it shouldn't pass kMinMeanDarkness=25. Wait, it says [CHOSEN] in the NMS list but with mean=4 it can't have passed evaluateColumn. The "[CHOSEN]" label means it spatially overlaps the final pick, not that it passed gates. The final pick at xVirt=1761 (via boundary rescue at xCenter=587 downscaled) is within 25 px of xVirt=1692 (69 virtual / 3 ≈ 23 downscaled ≈ within kMinSeparationDs).

3. **The final pick at xVirt=1761** has leftNbr=1.6 (paper on left), rightNbr=150.5 (dark photo on right), peakProm=-41.8. This is the **photo's left edge** — the column IS the transition from paper to photo. It was accepted via `paperToDarkBoundary` or `bindingAgainstText` or `per-row paper-adjacent`.

4. **The real gutter** is at ~1620 but its NMS candidate fails because only 51% of rows are dark. The gutter crease is faint. The `kMinDarkRowFraction` threshold of 0.55 is too high for this page.

### Likely fix direction for page 32:

Lower `kMinDarkRowFraction` from 0.55 to ~0.45 so the gutter at xVirt=1617 (drf=0.51) survives. BUT: verify this doesn't accept garbage candidates on other pages. The drf gate prevents bright columns from qualifying. A 0.45 threshold still requires nearly half of rows to be dark.

Alternatively: the per-x scan (which iterates every column in the anchor window) should also evaluate the column at x≈540 (virtual 1620). If it passes evaluateColumn (via per-row rescue), it enters viable and wins closest-to-anchor. The question is whether it's actually failing evaluateColumn — likely failing drf at that column too.

## Page 72 — Concrete Diagnosis

Layout: full-bleed idol photo on LEFT page (dark, extends full height to binding), grid of 6 plaque photos + text below on RIGHT page. Gutter between them.

### What the detector does:

```
[pdf=72] [STRONG-NORMAL] preferring strong normal spine over anchor pick:
         was xCenter=511 (normal) -> xCenter=599 peakProm=45.7 anchor=551.3
[pdf=72] [ANCHOR-PICK] overriding global max:
         was xCenter=703 mean=126 (distFromAnchor=152)
         → now xCenter=599 mean=106.5 (distFromAnchor=47.7)
[pdf=72] spine at 1797 meanDark=106.5 drf=0.56 leftNbr=60.8 rightNbr=55.0 peakProm=45.7
```

### NMS candidates for page 72:

```
#0 xVirt=2133 mean=117 drf=0.58 left=86  right=81  peakProm=32  [viable]
#1 xVirt=1797 mean=106 drf=0.56 left=61  right=55  peakProm=46  [CHOSEN]
#2 xVirt=1533 mean=103 drf=0.98 left=83  right=17  peakProm=20  [viable]
#3 xVirt=1911 mean=96  drf=0.52 left=59  right=57  peakProm=37  [gate-fail]
#4 xVirt=1989 mean=91  drf=0.54 left=63  right=52  peakProm=28  [gate-fail]
#5 xVirt=1704 mean=88  drf=0.52 left=53  right=58  peakProm=30  [gate-fail]
#6 xVirt=1389 mean=71  drf=0.34 left=67  right=15  peakProm=4   [gate-fail]
#7 xVirt=1161 mean=67  drf=0.36 left=66  right=55  peakProm=2   [gate-fail]
```

### Key observations:

1. **NMS #2 at xVirt=1533** is the closest to the gutter and is [viable] with drf=0.98, right=17 (paper), peakProm=20. This is a real gutter candidate with paper on its right and moderate darkness. It's at virtual 1533, which is 121 virtual px LEFT of anchor (1654). distFromAnchor = 121/3 ≈ 40 downscaled.

2. **NMS #1 at xVirt=1797** is the final [CHOSEN]. It has drf=0.56, peakProm=45.7, left=61, right=55 — both neighbors are moderately dark (not paper). This is inside the plaque grid. distFromAnchor = (1797-1654)/3 ≈ 48 downscaled.

3. **The STRONG-NORMAL override** kicked in and MOVED the pick FROM xCenter=511 (which is virtual 1533, the real gutter!) TO xCenter=599 (virtual 1797, the plaque grid). It preferred xCenter=599 because peakProm=45.7 > kStrongNormalPeakProm=20.

4. **This is a regression caused by the strong-normal override.** The default closest-to-anchor pick (xCenter=511, distFromAnchor=40) was CORRECT. The strong-normal override moved it to xCenter=599 (distFromAnchor=48) because 599 had higher peakProm. The override was designed to protect against boundary-replacing-good-normal, but on page 72 it's replacing the correct gutter with a stronger-but-wrong photo-interior candidate.

### Likely fix for page 72:

Remove or weaken the STRONG-NORMAL override. On page 72, the closest-to-anchor pick (default) was correct. The override destroyed it. The override was added to fix pages 6 and 21, but those pages work via per-row rescue now, so the override may no longer be needed.

Specifically: candidate at xVirt=1533 (drf=0.98, right=17) looks like a real gutter. Candidate at xVirt=1797 (drf=0.56, left=61, right=55) does not — both sides are moderately dark, neither is paper. A strong candidate with no paper-bright side is likely photo interior, not a gutter.

## Page 7, Page 19

Not yet diagnosed. Page 7 has a "slightly off" gutter split on a text-text spread. Page 19 has a regression where the split goes into the right-page illustration (Roman forts). Both need tagged log analysis.

## Rendered Test Images

Available in `/tmp/st-fix-pages/`:

- `page-006-006.png` — Page 6 (Frank's Casket, good)
- `page-020.png` — Page 20
- `page-021.png` — Page 21 (aerial photo / White Horse, good)
- `page32-032.png` — Page 32 (text / barn photo, BAD)
- `page65-065.png` — Page 65 (text / wagon photo, good)
- `page69-069.png` — Page 69 (good)
- `page-072-072.png` — Page 72 (idol / plaques, BAD)

To render additional pages:

```bash
PDF="/private/tmp/fix fix/originals/The lost gods of England_ [2d ed -- Brian Branston -- [2d ed_], London, England, 1974 -- Thames and Hudson -- 9780500110133 -- c796c21dacff93238daef6a517bacfa9 -- Anna's Archive.pdf"
pdftoppm -r 150 -f <N> -l <N> "$PDF" /tmp/st-fix-pages/page-<N> -png
```

## Build And Launch

Before every build, add a `BUILD_LOG.md` entry.

Build command:

```bash
export CLANG_MODULE_CACHE_PATH=$(pwd)/build/.cache && cmake --build build --target scantailor -- -j4
```

If app is running, quit/kill it before build. A previous build failed because the bundle executable was open and got deleted during relink/package.

Launch command:

```bash
"/Users/kazys/Developer/scantailor-weasel/build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre"
```

Always launch after rebuild.

## What Has Been Changed (Session Summary)

### Changes that WORKED (keep these):

1. **Per-row paper-adjacent rescue** in `evaluateColumn()`: samples the column and its two neighbor stripes per-row, counts rows where at least one neighbor is paper-bright (pixel ≥ 230) and the column is ≥ 50 darker than that paper neighbor. Accepts at ≥ 0.30 fraction. This fixed pages 6 and 21 where full-stripe neighbor means blended photo + paper into mid-gray and failed the gates.

2. **Per-x scan opened to all viable candidates** (not just boundary): the scan of columns in the anchor window now adds ANY column passing evaluateColumn to the viable set (previously filtered `!boundary`, discarding normal candidates). This lets the per-row rescue deliver its candidates into the viable set.

3. **Page-tag logging**: all qDebug lines in SpineDarknessFinder and PageLayoutEstimator are prefixed with `[pdf=N sub=X]` for grepping.

### Changes that are HARMFUL (fix or remove):

1. **STRONG-NORMAL override** (lines ~784-805 in SpineDarknessFinder.cpp): scans viable for non-boundary candidates with peakProm ≥ 20, picks the closest-to-anchor among those. On page 72, this override MOVED the pick from the correct gutter (xVirt=1533, drf=0.98, rightNbr=17=paper, peakProm=20) to a wrong photo-interior candidate (xVirt=1797, drf=0.56, leftNbr=61, rightNbr=55, peakProm=46). The override was designed for pages 6 and 21 but those now work via per-row rescue without the override. **Recommend removing entirely.**

### Unsolved problems:

1. **Page 32**: the real gutter at xVirt=1617 has drf=0.51 which fails `kMinDarkRowFraction=0.55`. The gutter crease is faint on this page. Lowering the threshold to 0.45 might help but needs testing across the project.

2. **Page 72**: the strong-normal override picks a photo-interior candidate over the correct gutter. Removing the override should fix this. The closest-to-anchor default was already correct.

3. **Pages 7, 19**: not yet diagnosed with tagged logs.

## Key Algorithm Constants

In `SpineDarknessFinder.cpp`:

| Constant | Value | Purpose |
|----------|-------|---------|
| `kMinMeanDarkness` | 25.0 | Min darkness to even consider a column |
| `kMinDarkRowFraction` | 0.55 | Min fraction of rows that are "dark enough" |
| `kMinDarknessProminence` | 18.0 | Mean darkness must exceed median by this |
| `kMinNeighborContrast` | 14.0 | Mean darkness - lighterNeighbor ≥ this |
| `kMaxPaperNeighborDarkness` | 70.0 | At least one neighbor must be ≤ this |
| `kMinPeakProminence` | 14.0 | Mean - darkerNeighbor ≥ this for normal acceptance |
| `kPerRowPaperBrightness` | 230 | Pixel value for per-row "paper" test |
| `kPerRowMinDrop` | 50 | Column - paper neighbor darkness drop |
| `kPerRowPaperAdjacentAcceptFrac` | 0.30 | Min fraction of paper-adjacent dark rows |
| `kStrongNormalPeakProm` | 20.0 | Threshold for strong-normal override (REMOVE THIS) |
| `kBoundaryAnchorWindowFraction` | 0.10 | Per-x scan window ±10% of page width |

## Specific Next Steps (Recommended)

1. **Remove the STRONG-NORMAL override** (lines ~784-805). This should fix page 72 immediately.

2. **Lower `kMinDarkRowFraction`** from 0.55 to 0.45. This should let page 32's gutter candidate (drf=0.51) survive and enter viable. Verify across all pages.

3. **Diagnose pages 7 and 19** using tagged logs.

4. **After each change, ask the user to verify** specific pages before making the next change. Do NOT stack multiple speculative changes.
