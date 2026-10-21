// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_STAGECOLUMNLAYOUT_H_
#define SCANTAILOR_APP_STAGECOLUMNLAYOUT_H_

class QDockWidget;
class QMainWindow;
class QScrollArea;
class QWidget;

class StageColumnLayout {
 public:
  static int requiredDockWidth(QDockWidget* dock,
                               QScrollArea* scrollArea,
                               QWidget* contents,
                               QWidget* panel,
                               int baseMinimumWidth);

  static int apply(QMainWindow* window,
                   QDockWidget* dock,
                   QScrollArea* scrollArea,
                   QWidget* contents,
                   QWidget* panel,
                   int baseMinimumWidth);
};

#endif  // SCANTAILOR_APP_STAGECOLUMNLAYOUT_H_
