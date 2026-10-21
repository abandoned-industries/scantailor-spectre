// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_EXPORTPANELHEIGHTLAYOUT_H_
#define SCANTAILOR_APP_EXPORTPANELHEIGHTLAYOUT_H_

class QScrollArea;
class QWidget;
class StageActionPanel;

// Computes and applies the fixed height Export's inner settings widget needs
// so that, together with the pinned StageActionPanel chrome around it
// (heading, Adjust toggle, and the pinned Zotero/Export-PDF footer), the
// whole stack fits the outer scroll area's viewport instead of growing it.
//
// See MainWindow::updateFilterOptionsHeight() for the call site: this runs on
// every resize of the outer scroll area's viewport while the Export stage is
// open and the Adjust fold is expanded. Extracted out of MainWindow so it can
// be exercised by an offscreen regression test without linking the whole GUI
// executable, mirroring StageColumnLayout.
class ExportPanelHeightLayout {
 public:
  // Returns the height applied to `filterOptions`, or -1 if nothing was
  // changed (missing panel/widget/scrollArea, the Adjust fold is closed, or
  // the viewport has no usable height yet).
  //
  // `scrollArea` is the outer scroll area whose viewport hosts `panel`. Its
  // vertical scrollbar sizeHint/visibility are used to measure `panel`'s
  // chrome height at a width that stays the same regardless of whether that
  // scrollbar happens to be shown right now -- see the .cpp for why that
  // matters.
  static int apply(StageActionPanel* panel, QWidget* filterOptions, int minimumOptionsHeight,
                   QScrollArea* scrollArea);
};

#endif  // SCANTAILOR_APP_EXPORTPANELHEIGHTLAYOUT_H_
