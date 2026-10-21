// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "StageColumnLayout.h"

#include <QDockWidget>
#include <QLayout>
#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>

int StageColumnLayout::requiredDockWidth(QDockWidget* dock,
                                         QScrollArea* scrollArea,
                                         QWidget* contents,
                                         QWidget* panel,
                                         const int baseMinimumWidth) {
  if (!dock || !scrollArea || !contents || !panel) {
    return baseMinimumWidth;
  }

  panel->ensurePolished();
  contents->ensurePolished();
  if (panel->layout()) {
    panel->layout()->activate();
  }
  if (contents->layout()) {
    contents->layout()->activate();
  }

  const int contentWidth = contents->minimumSizeHint().width();
  const int scrollBarWidth = scrollArea->verticalScrollBar()->sizeHint().width();
  const int frameWidth = scrollArea->frameWidth() * 2;
  const int dockMargins = qMax(0, dock->width() - scrollArea->width());
  return qMax(baseMinimumWidth,
              contentWidth + scrollBarWidth + frameWidth + dockMargins);
}

int StageColumnLayout::apply(QMainWindow* window,
                             QDockWidget* dock,
                             QScrollArea* scrollArea,
                             QWidget* contents,
                             QWidget* panel,
                             const int baseMinimumWidth) {
  const int width = requiredDockWidth(
      dock, scrollArea, contents, panel, baseMinimumWidth);
  dock->setMinimumWidth(width);
  if (window && dock->width() < width) {
    window->resizeDocks({dock}, {width}, Qt::Horizontal);
  }
  return width;
}
