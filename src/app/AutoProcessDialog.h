// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_AUTOPROCESSDIALOG_H_
#define SCANTAILOR_APP_AUTOPROCESSDIALOG_H_

#include <QDialog>

#include "ui_AutoProcessDialog.h"

class AutoProcessDialog : public QDialog, private Ui::AutoProcessDialog {
  Q_OBJECT

 public:
  enum class ColorHandling {
    ForceBlackAndWhite = 0,
    BlackAndWhiteAndGrayscale = 1,
    BestGuess = 2
  };

  explicit AutoProcessDialog(QWidget* parent = nullptr);

  ColorHandling colorHandling() const;
  bool redetectColor() const;
  bool includeOcr() const;

 public slots:
  void accept() override;

 private:
  void updateRedetectColorEnabled();
};

#endif  // SCANTAILOR_APP_AUTOPROCESSDIALOG_H_
