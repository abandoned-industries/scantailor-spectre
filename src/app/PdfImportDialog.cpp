// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfImportDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

PdfImportDialog::PdfImportDialog(QWidget* parent, const QString& pdfPath, int pageCount, int detectedDpi)
    : QDialog(parent) {
  setWindowTitle(tr("Import PDF"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(12);
  layout->setContentsMargins(20, 20, 20, 20);

  // File info
  const QString fileName = QFileInfo(pdfPath).fileName();
  auto* fileLabel = new QLabel(tr("<b>%1</b><br>%2 pages").arg(fileName).arg(pageCount), this);
  layout->addWidget(fileLabel);

  // DPI selection
  auto* dpiLayout = new QHBoxLayout();
  auto* dpiLabel = new QLabel(tr("Import resolution:"), this);
  dpiLayout->addWidget(dpiLabel);

  m_dpiCombo = new QComboBox(this);
  m_dpiCombo->addItem(tr("300 DPI (standard)"), 300);
  m_dpiCombo->addItem(tr("400 DPI (high quality)"), 400);
  m_dpiCombo->addItem(tr("600 DPI (archival)"), 600);

  // Select the detected DPI by default
  int defaultIndex = 0;
  if (detectedDpi >= 600) {
    defaultIndex = 2;
  } else if (detectedDpi >= 400) {
    defaultIndex = 1;
  }
  m_dpiCombo->setCurrentIndex(defaultIndex);

  dpiLayout->addWidget(m_dpiCombo);
  dpiLayout->addStretch();
  layout->addLayout(dpiLayout);

  // Show detected DPI as hint
  QString hint;
  if (detectedDpi > 300) {
    hint = tr("Detected resolution: %1 DPI").arg(detectedDpi);
  } else {
    hint = tr("Default resolution (no high-res images detected)");
  }
  auto* hintLabel = new QLabel(hint, this);
  hintLabel->setStyleSheet("color: gray; font-size: 11px;");
  layout->addWidget(hintLabel);

  layout->addSpacing(8);

  // Buttons
  auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttonBox);

  setFixedWidth(350);
}

int PdfImportDialog::selectedDpi() const {
  return m_dpiCombo->currentData().toInt();
}
