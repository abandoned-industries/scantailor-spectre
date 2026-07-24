// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "AutoProcessDialog.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>

namespace {
const char* const kColorHandlingKey = "auto_process/color_handling";
const char* const kRedetectColorKey = "auto_process/redetect_color";
const char* const kIncludeOcrKey = "auto_process/include_ocr";
}

AutoProcessDialog::AutoProcessDialog(QWidget* parent) : QDialog(parent) {
  setupUi(this);

  QSettings settings;
  switch (static_cast<ColorHandling>(
      settings.value(kColorHandlingKey, static_cast<int>(ColorHandling::BestGuess)).toInt())) {
    case ColorHandling::ForceBlackAndWhite:
      forceBlackAndWhiteButton->setChecked(true);
      break;
    case ColorHandling::BlackAndWhiteAndGrayscale:
      blackAndWhiteAndGrayscaleButton->setChecked(true);
      break;
    case ColorHandling::BestGuess:
    default:
      bestGuessButton->setChecked(true);
      break;
  }
  redetectColorCheckBox->setChecked(settings.value(kRedetectColorKey, false).toBool());
  ocrCheckBox->setChecked(settings.value(kIncludeOcrKey, false).toBool());
  connect(forceBlackAndWhiteButton, &QRadioButton::toggled,
          this, &AutoProcessDialog::updateRedetectColorEnabled);
  updateRedetectColorEnabled();

  QPushButton* runButton = buttonBox->button(QDialogButtonBox::Ok);
  runButton->setText(tr("Run"));
  runButton->setDefault(true);
  runButton->setAutoDefault(true);
}

AutoProcessDialog::ColorHandling AutoProcessDialog::colorHandling() const {
  if (forceBlackAndWhiteButton->isChecked()) {
    return ColorHandling::ForceBlackAndWhite;
  }
  if (blackAndWhiteAndGrayscaleButton->isChecked()) {
    return ColorHandling::BlackAndWhiteAndGrayscale;
  }
  return ColorHandling::BestGuess;
}

bool AutoProcessDialog::redetectColor() const {
  return redetectColorCheckBox->isEnabled() && redetectColorCheckBox->isChecked();
}

bool AutoProcessDialog::includeOcr() const {
  return ocrCheckBox->isChecked();
}

void AutoProcessDialog::accept() {
  QSettings settings;
  settings.setValue(kColorHandlingKey, static_cast<int>(colorHandling()));
  settings.setValue(kRedetectColorKey, redetectColorCheckBox->isChecked());
  settings.setValue(kIncludeOcrKey, includeOcr());
  QDialog::accept();
}

void AutoProcessDialog::updateRedetectColorEnabled() {
  redetectColorCheckBox->setEnabled(!forceBlackAndWhiteButton->isChecked());
}
