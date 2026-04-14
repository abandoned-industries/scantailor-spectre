# Page Split Auto Detection Handoff

## Current State: detector v30

This handoff covers the page-split detector work after Version 2.0a24,
including the current Medina/Lasansky art-book testing.

Version 2.0a24 was not primarily a page-split release. It fixed Page Box
Apply To behavior, build timestamp generation, and toolchain compatibility.
The page-split detector still mostly depended on the older geometry/Vision
flow: find a plausible two-page spread, accept a central-ish split, and use
manual correction for harder art-book spreads.

Version 2.0a25 introduced the major page-split detector work:

- `SpineDarknessFinder` became an active refinement path, not just a fallback.
- Apple Vision text-region and page-number evidence became an anchor for the
  dark-spine search.
- Detector params are invalidated by `kPageSplitDetectorVersion`; current
  value is 30 in `src/core/filters/page_split/Dependencies.cpp`.

## Current Test Project

Medina/Lasansky project:

`/Users/kazys/Downloads/d-medina-lasansky-the-renaissance-perfected-architecture_project/d-medina-lasansky-the-renaissance-perfected-architecture_project.ScanTailor`

Source PDF:

`/Users/kazys/Downloads/d-medina-lasansky-the-renaissance-perfected-architecture_project/originals/d-medina-lasansky-the-renaissance-perfected-architecture.pdf`

Useful reference renders created during debugging:

- `/tmp/st-medina-target/medina-pdf-045-pages-46-47-reference.jpg`
- `/tmp/st-medina-target/medina-pdf-045-center-green-refined-red.jpg`
- `/tmp/st-medina-target/medina-pdf-046-broad-yellow-center-green-current-red.jpg`
- `/tmp/st-medina-target/medina-pdf-048-candidates.jpg`
- `/tmp/st-medina-pp-075.jpg`
- `/tmp/st-medina-pp-076.jpg`
- `/tmp/st-medina-pp-077.jpg`

## Accepted Medina Results

User-accepted or acceptable as of the current session:

| Source PDF page | Printed pages | Current intent |
| --- | --- | --- |
| 43 | 42/43 | Good after allowing a strong leftward broad-gutter snap |
| 45 | 46/47 | Good after avoiding the old right-edge drift |
| 46 | 48/49 | Not perfect, but acceptable; avoid disrupting it |
| 48 | 52/53 | Use Vision center when only one page number is found and gutter evidence exists |
| 63 | 82/83 | Good; avoid right-edge broad-gutter snap |
| 75 | 106/107 | Good with photo-left/text-right gutter rescue |
| 76 | 108/109 | Good with photo-left/text-right gutter rescue |
| 77 | 110/111 | Good with photo-left/text-right gutter rescue |

The v30 Medina log showed PDF page 43 moving from Vision's exact split
`2544.5` to the broad-gutter column `2454`. PDF page 48 stayed at `2544.5`,
because its broad band was closer to the Vision anchor and the user had
already accepted that page.

## Regression Projects

Anselm Jappe / Guy Debord project:

`/private/tmp/fix fix/AnselmJappe-GuyDebord-UniversityofCaliforniaPress1999_1_project/AnselmJappe-GuyDebord-UniversityofCaliforniaPress1999_1_project.ScanTailor`

This file has very light or nonexistent gutters. It caught two over-broad
rescues:

- weak text-block edges were accepted by the per-row paper-adjacent rescue
- isolated dark strokes with paper on both sides could still win when they
  were off the Vision/geometric anchor

The fixes were to require binding-strength darkness and continuity for the
per-row rescue, and to reject isolated off-anchor dark strokes. User checked
the current build and called it clean. Version 2.0a20 only showed a page 2
odd-size result on this file, which was not considered a final-output failure.

Virilio project:

`/private/tmp/fix fix/Virilio_Speedandpolitics_2007_project/Virilio_Speedandpolitics_2007_project.ScanTailor`

User checked current detector v30 and reported it is fine.

Branston `fix fix` project:

`/private/tmp/fix fix/fix fix.ScanTailor`

User checked current detector v30 and reported no regressions.

## Approaches Taken

### 1. Spine darkness refinement

`SpineDarknessFinder` scans vertical candidate columns around an anchor and
tests whether they look like a real gutter. The main signals are:

- mean darkness of the candidate column
- dark-row fraction, meaning how much of the page height contains the mark
- prominence versus the surrounding search window
- contrast against left/right neighbor stripes
- whether at least one side looks like paper for ordinary text-page gutters

This is the core difference from 2.0a24. Instead of trusting a central split
because Vision or geometry said "two pages", current code asks whether a
visible binding mark exists near the anchor.

### 2. Candidate ranking by vertical persistence

Earlier detector versions often selected image edges or illustration edges.
Those edges can be dark, but they usually occupy only part of the page height.
The current ranking prefers high dark-row-fraction candidates when any exist,
because real binding shadows often run nearly full height.

This was the v24/v25-era direction in `SpineDarknessFinder.cpp` and is still
the correct structural approach.

### 3. Gate reorder

The old evaluation order could reject good gutters before rescue paths ran.
On photo-heavy spreads, a dark photograph raises the median darkness of the
search window, so a real gutter can fail the prominence gate too early.

The current evaluator computes neighbor stripes and runs narrow rescue paths
before the global prominence rejection. This lets specific high-confidence
patterns survive without lowering global thresholds for every page.

### 4. Apple Vision as an anchor, not a final answer

Apple Vision gives useful text-region and page-number evidence. Current code
tracks:

- `hasPageNumbers`: page numbers found on both sides
- `hasAnyPageNumber`: page number found on either side
- `rightmostLeftTextX`: text-boundary anchor for one-sided text cases

When both page numbers are found, Vision is strong evidence that the spread
really is two pages. When only one page number is found, the code is more
conservative: a nearby broad gutter can validate the Vision center, but the
detector avoids snapping to a dubious stripe.

### 5. Broad dark-gutter handling

The Medina pages exposed a new failure mode: the visible gutter is not a thin
line but a broad dark band. The normal thin-spine finder can slide to one edge
of that band.

Current behavior:

- scan a narrow window around the Vision anchor for a persistent, modest-dark
  broad gutter band
- allow leftward snaps when they match accepted Medina cases; in one-page-
  number mode this requires a stronger leftward offset so page 43 can move
  while page 48 remains anchored
- prevent large rightward snaps, because page 63 showed that the right edge
  of a gutter band can cut into the right page
- for one-page-number evidence, use the broad band as validation and keep the
  exact Vision anchor unless the leftward broad band is far enough away to be
  the safer physical gutter

### 6. Photo-left/text-right gutter rescue

Printed pages 106/107, 108/109, and 110/111 have a dark full-height gutter
left of the geometric center. The normal "one neighbor must look like blank
paper" rule rejected it, because the left neighbor is photograph/illustration
and the right neighbor is text or gutter shadow.

The current rescue runs before the near-anchor broad-gutter shortcut. It
accepts a very dark full-height candidate left of Vision's anchor only when:

- Vision has page-number evidence
- the candidate is very dark
- the dark-row fraction is high
- both neighbor bands are dark enough to match photo/text material, not
  ordinary paper-shadow cases

This is deliberately narrow. It should not become a general "darkest left
column wins" rule.

### 7. No-gutter regression guards

Light/no-gutter books must not be forced into the Medina art-book rescue
paths. The Anselm Jappe project showed that a weak text edge can look
paper-adjacent row by row, and an isolated off-anchor mark can look dark while
both sides are blank paper.

Current behavior:

- per-row paper-adjacent rescue needs enough mean darkness and dark-row
  fraction before bypassing normal gates
- isolated off-anchor dark strokes are rejected when both neighbor stripes are
  near paper
- broad-gutter and photo/text rescues only run from Vision page-number
  evidence, not arbitrary light-gutter spreads

## Failed or Risky Approaches

Do not revive these without a new, page-specific reason:

- paper-gap or pale-corridor midpoint detectors
- "geometric center is good enough" fallbacks when a visible gutter exists
- strongest near-center vertical edge overrides
- broad early overrides that bypass the normal gate sequence
- global lowering of darkness, neighbor, or prominence thresholds

These repeatedly fixed one screenshot while regressing another. The safer
pattern is a narrow acceptance path inside `SpineDarknessFinder::findSpine`,
guarded by concrete evidence and verified against named pages.

## Logs and Commands

Build command:

```bash
export CLANG_MODULE_CACHE_PATH=$(pwd)/build/.cache && cmake --build build --target scantailor -- -j4
```

Launch Medina project:

```bash
"/Users/kazys/Developer/scantailor-weasel/build/ScanTailor Spectre.app/Contents/MacOS/ScanTailor Spectre" \
  "/Users/kazys/Downloads/d-medina-lasansky-the-renaissance-perfected-architecture_project/d-medina-lasansky-the-renaissance-perfected-architecture_project.ScanTailor"
```

Useful log filters:

```bash
rg -n -C 4 '\[pdf=(45|46|48|63|75|76|77)|stored params pdfPage= (45|46|48|63|75|76|77)|PHOTO-TEXT-GUTTER|ANCHOR-BROAD-GUTTER' /tmp/st-medina-*.log
```

Render source PDF pages for offline inspection:

```bash
pdftoppm -jpeg -f 75 -l 77 -r 300 \
  "/Users/kazys/Downloads/d-medina-lasansky-the-renaissance-perfected-architecture_project/originals/d-medina-lasansky-the-renaissance-perfected-architecture.pdf" \
  /tmp/st-medina-pp
```

## Guardrails For Future Work

- Keep page 43 / printed 42/43 on the leftward broad-gutter snap.
- Keep page 46 / printed 48/49 acceptable. The user said it is not perfect,
  but ok.
- Keep page 63 / printed 82/83 behavior. The user said it is nice.
- Keep page 75, page 76, and page 77 behavior. The user said all are good
  enough.
- Keep Anselm Jappe, Virilio, and Branston clean; these are the current
  regression set for light/no-gutter and older successful cases.
- Bump `kPageSplitDetectorVersion` for detector behavior changes.
- Add a `BUILD_LOG.md` entry before every build.
- Prefer one narrow, explainable rescue over broad threshold changes.
