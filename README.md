# ScanTailor Spectre

<img width="256" height="256" alt="scantailor-spectre" src="https://github.com/user-attachments/assets/a3988c3d-d80e-4089-9418-2bd8302b4b63" />

**Version 2.0b7** | macOS (Apple Silicon) | Requires macOS 15 or later

## New in 2.0b7: a redesigned workspace

ScanTailor Spectre turns scanned books into cleaned-up, searchable PDFs. Version 2.0b7 introduces a more modern interface: explicit processing buttons, progress you can follow without losing sight of your pages, and inline review cards instead of summary dialogs.

If you are upgrading from 2.0b3, the ten-stage workflow is familiar, but the workspace has changed:

- Run a stage with **Process all pages**, or reprocess the page in front of you with **This page only**. Its settings sit under **Adjust**, which remembers whether you left it open or closed.
- Keep the stage list and thumbnails in view while processing. A page count, progress bar and **Stop** button show what is happening; completion marks and a pages-waiting count show what remains.
- Review **pages to check** directly in the workspace after Split Pages, Page Box, Select Content or Margins. Each card offers **Fine** and a **Fix** menu, so you can accept an unusual page or correct it without a separate summary dialog.
- Navigate from the **Pages** header, with color-mode filters and numbered thumbnails. Page Up/Down now scroll the thumbnail list without changing the page you are editing; Q/W and the arrow keys still change pages.
- Export from a **Your book is ready.** panel with **Export PDF…** in a fixed footer and resolution, PDF settings and metadata under **Adjust**. The Export panel also stays stable at window sizes that previously made it shake.
- Start with **Import PDF**, **Import Folder** or **Open Project**, or reopen a recent project by name and folder.

The redesign changes how you run and review your work, not the need to inspect the results. See the [quick start](#quick-start) below and the [release details](#version-20b7) for navigation changes and known limitations.

ScanTailor Spectre is not intended for copyrighted works, but rather for works that you have the rights to or are in the public domain.

---

## Quick Start

### 1. Import Your Scans

From the startup screen or from the file menu:
- **Import PDF** - Extract pages from an existing PDF
- **Import Folder** - Load a folder of scanned images
- **Open Project** - Load a project you have previously saved.

**Supported formats:** PDF, TIFF, PNG, JPEG, JPEG 2000 (.jp2), HEIC/HEIF, WebP, BMP, GIF, PNM (.pbm/.pgm/.ppm)

Recently saved projects will be shown here.  

### 2. Work Through the Filters

Each filter stage refines your pages. Work top to bottom:

| # | Stage | Purpose |
|---|-------|---------|
| 1 | **Fix Orientation** | Rotate pages right-side up |
| 2 | **Split Pages** | Separate two-page spreads |
| 3 | **Deskew** | Straighten tilted scans |
| 4 | **Page Box** | Define page boundaries |
| 5 | **Select Content** | Crop to content area |
| 6 | **Margins** | Set page margins |
| 7 | **Finalize** | Choose B&W, Grayscale, or Color |
| 8 | **Output** | Apply image processing |
| 9 | **OCR** | Text recognition for searchable PDFs |
| 10 | **Export** | Create final PDF |

Select a processing stage, then click **Process all pages** to run it across the book or **This page only** to reprocess the current page. Open **Adjust** to change its settings. Export is different: select **Export**, check its settings under **Adjust**, then click **Export PDF…**.

### 3. Auto Mode or Batch Process

**Auto Mode** (`Cmd+Shift+A`) processes all pages through stages 2-8 with smart defaults. It automatically detects majority page layout, sets deskew to zero, preserves layout for low-coverage pages, and detaches size outliers from aggregate sizing. For OCR as well, use `Cmd+Shift+O`. You can stop auto mode at any point with the stop button.

For manual control, work down the stage list using **Process all pages**. Check splitting and page geometry before continuing through Finalize and Output. Run OCR if you want searchable text, then use **Export PDF…** in Export.

After Split Pages, Page Box, Select Content or Margins, an inline **pages to check** view helps you review unusual pages:

- **Split Pages (2)** lets you review split and unsplit scans and correct their layout.
- **Page Box (4)** flags unusual page boundaries.
- **Select Content (5)** flags low-coverage pages whose original layout you may want to preserve.
- **Margins (6)** flags size outliers and possible unsplit spreads.

Use **Fine** to accept a card or its **Fix** menu to jump to the page or apply a correction. **All fine** clears the review; **Show all** returns to the page view. Review thresholds and bulk actions are under **Adjust**. These checks do not replace inspecting your output.

---

## The Workflow Explained

Each stage has automatic operations and potentially requires minimal input, but some scans can be more difficult to process than others. For example, a decent scan with only text pages might require virtually no input on your part. A 500 page book with color pages, some of which are skewed, many of which have color casts and require individual adjustments may require substantial intervention. 

Although ScanTailor Spectre is designed to be as automated as possible, it can make mistakes. It's always best to check your output. 

After you run a stage, the parameters that stage set are stored in your project on a per-page basis. Let's say you set a page to color once. If you rerun the automatic color detection, it will not alter that setting even if it thinks it is grayscale.

This is beta software. Save your project frequently.

Before a new project is saved, generated page images and thumbnails are kept in macOS temporary storage. **Save Project** creates a self-contained project folder and moves a permanent copy into its `output` subfolder; closing an unsaved project may remove the temporary copy. The final PDF is created only when you use **Export PDF…**.

### Stage 1: Fix Orientation

Corrects pages that are upside-down or rotated. Auto-detection handles most cases; use the rotate buttons for manual override. It is rare that you will have to even run this separately unless your scans are awful. This is not the same as Stage 3, Deskew. 

### Stage 2: Split Pages

Separates book spreads (two pages per scan) into individual pages. The split line is auto-detected but can be adjusted manually.

The current detector is substantially different from Version 2.0a24. In
2.0a24, page splitting mostly relied on the older geometry/Vision decision
path and then trusted the center-ish split it found. That worked for clean
text spreads, but it was weak on art books and photo-heavy scans where the
visible gutter is a broad shadow rather than a thin white gap.

Split Pages combines Apple Vision text-region
evidence with a dedicated spine-darkness search. Vision provides a coarse
anchor and page-number evidence; `SpineDarknessFinder` then tests candidate
gutter columns using vertical persistence, local darkness, neighbor contrast,
and several narrow rescue paths for real binding shadows next to photos or
text. The goal is not to make every spread automatic, but to reduce the
number of obvious manual fixes in books with photographs, illustrations, and
asymmetric page layouts.

Recent detector work added:
- page-number-aware refinement, so two detected page numbers count as strong
  evidence for a two-page spread
- conservative handling when only one page number is found
- broad dark-gutter handling, including protection against snapping to the
  wrong edge of a gutter band
- a photo-left/text-right rescue for spreads where the true gutter is dark
  and both neighboring bands are also dark, so the old "one neighbor must be
  blank paper" rule would reject it
- no-gutter regression guards, so light text-only spreads are not forced into
  the photo/book-gutter rescue paths

The detector can still be wrong. For difficult art books, inspect Stage 2
before continuing, especially spreads with full-page photographs, chapter
openers, or images that run into the gutter.

After batch processing, review the inline **pages to check** cards. Under **Adjust**, switch between **Not Split** and **Split** to inspect either group. Use a card's **Fix** menu to jump to the page, force a two-page or single-page layout, or reset it to automatic layout. Use **Fine** when the result is correct.

When applying a manual layout with **Apply to…**, the split line carries to the chosen pages, fitted to each page's outline. If you have dragged the line, the dialog opens on Manual; choosing Auto re-detects the split instead.

### Stage 3: Deskew

Straightens pages that were slightly tilted during scanning. Even 1-2° of tilt is noticeable in output.

**Tip:** Sort by "decreasing deviation" to review the most-skewed pages first. The sort controls are in the **Pages** header above the thumbnails on the right.

**Thumbnail Filters:** The B, G, M, C pills in the **Pages** header filter pages by color mode (B&W, Grayscale, Mixed, Color). Mixed is independent, so you can review pages that combine text and photographs without also showing every color page. Click a button to show only pages of that type.

### Stage 4: Page Box

Defines the physical page boundaries within the scan. This is useful when the scanner captured more than just the page (scanner bed edges, background), or when you want to define a consistent page size across all scans.

**Page Box options:**
- **Disable** - Don't detect page boundaries; use the entire image
- **Auto** - Automatically detect the page edges (looks for the transition from scanner background to paper)
- **Manual** - Draw the page boundary yourself by dragging corners and edges

When Auto mode is selected, **Fine Tune Page Corners** runs a second pass after initial edge detection. It steps each corner diagonally inward, skipping over dark pixels (shadows, bent page edges) until it finds light pixels (actual paper). This is useful for thick books where page corners cast shadows or curl away from the scanner bed.

In Manual mode, you can enter exact **Width** and **Height** values to set a specific page size.

**Sorting:** Use "Order by increasing width" or "Order by increasing height" to quickly find pages with incorrect page boundaries. This is the key advantage of having Page Box as its own stage — you can review and fix page boundaries before running content detection.

### Stage 5: Select Content

Defines the **Content Box** — the area containing actual content like text and images. Everything outside this box becomes white margin in the final output. The content box is constrained within the page box set in the previous stage.

**When do you need this?** The Content Box identifies where text or images are so margins can be standardized. For most workflows, Auto mode handles this well, but you may need Manual adjustments for pages with unusual layouts (title pages, illustrations, fold-outs) or when auto-detection picks up shadows or artifacts as content.

**Content Box options:**
- **Disable** - Don't detect content; use the entire page
- **Auto** - Automatically find text and images on the page
- **Manual** - Draw the content area yourself

#### Detection Settings

When using Auto mode for Content Box, these settings control how content is detected:

- **Fill Factor** (0.50-1.00, default 0.65) - Controls what density of content is accepted. The default is optimized for text documents. For art books, photo-heavy pages, or pages with large dark areas, increase this value:
  - **0.65** - Default, works well for text documents
  - **0.85** - Better for mixed text/image content
  - **0.95+** - Use the entire page as content (best for art books, covers, full-page images)

- **Border Tolerance** (0-20px, default 2) - Controls how much content touching page edges is preserved:
  - **Low (0-5)** - Aggressively trim edge content (removes shadows, scanner artifacts)
  - **High (15-20)** - Preserve content that extends to page edges (for images that bleed to margins)

**Tip:** For art books or pages with large photographs, set Fill Factor to 0.95+ to capture the entire page content. The default settings are optimized for text documents and may clip images.

This excludes:
- Scanner bed edges
- Book margins you want to remove
- Fingers or page holders

Auto detection is not always perfect, especially with images or decorative elements. Sort by "Order by completeness" to review pages that may need manual adjustment.

#### Review content coverage

After batch processing, **pages to check** lists pages whose detected content covers less than the threshold percentage of the page area. Chapter titles and short paragraphs may have intentional spacing that should not be centered.

Under **Adjust**, use the **Coverage threshold** slider (20%-80%) to choose which pages are listed. A card's **Fix** menu lets you jump to that page or preserve its layout by disabling content detection. **Preserve layout (all listed)** applies that choice to the whole list. Use **Fine** to accept a page without changing it.

### Stage 6: Margins

Sets white space around content in the final output and page size.

- **Top/Bottom/Left/Right** - Individual margin sizes
- **Alignment** - Where content sits within the page

### Stage 7: Finalize

This stage determines how each page will be processed:

- **Color Mode**: Black & White, Grayscale, or Color
- **Output Format**: TIFF or PNG (you can ignore this if you don’t care to save the individual images)
- **Output Location**: Where processed files are saved

The app auto-detects the appropriate color mode. Obviously, Color pages require the most storage space, Grayscale requires less, and Black and White the least of all.

The **Midtone Threshold** slider adjusts detection sensitivity, useful for smaller images (lower = more pages will be inspected and detection will be slower). You can override the determination after inspection.

**White Balance** (Color and Grayscale modes):
- **Force white balance** - Automatically corrects color casts from aged paper or poor lighting
- **Pick Paper Color** - Click this, then click on an area that should be white/neutral in the preview. The app will correct the entire page based on that sample.

**Clear All Pages** resets all pages to unprocessed state, useful if you want to re-run automatic detection with different settings.

Generated images belong to the project workflow; they are not the final PDF. Save the project to keep its source images, processed images and settings together. Unsaved projects use temporary storage (see [Project Files](#project-files)).

### Stage 8: Output

Applies image processing to generate the output files. Options vary by color mode.

**Output Resolution**: Sets the DPI of the output image. 400 DPI is the Library of Congress recommendation for most documents. Higher = sharper but larger files.

#### Black & White Mode Options

- **Binarization Threshold** - Controls the cutoff between black and white. Lower values = more black, higher = more white. Adjust if text appears too thin or too bold.
- **Normalize Illumination** - Evens out lighting variations across the page. Helps with scans that have shadows or uneven exposure.
- **Morphological Smoothing** - Smooths jagged edges on text and lines.
- **Morphological Opening/Closing** - Advanced cleanup options for removing small artifacts.

#### Grayscale & Color Mode Options

- **Normalize Illumination** - Evens out lighting variations.
- **Sharpen/Blur filters** - Enhance or soften the image.

#### Pass-through Mode

Enable **Pass-through** to skip heavy processing (binarization, despeckling, dewarping) while still applying adjustments. Useful for:
- Pre-processed pages that only need brightness/contrast tweaks
- Covers and illustrations that should remain untouched except for tonal adjustments
- Pages where automatic processing produces poor results

When pass-through is enabled, the page is scaled to output DPI and adjustments (brightness, contrast, auto levels) are applied, but no other processing occurs.

#### Brightness & Contrast

- **Brightness** - Adjusts overall image lightness. Slide right to brighten, left to darken.
- **Contrast** - Adjusts tonal range. Slide right to increase contrast, left to decrease.
- **Auto Levels** - Stretches the histogram to use the full 0-255 range for maximum contrast.

The sliders have tick marks at -100, 0, and +100, with a center detent that snaps to 0 for easy reset. These adjustments apply even when pass-through mode is enabled.

#### Paper Detection (Equalize Illumination)

When "Equalize illumination (Color)" is enabled, these advanced controls appear:

- **Min Brightness** (0-255, default 120) - Pixels brighter than this may be paper
- **Max Saturation** (0-255, default 60) - Pixels less saturated than this may be paper
- **Min Coverage** (0-50%, default 1%) - Minimum paper area required for equalization
- **Adaptive (sample margins)** - Auto-detect paper color from page margins

These controls help the algorithm distinguish paper from content when correcting uneven lighting.

#### Common Options (All Modes)

- **Dewarping** - Flattens curved pages from book spines. Set to "Auto" for automatic detection or "Manual" to adjust control points yourself. Most useful for thick books where pages curve near the spine.
- **Despeckle** - Removes small dots and noise:
  - *Cautious* - Minimal cleanup, preserves detail
  - *Normal* - Balanced approach
  - *Aggressive* - Heavy cleanup, may remove fine details
- **Equalize Illumination** - Additional lighting correction.
- **White Margins** - Ensures margins are pure white.

#### Zones

- **Picture Zones** - Draw rectangles around photographs or illustrations. These areas are processed differently to preserve detail and gradients instead of being binarized.
- **Fill Zones** - Draw areas to fill with a solid color (usually white). Useful for removing stamps, stains, or unwanted marks.

To draw a zone, select the zone tool, then click and drag on the preview image.

#### Apply to…

Once you've tuned settings for one page, use **Apply to…** to copy those settings to other pages. You can apply to:
- All pages
- Selected pages only
- Pages with the same color mode

This is essential for efficiently processing large documents.

### Stage 9: OCR

Performs optical character recognition to make your PDF searchable. When enabled, text is recognized and embedded as an invisible layer in the exported PDF, allowing you to search and select text.

**Options:**
- **Enable OCR** - Toggle text recognition on/off (enabled by default)
- **Language** - Select the document language for better recognition accuracy, or leave on Auto-detect
- **Accurate Recognition** - Use accurate mode for best results (slower) or fast mode for quicker processing

**Status Display:**
- Shows whether the current page has been processed
- Displays the number of text blocks found

Use **Process all pages** in OCR, or Auto Process All + OCR, to recognize text before exporting. Results are cached per-page, so re-running won't re-process pages that already have OCR data unless you clear them.

### Stage 10: Export

Creates the final PDF. The **Your book is ready.** panel keeps **Export PDF…** in a fixed footer while settings scroll separately. Open **Adjust** for resolution, PDF settings and book metadata. For Zotero projects, the return-to-Zotero option sits above the export button.

**Max DPI:** Limits output resolution for grayscale and color pages. 

400 DPI is the Library of Congress recommendation for most documents.
If your scan is lower quality or to be read on screen only, you may find that lowering the resolution as low as 72 DPI gives acceptable results and a much lower final PDF size. At 72 DPI, however, it would be very difficult to read black and white pages, which is why they are controlled by the output resolution in Stage 8 (Output).
 
---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Cmd+Shift+A` | Auto Process All (Split Pages through Output) |
| `Cmd+Shift+O` | Auto Process All + OCR (Split Pages through OCR) |
| `Q` / `W`, arrow keys | Previous/next page |
| `Page Up/Down` | Scroll thumbnails one screenful (does not change the current page) |
| `Home` / `End` | First/last page |
| `Cmd+S` | Save project |

**Finalize (7) & Output (8) Stage Shortcuts:**

| Key | Action |
|-----|--------|
| `C` | Set selected page(s) to Color mode |
| `G` | Set selected page(s) to Grayscale mode |
| `B` | Set selected page(s) to Black & White mode |
| `Shift+C` | Toggle Color page filter |
| `Shift+G` | Toggle Grayscale page filter |
| `Shift+M` | Toggle Mixed page filter |
| `Shift+B` | Toggle B&W page filter |

**Output (8) Stage Only:**

| Key | Action |
|-----|--------|
| `P` | Toggle Pass-through mode |

## Tips

### Scanning
- **300-400 DPI** for text documents
- **600 DPI** for fine detail or small text
- Scan in **color** even for B&W content - better conversion results

### Processing
- Work through stages **in order** - each depends on the previous
- Configure **one page well**, then batch apply to similar pages
- Use **page ordering** options to find problem pages
- **Save frequently** - all settings are preserved in the project file

### Color Mode Selection
- **B&W (1-bit)**: Smallest files, pure black and white, ideal for text-only
- **Grayscale**: Better for pages with photos or illustrations
- **Color**: Only when color information matters

### Typical File Sizes (100-page book)
- B&W PDF: 5-15 MB
- Grayscale PDF: 20-50 MB
- Color PDF: 50-150 MB

## Project Files

**Save Project** creates a self-contained folder containing:

- The visible `<project-folder>.ScanTailor` project file
- Copies of the source images in `originals`
- Generated page images and thumbnails in `output`
- All settings and processing state

The exported PDF is separate. Create it from Stage 10 with **Export PDF…**.

## Zotero Integration

ScanTailor Spectre can round-trip a scan with Zotero: pick a PDF in your Zotero library, clean it up in ScanTailor Spectre, and the exported PDF returns to the same Zotero item as a new attachment. The original Zotero-managed file is never modified.

### Installing the Zotero Plugin

The round trip is driven by **ScanTailor Spectre Loop**, a small companion plugin for Zotero. The plugin ships inside ScanTailor Spectre, so it remains available after you move the app to Applications and eject the disk image. Choose **Help → Install Zotero Plugin…** in ScanTailor Spectre to reveal `st-spectre-loop.xpi` in Finder. A second copy is also included next to the app on the disk image.

To install it:

- In Zotero, open **Tools → Plugins**
- Click the gear menu and choose **Install Plugin From File…**
- In ScanTailor Spectre, choose **Help → Install Zotero Plugin…**, then select the revealed `st-spectre-loop.xpi`

Zotero must be running when you export — the cleaned PDF is handed back through Zotero itself.

### Cleaning a Scan from Zotero

- Right-click a book or its PDF in Zotero and choose **Clean scan in ScanTailor Spectre…**
- A dialog confirms which item and PDF you picked and where projects will be kept. The first time, click **Change…** and pick a projects folder — nothing is written anywhere until you have chosen one.
- Click **Clean Scan**. A copy of the PDF goes into a new folder named after the item, inside your projects folder, and ScanTailor Spectre opens it.

If an item has several PDFs, the dialog lets you pick which one; if you right-clicked a specific PDF, exactly that file is used.

### Returning the Cleaned PDF

Work through the stages as usual. For Zotero round-trip projects, **Return to Zotero** in the Export stage is turned on automatically (unchecking it is remembered for that project). When you export, the finished PDF is attached to the original Zotero item, and the export also succeeds locally even if Zotero is unreachable at that moment.

## Features
- **Auto Mode** - Processing from Split Pages through Output, with optional OCR and smart defaults: majority-vote page splitting, zero deskew, content outlier detection, and size outlier handling
- **Apple Silicon Native** -  Gatekeeper-friendly application
- **PDF Import** - Open PDFs directly, no need to extract pages first
- **Inline review** - Pages-to-check cards after Split Pages, Page Box, Select Content and Margins, with direct correction actions
- **Detection Settings** - Adjustable Fill Factor and Border Tolerance for art books and photo-heavy content
- **Finalize Stage** - Dedicated stage for color mode selection and output format
- **Intelligent Color Detection** - Auto-detects B&W vs grayscale vs color pages, including embedded photographs
- **Photo Adjustments** - Temp, Tint, Exposure, Contrast, Highlights, Shadows, Whites, Blacks sliders with Auto and Reset. Exposure covers -1.00 to +1.00 f-stops in fine 0.01-stop steps.
- **Packaged Web Output Panel** - Output stage web controls now ship with the required Qt WebEngine helper dependencies in the app bundle
- **OCR Stage** - Automatic text recognition for searchable PDFs
- **Export Stage** - PDF export with quality presets, metadata under Adjust and a fixed export footer

---

## Credits

ScanTailor Spectre is based on:
- [ScanTailor Advanced](https://github.com/ScanTailor-Advanced/scantailor-advanced) by 4lex4
- [ScanTailor](https://scantailor.org/) by Joseph Artsimovich

## License

GPL-3.0 - See [LICENSE](LICENSE) for details.

---

## ScanTailor What? 

ScanTailor Spectre is a Mac-native fork of ScanTailor Advanced, developed by Claude. Several considerations informed the choice of its name.

First, the name serves to distinguish it from other community releases, including ScanTailor, ScanTailor Advanced, and ScanTailor Experimental.

Second, the title references the opening line of the Communist Manifesto: "A spectre is haunting Europe." Jacques Derrida, in "Spectres of Marx" wrote that the dead refuse to remain absent. For Derrida, this refers to the spectre of Marxism, but it can also refer to the haunting of the Internet by the ongoing persistence of ScanTailor as well as to the scanned book existing as a ghostly trace circulating online rather than as a physical object.

Third, ScanTailor Spectre is developed using Claude Code. The name also draws attention to the role of unauthorized book scans in the development of large language model artificial intelligences. It is illegal to copy and distribute copyrighted books, and we do not condone using ScanTailor Spectre for such purposes. Both Anthropic and Meta trained their models on millions of books obtained from shadow libraries such as Library Genesis, which are motivated by a vision of information freedom championed by Aaron Swartz. Recently, Anthropic settled with authors for $1.5 billion, equating to approximately $3,000 per book, a sum that exceeds the lifetime earnings of most books. In contemporary society, intellectual property is controlled not by individuals, but by those in positions of power.

Lastly, the first application I vibe-coded was a clone of the 1990s game Spectre VR. 

The "spectre in the machine" draws on all of these. 

## Version History

### Version 2.0b7

This release brings a redesigned workspace to users upgrading from 2.0b3. The processing stages remain, but their actions, settings and review results now have distinct places in the window. Page Up/Down navigation has changed as described below.

**The left column**

- Processing stages now open with two buttons at the top: **Process all pages** runs the stage over the whole book (the small play button that used to sit in the stage list is gone; this button does its job), and **This page only** reprocesses the page you are looking at.
- Under the buttons is a fold labeled **Adjust**. Click it to hide or show the stage's controls. Each stage remembers whether you left its fold open or closed, and reopens it that way next time. On first run every fold is open.
- The stage list shows a check mark on stages that are finished for every page and a half circle on stages that are partly done. The stage you are on has a blue bar at its left edge.
- The bottom of the column shows where you are: **Page 37 of 212**, or **Editing 3 pages** when several are selected, and **N pages waiting** when the current stage still has unprocessed pages. The **Editing N pages** label that used to sit inside each stage's controls is gone; this footer replaces it.
- The column is never narrower than its contents. If the two buttons cannot fit side by side, the second drops under the first. Nothing runs off the right edge, scrollbar included.

**Thumbnails**

- The tool strip beside the thumbnails is now a two-row header under the word **Pages**. Every button that was there before is still there; the B, G, M and C color toggles are now small pills.
- ⌘-click on the header's ▲ or ▼ goes to the first or last page, same as Home and End.
- ⇧-click on ▲ or ▼, or Page Up and Page Down, scroll the thumbnails one screenful, so the row that was at the top ends up at the bottom. This only scrolls: the current page does not change and nothing is reprocessed. Q and W, the arrow keys and a plain click on ▲/▼ still step one page at a time; Shift+Page Up/Down still go to the previous or next selected page. Page Up and Page Down used to mean previous and next page; they no longer do.
- The caption under each thumbnail is its page number in the current order (**37**). It used to be the file name.
- The thumbnails no longer snap back to the current page while you are scrolling them; they re-center only when the current page actually changes.

**Processing**

- The stage list and thumbnails stay on screen while pages are being processed. The middle shows the stage name, a count such as **40 of 212**, a thin blue progress bar, a **Stop** button, **Beep when finished**, and the **System load** slider with its **Minimal** and **Maximal** ends. Thumbnails fill in as each page finishes.
- The four pop-ups that used to appear after processing (split summary, page box, content coverage, page size) are gone. When some pages fall outside the rest, the middle shows them as cards under a heading such as **3 pages to check**. Each card has the page number, a note such as **152 × 228 mm · +31%**, a **Fine** button and a **Fix** menu with the same actions those pop-ups offered: jump to page, force two-page or single page layout, reset to auto layout, disable page box, preserve layout, detach from aggregate size, go to Split Pages. **All fine** clears the list; **Show all 212** brings back the full page. The threshold slider and the "all listed" button from each pop-up live under **Adjust**. Which pages get flagged, and at what threshold, is unchanged.

**Split Pages**

- The **Apply cut** box in the Apply to… dialog is gone. Choosing a manual layout and a range of pages now always carries your split line to those pages, fitted to each page's own outline. Auto still re-detects and carries no line. Nothing you drew by hand is ever discarded by an apply.
- If you have dragged the split line on a page, the Apply to… dialog opens on **Manual** for that page instead of Auto, so accepting it no longer re-detects the line away. It opens on Auto only when the page is untouched.

**Export**

- The Export panel no longer oscillates (shakes) with each repaint at certain window sizes.
- The Export stage is a card headed **Your book is ready.** with **Export PDF…** pinned at the bottom so it never scrolls out of reach. When a Zotero project is open, the return-to-Zotero line sits just above the button. Resolution, PDF settings and book metadata are under **Adjust**.

**Start window**

- The Start window shows the app icon and version, three large rows (**Import PDF**, **Import Folder**, **Open Project**) with their keyboard shortcuts, and your recent projects with the project name and its folder. There is no Zotero row: a scan sent from Zotero opens straight into its project.

**Everywhere**

- Every button that chooses which pages a setting applies to now reads **Apply to…**, Split Pages included. The three buttons that open a settings editor rather than a page chooser (output resolution, dewarping, export resolution) keep **Change…**.
- The status bar at the bottom of the window still shows mouse position, image size, units and zone mode.
- The file name of the current page is no longer shown anywhere. In 2.0b3 it appeared under each thumbnail; now the thumbnails show page numbers and nothing else carries it.

**Unchanged on purpose**

- Settings, the Project Files dialog, Fix DPI, Relinking, the menus and the keyboard shortcuts (apart from Page Up/Down above) are exactly as in 2.0b3. Dark mode was not checked in this version.

**Known gaps**

- Recent projects on the Start window show only the name and folder, not the page count or the last stage reached.
- "Pages to check" appears only after Split Pages, Page Box, Select Content and Margins. Output does not flag pages.
- The file name of the current page is not shown anywhere (see above).
- There is no zoom readout.
- Dark mode has not been validated for the redesign.

**Compatibility and build corrections**

- The minimum requirement remains **macOS 15 on Apple Silicon**. Runtime verification is limited to the current macOS 27 host; **macOS 15 has not been runtime-tested**. The release candidate was manually tested and accepted by the user on that host; this is not a claim that every workflow or input has been tested.
- Uses official Qt 6.8.3 and checksum-pinned image libraries compiled for macOS 15, without lowering deployment-version metadata after compilation.
- Repairs bundled dependencies, framework IDs and search paths, including nested WebEngine helpers; excludes the unused virtual-keyboard plugin. Metal shaders, translations, documentation and the Zotero companion are included.
- The optional macOS 26 OCR bridge uses compiler-checked availability and retains the earlier-system fallback.
- Includes **knutfh's PR #13 OpenGL/AGL workaround**, with corrected SDK lookup and override handling, plus TIFF public-header propagation and Metal deployment-target flags.

### Earlier releases (archive)

The entries below describe earlier versions, not additional changes in 2.0b7. Historical interface descriptions and benchmark results apply to the versions named. Use the guide above for the current workflow.

### Version 2.0b3

**Released September 1, 2026.**

- Auto Process exports now always carry the OCR text layer. Previously, a project whose OCR stage had been run and then switched off exported a PDF with no searchable text even though the recognized text was already stored; export now embeds whatever OCR results exist regardless of the stage toggle.
- Quitting the app now actually exits it. The process used to linger after Quit, and opening a file into a windowless leftover instance wedged. Both paths are fixed.
- Fixed a crash on the rendering thread when a Mixed page was output with split output, black-and-white foreground, and **Original background** on.
- Projects made from imported PDFs now reopen at the resolution they were imported at. The import DPI was never saved, so a reopened project rendered every PDF page at 300 DPI while the stored page sizes still assumed the import resolution, silently corrupting page boxes, content boxes, and margins. The DPI is now written into the project file.
- The TIFF compression setting in Finalize now takes effect. The control was wired to nothing; black-and-white and color TIFFs now use the compression you choose.
- Cancelling a batch no longer wastes time decoding pages that were already cancelled.
- Fixed a thread-safety problem in the page-deviation statistics used to flag unusual pages during batch processing.
- **Fill margins: black** now works when dewarping is on. Under dewarping the setting was silently ignored and the margins came out white.
- Right-clicking thumbnails and choosing **Convert to Black and White / Grayscale / Color** now re-renders every selected page immediately instead of only the current one.
- New input formats: JPEG 2000 (.jp2), HEIC/HEIF, WebP, BMP, GIF, and PBM/PGM/PPM. Each is recognized with its true dimensions and resolution when the file records one.
- Removed dead code: the phantom libharu dependency, the Metal GPU morphology path, and the unused ImageTypeDetector.
- Under the hood, large pieces of the main window and the output generator were extracted into separate units with no behavior change, and new characterization test suites now pin project-file round trips, frozen project fixtures, output rendering goldens, deviation statistics, and TIFF compression wiring.
- The macOS packaging documents and DMG script now match the release procedure.

### Version 2.0b2

**Released August 22, 2026.**

- The app no longer quits unexpectedly when a screen reader or an automation tool inspects its window. VoiceOver, macOS automation, and assistive software all read an app's controls as it starts up, and a bug in the Qt 6.11.1 toolkit crashed the app the moment any of them looked at a list such as the stage list. Those lists are now described more simply so the crash cannot happen; everything else stays fully readable to assistive tools.
- PDF import now accepts PDFs with junk bytes (such as a UTF-8 BOM) before the `%PDF` header, matching Preview's behavior; some Papers-imported PDFs previously failed with "Failed to read PDF file."
- A PDF that is still being copied into place when you open it is now given a moment to finish instead of being reported as unreadable.
- When a PDF really cannot be read, the message now names the file and says why — missing header, unreadable file, rejected by macOS, or no pages — instead of a bare "Failed to read PDF file."
- Changing a page's color mode in Finalize no longer wipes that page's own Output settings. Black-and-white options, color options, and photo adjustments now survive a mode change instead of reverting to defaults.
- ISBN lookup in Export now queries Open Library and Google Books at the same time under a single 15-second budget, retries Open Library when it stalls, and tells you exactly which source failed and how.
- Right-clicking thumbnails in the Finalize and Output stages now offers **Convert to Black and White**, **Convert to Grayscale**, and **Convert to Color**, applying to all selected pages; the matching c/g/b keyboard shortcuts now also keep the Finalize mode display in sync.

### Version 2.0b1

- Exposure adjustment now uses fine 0.01-stop steps across a focused -1.00 to +1.00 f-stop range, with older out-of-range project values safely clamped.
- Mixed pages now have their own independent thumbnail filter and `Shift+M` shortcut instead of being folded into the Color filter.
- Zotero round trips now ask you to choose their workspace folder, remember that choice, and provide a clear way to change it instead of silently writing to Documents.
- New Zotero projects use visible, folder-named `.ScanTailor` files, while projects created with the former hidden filename still open normally.
- Returning an exported PDF now includes Zotero's required connector protocol header, preventing successful exports from being reported as unreachable.
- Closing the app no longer schedules overlapping save prompts that could crash the window during shutdown.
- The Export stage now keeps **Export PDF** and the Zotero return status in a fixed footer while settings and book metadata scroll independently, and the panel stays reachable even in very short windows.
- Zotero round-trip projects now turn on **Return to Zotero** automatically; unchecking it is remembered for that project.

### Version 2.0a38

- Force B&W Auto mode now handles scanned leaves embedded inside pure-white PDF mattes without turning the leaf black, including sparse title pages, inverted-polarity decisions, and faint low-contrast versos.
- Output normalization now learns illumination from the physical paper rather than the PDF canvas, while preserving genuinely dark, saturated covers.
- Output render caches are versioned so pages produced by the faulty renderer are regenerated automatically.

### Version 2.0a37

- Zotero round-trip projects can now be launched directly from their staging directory. ScanTailor Spectre resumes an existing project or creates and saves one from the staged source PDF.
- The Export stage recognizes Zotero loop sidecars and can return each successfully exported PDF to its original Zotero item as a new attachment, with authenticated error reporting that never invalidates the PDF export.

### Version 2.0a36

- Hotfix: pressing F outside the Margins stage silently switched selected pages to Full Bleed (skipping margin standardization in output) — present since 2.0a32. The shortcut now works only in Margins, where the checkbox is visible. Affected saved projects: select the pages in Margins and uncheck "Full bleed page" to restore uniform margins.

### Version 2.0a35

- The pipeline is now photograph-aware. Validated page-by-page against a 268-page photography monograph alongside the existing text-book corpus.
- Color detection rebuilt on spatial evidence: full-page photographic plates lost to B&W binarization dropped from 139/185 to 4/185; all 19 mixed text+photo pages now correctly preserve their photographs; text-page classification remains exact.
- Geometry stages no longer treat photographs as documents: photo pages are not split, not rotated by scene edges, and keep full-page content. Near-square single leaves never auto-split without physical gutter evidence — two-column layouts no longer masquerade as spreads. Every automatic decision records its reason in the project file.
- Mixed-mode output finds each photograph's printed frame and fills it to exact edges; bright regions inside photos (faces, skies) are no longer bleached to paper.
- Detection results are versioned: improved future detectors automatically re-judge stale automatic decisions while manual choices stay authoritative.

### Version 2.0a34 — The Second Speed Build

**Released July 24, 2026.** Version 2.0a34 is the second major performance
release for modern Apple Silicon Macs. On the same 392-page scanned-book
workflow, observed end-to-end times progressed from **6 m 38 s in v2.0a19** to
**3 m 28 s in v2.0a33**, then to the following choices in v2.0a34:

| OCR setting | 392-page time | Intended use |
|---|---:|---|
| Accurate OCR + language correction | **2 m 48 s** | Highest measured text quality |
| Accurate OCR + skip language correction | **1 m 30 s** | Best speed/quality balance |
| Fast OCR + skip language correction | **29.5 s** | Maximum throughput; largest OCR accuracy tradeoff |

These are measured runs on the reference book, not a promise that every book
or Mac will finish in exactly the same time. Fast OCR is substantially less
accurate than Accurate OCR; use the 1 m 30 s setting when searchable-text
quality still matters.

- Auto Process now keeps decoded PDF pages in a RAM-aware cache and coalesces
  concurrent requests for the same page, avoiding repeated rasterization. It
  also combines compatible pipeline stages and carries Output's newly
  processed in-memory image directly into OCR instead of saving and reloading
  it or running Output twice.
- Mixed mode is now conservatively autodetected from the existing Finalize
  color analysis. It finds localized continuous-tone pictures on otherwise
  document-like pages without adding another full-image detection pass, and
  the choice is preserved correctly through Finalize and Output.
- The OCR stage and Auto Process dialog now expose the same three remembered
  controls: **Fast OCR**, **Multilingual detection**, and **Skip language
  correction**.
- The language choices are now honest about their cost. **System language**
  is the fast default; **Multilingual detection** enables true automatic
  language detection and is deliberately labeled slower. A selected primary
  language is used only when multilingual detection is off.
- OCR result caching now includes the recognition mode, language-correction
  choice, engine schema, and output-file identity, so changing OCR settings or
  regenerating a page cannot silently reuse stale text.
- Fixed unbounded Apple Vision object lifetime growth during long OCR runs and
  retained stable page-parallel Vision OCR as the default.
- macOS 26's new document-recognition engine was investigated using only
  Weasel's newly processed in-memory page images. It can be faster on short
  runs, but concurrent requests crashed inside Apple's framework during
  full-book stress tests, while the stable serialized form was slower than the
  normal engine. It therefore remains research-only and opt-in through
  `SCANTAILOR_DOCUMENT_VISION_OCR=1`; it never reads or reuses text from the
  original PDF.
- Added performance instrumentation and regression coverage for image-cache
  behavior, OCR cache validity, color/Mixed classification, PDF loading, and
  long-run stability. The release keeps the crash-prone document-OCR
  concurrency path disabled.

### Version 2.0a33 — The First Speed Build

**Measured speedup:** a 392-page scanned book ([*The Best Short Stories of 1924*, Internet Archive](https://archive.org/details/bestshortstories1924unse)) auto-processes end-to-end — including accurate-mode OCR — in **3 m 28 s (0.53 s/page)**, versus 6 m 38 s on v2.0a19: about **1.9× faster overall, and 5–6× faster in the processing pipeline itself** (OCR time is Apple Vision's and unchanged). Color classification on the same book improved from 340 pages wrongly detected as Color to 388 correct B&W pages, with only the covers (grayscale) and the bookplate/library card (color) treated specially.

- Major performance overhaul: parallel PDF rasterization, decoded-image cache, coalesced thumbnail updates, batch queue refill before UI work, gated per-page parallelism.
- OCR receives the rendered output image in memory; PDF export moved off the GUI thread with cancellation and atomic file replacement.
- New unified "Auto Process…" dialog: color handling preset (Force B&W / B&W + grayscale / Best guess), OCR on/off, remembered settings.
- Auto-process runs report elapsed time and per-stage breakdown.
- Fixed three long-standing silent image-pipeline bugs: gray morphology min/max was inverted in the accelerated (vImage/Metal) paths since December, distorting Mixed-mode picture detection and page-split line finding; grayscale downscaling used a resampler that strayed up to 66 gray levels from the intended area average; and morphological reconstruction (seed fill) could stop propagating early, subtly affecting picture detection and background estimation.
- Added acceleration parity tests so GPU/SIMD paths can no longer silently diverge from the reference implementation; test suites are now fully green.
- Finalize no longer silently misclassifies aged/toned paper as Color when margin sampling fails.

### Version 2.0a32

- PDF export now opens in the source document's folder with a safe `<source>_processed.pdf` fallback name.
- Saving a new project now carries already-generated pages and thumbnails from temporary storage into the project's `output` folder.

### Version 2.0a31

- Export now includes book metadata fields, recommended PDF filenames, ISBN lookup, and optional send-to-Zotero support for the locally running Zotero app.
- After exporting a PDF, the success dialog now has a **Reveal in Finder** button so you can jump straight to the file you just saved.
- Margins now supports full-bleed pages with the `F` shortcut; full-bleed pages ignore content boxes and match-size aggregation.
- The Margins stage now has one **Apply To** flow for page layout instead of separate competing margin/alignment buttons.
- The macOS startup view no longer falls back to the oversized legacy gray project panel.
- Output uses the native controls again to avoid QtWebEngine/macOS accessibility crashes.

### Version 2.0a30

- Output photo adjustments now show a single, unified panel for Color and Grayscale pages. No more duplicated panels, and no empty gap between the sliders and Dewarping.
- Sliders feel smoother during drag: the app no longer stutters while you fine-tune adjustments.
- Exposure, Contrast, Highlights/Shadows, and Whites/Blacks now apply in the same order Lightroom uses, so results match what you'd expect from other photo tools.
- Stage 2 (Split Pages): opening older projects no longer re-runs detection on every page.
- Stage 2 (Split Pages): art books with broad, dark binding gutters are detected more reliably.
- Stage 4 (Page Box): applying a page box across pages of different sizes or rotations now stays centered instead of drifting off the page.
- Dark-theme slider handles now render cleanly (previously slightly clipped).
- Stability and packaging improvements: the app bundle is more self-contained and safer to close during the Output stage.

### Version 2.0a29

- Output photo-adjustment sliders are now responsive: thumbnails redraw once after a 300 ms pause instead of on every drag tick.
- Clicking anywhere on a slider track now jumps the handle to that position (Lightroom-style), in addition to the existing drag behaviour.
- Numeric value boxes next to each slider are now compact and no longer expand to fill the column.
- Removes a stale snap-to-zero behaviour that interrupted slider drags mid-stroke.

### Version 2.0a28

- Adds photo adjustment controls in Output, including exposure, contrast, highlights, shadows, whites, blacks, temperature, and tint.
- Fixes photo adjustments so they apply properly to multiple selected pages and refresh the thumbnails afterward.
- Fixes Output so B&W and grayscale pages show the right controls instead of the full color adjustment panel.
- Sets the minimum supported macOS version to 15.
