// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <QSignalBlocker>

#include "Dpi.h"
#include "filters/output/ChangeDpiDialog.h"
#include "filters/output/Settings.h"
#include "PdfExporter.h"

namespace export_ {

OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)), m_pageSelectionAccessor(pageSelectionAccessor) {
  setupUi(this);

  populateQualityCombo();

  // Connect UI signals
  connect(noDpiLimitCB, &QCheckBox::toggled, this, &OptionsWidget::noDpiLimitChanged);
  connect(maxDpiSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &OptionsWidget::maxDpiChanged);
  connect(compressGrayscaleCB, &QCheckBox::toggled, this, &OptionsWidget::compressGrayscaleChanged);
  connect(qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsWidget::qualityChanged);
  connect(exportToPdfBtn, &QPushButton::clicked, this, &OptionsWidget::exportToPdfClicked);
  connect(changeOutputDpiBtn, &QPushButton::clicked, this, &OptionsWidget::changeOutputDpiClicked);

  updateDisplay();
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::setOutputSettings(std::shared_ptr<output::Settings> outputSettings) {
  m_outputSettings = std::move(outputSettings);
}

void OptionsWidget::preUpdateUI(const PageInfo& pageInfo) {
  m_pageId = pageInfo.id();
  updateDisplay();
  updateOutputDpiDisplay();
}

void OptionsWidget::postUpdateUI(const PageId& pageId) {
  // Nothing special needed for export filter
}

void OptionsWidget::populateQualityCombo() {
  qualityCombo->clear();
  qualityCombo->addItem(tr("High (95%)"), static_cast<int>(PdfExporter::Quality::High));
  qualityCombo->addItem(tr("Medium (85%)"), static_cast<int>(PdfExporter::Quality::Medium));
  qualityCombo->addItem(tr("Low (70%)"), static_cast<int>(PdfExporter::Quality::Low));
}

void OptionsWidget::updateDisplay() {
  const QSignalBlocker blocker1(noDpiLimitCB);
  const QSignalBlocker blocker2(maxDpiSpinBox);
  const QSignalBlocker blocker3(compressGrayscaleCB);
  const QSignalBlocker blocker4(qualityCombo);

  noDpiLimitCB->setChecked(m_settings->noDpiLimit());
  maxDpiSpinBox->setValue(m_settings->maxDpi());
  maxDpiSpinBox->setEnabled(!m_settings->noDpiLimit());
  maxDpiLabel->setEnabled(!m_settings->noDpiLimit());
  compressGrayscaleCB->setChecked(m_settings->compressGrayscale());

  // Find and select the quality index
  int qualityValue = static_cast<int>(m_settings->quality());
  for (int i = 0; i < qualityCombo->count(); ++i) {
    if (qualityCombo->itemData(i).toInt() == qualityValue) {
      qualityCombo->setCurrentIndex(i);
      break;
    }
  }
}

void OptionsWidget::noDpiLimitChanged(bool checked) {
  m_settings->setNoDpiLimit(checked);
  maxDpiSpinBox->setEnabled(!checked);
  maxDpiLabel->setEnabled(!checked);
}

void OptionsWidget::maxDpiChanged(int value) {
  m_settings->setMaxDpi(value);
}

void OptionsWidget::compressGrayscaleChanged(bool checked) {
  m_settings->setCompressGrayscale(checked);
}

void OptionsWidget::qualityChanged(int index) {
  if (index >= 0) {
    int qualityValue = qualityCombo->itemData(index).toInt();
    m_settings->setQuality(static_cast<PdfExporter::Quality>(qualityValue));
  }
}

void OptionsWidget::exportToPdfClicked() {
  emit exportToPdfRequested();
}

void OptionsWidget::changeOutputDpiClicked() {
  if (!m_outputSettings) return;

  const output::Params params = m_outputSettings->getParams(m_pageId);
  const Dpi outputDpi = params.outputDpi();

  auto* dialog = new output::ChangeDpiDialog(this, outputDpi, m_pageId, m_pageSelectionAccessor);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&, const Dpi&)),
          this, SLOT(outputDpiChanged(const std::set<PageId>&, const Dpi&)));
  dialog->show();
}

void OptionsWidget::outputDpiChanged(const std::set<PageId>& pages, const Dpi& dpi) {
  if (!m_outputSettings) return;

  for (const PageId& pageId : pages) {
    output::Params params = m_outputSettings->getParams(pageId);
    params.setOutputDpi(dpi);
    m_outputSettings->setParams(pageId, params);
  }
  updateOutputDpiDisplay();
}

void OptionsWidget::updateOutputDpiDisplay() {
  if (!m_outputSettings) {
    outputDpiLabel->setText("300 dpi");
    return;
  }
  const output::Params params = m_outputSettings->getParams(m_pageId);
  const Dpi dpi = params.outputDpi();
  if (dpi.horizontal() != dpi.vertical()) {
    outputDpiLabel->setText(QString("%1 x %2 dpi").arg(dpi.horizontal()).arg(dpi.vertical()));
  } else {
    outputDpiLabel->setText(QString("%1 dpi").arg(dpi.horizontal()));
  }
}

}  // namespace export_
