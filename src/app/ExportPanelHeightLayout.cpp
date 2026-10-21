// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ExportPanelHeightLayout.h"

#include <QLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>
#include <QtGlobal>

#include "StageActionPanel.h"

int ExportPanelHeightLayout::apply(StageActionPanel* panel, QWidget* filterOptions,
                                   const int minimumOptionsHeight, QScrollArea* scrollArea) {
  if (!panel || !filterOptions || !scrollArea || !panel->adjustOpen()) {
    return -1;
  }
  const int viewportHeight = scrollArea->viewport()->height();
  if (viewportHeight <= 0) {
    return -1;
  }

  filterOptions->setMinimumHeight(0);
  filterOptions->setMaximumHeight(QWIDGETSIZE_MAX);
  panel->layout()->activate();

  // Measure the panel's chrome (heading + pinned Zotero/Export-PDF footer)
  // at a width that does NOT track panel's current actual width.
  //
  // panel's actual width mirrors the outer scroll area's viewport width,
  // which itself changes by exactly the vertical scrollbar's width
  // depending on whether Qt is CURRENTLY drawing that scrollbar
  // (Qt::ScrollBarAsNeeded) -- and that decision is in turn driven by the
  // total height this very function produces. Feeding panel's live,
  // fluctuating width into a word-wrap-sensitive sizeHint() therefore
  // closes a feedback loop: height computed here -> total content height
  // changes -> scrollbar toggles -> viewport width changes -> panel's
  // word-wrapped labels rewrap -> chrome height changes -> height computed
  // here changes again, forever. That loop is what made the Export options
  // panel visibly "vibrate" (grow and shrink continuously) at window sizes
  // where the outer scrollbar sits right on the edge of being needed.
  //
  // Break the loop by always measuring chrome height at the width panel
  // would have if the vertical scrollbar were shown -- a value that stays
  // constant across both scrollbar states, so this function's output no
  // longer depends on its own previous output. QLayout::totalHeightForWidth()
  // is not used here: it does not reliably propagate an explicit width
  // through the pinned footer's nested QGroupBox/QVBoxLayout chain down to
  // the word-wrapped labels actually driving the size change (verified
  // empirically -- it kept returning heights tied to panel's real, live
  // width instead of the hypothetical one passed in). Forcing the real
  // geometry to the stable width and activating the layout does reach the
  // leaf labels, because that is the exact mechanism (sizeHint() reflecting
  // actual current width) that produced the oscillation in the first place.
  const int scrollBarWidth = scrollArea->verticalScrollBar()->sizeHint().width();
  const bool scrollBarVisible = scrollArea->verticalScrollBar()->isVisible();
  const int stableWidth
      = qMax(1, scrollArea->viewport()->width() - (scrollBarVisible ? 0 : scrollBarWidth));

  const int actualPanelWidth = panel->width();
  int panelHeightAtStableWidth;
  if (actualPanelWidth == stableWidth) {
    panelHeightAtStableWidth = panel->sizeHint().height();
  } else {
    const int actualPanelHeight = panel->height();
    panel->resize(stableWidth, actualPanelHeight);
    panel->layout()->activate();
    panelHeightAtStableWidth = panel->sizeHint().height();
    panel->resize(actualPanelWidth, actualPanelHeight);
    panel->layout()->activate();
  }

  const int panelChromeHeight = qMax(0, panelHeightAtStableWidth - filterOptions->sizeHint().height());
  const int availableOptionsHeight = qMax(0, viewportHeight - panelChromeHeight);
  const int height = qMax(availableOptionsHeight, minimumOptionsHeight);
  filterOptions->setFixedHeight(height);
  return height;
}
