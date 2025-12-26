// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <QSignalBlocker>

#include "OcrResult.h"

namespace ocr {

OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)), m_pageSelectionAccessor(pageSelectionAccessor) {
  setupUi(this);

  populateLanguageCombo();

  // Connect UI signals
  connect(enableOcrCB, &QCheckBox::toggled, this, &OptionsWidget::enableOcrChanged);
  connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsWidget::languageChanged);
  connect(accurateCB, &QCheckBox::toggled, this, &OptionsWidget::accurateChanged);
  connect(clearResultBtn, &QPushButton::clicked, this, &OptionsWidget::clearResultClicked);
  connect(clearAllBtn, &QPushButton::clicked, this, &OptionsWidget::clearAllClicked);

  updateDisplay();
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::preUpdateUI(const PageInfo& pageInfo) {
  m_pageId = pageInfo.id();
  updateDisplay();
}

void OptionsWidget::postUpdateUI(const PageId& pageId) {
  m_pageId = pageId;
  updateDisplay();
}

void OptionsWidget::populateLanguageCombo() {
  languageCombo->clear();

  // Add languages with their display names
  const QStringList codes = supportedLanguageCodes();
  for (const QString& code : codes) {
    languageCombo->addItem(languageDisplayName(code), code);
  }
}

void OptionsWidget::updateDisplay() {
  const QSignalBlocker blocker1(enableOcrCB);
  const QSignalBlocker blocker2(languageCombo);
  const QSignalBlocker blocker3(accurateCB);

  const bool ocrEnabled = m_settings->ocrEnabled();
  enableOcrCB->setChecked(ocrEnabled);
  accurateCB->setChecked(m_settings->useAccurateRecognition());

  // Enable/disable controls based on OCR enabled state
  languageLabel->setEnabled(ocrEnabled);
  languageCombo->setEnabled(ocrEnabled);
  accurateCB->setEnabled(ocrEnabled);
  resultsGroup->setEnabled(ocrEnabled);

  // Find and select the language
  const QString currentLang = m_settings->language();
  for (int i = 0; i < languageCombo->count(); ++i) {
    if (languageCombo->itemData(i).toString() == currentLang) {
      languageCombo->setCurrentIndex(i);
      break;
    }
  }

  // Update status for current page
  const std::unique_ptr<OcrResult> result = m_settings->getOcrResult(m_pageId);
  if (result && !result->isEmpty()) {
    statusLabel->setText(tr("Status: Processed"));
    wordCountLabel->setText(tr("Text blocks: %1").arg(result->words().size()));
    clearResultBtn->setEnabled(true);
  } else {
    statusLabel->setText(tr("Status: Not processed"));
    wordCountLabel->setText(tr("Text blocks: 0"));
    clearResultBtn->setEnabled(false);
  }
}

void OptionsWidget::enableOcrChanged(bool checked) {
  m_settings->setOcrEnabled(checked);
  updateDisplay();
}

void OptionsWidget::languageChanged(int index) {
  if (index >= 0) {
    const QString langCode = languageCombo->itemData(index).toString();
    m_settings->setLanguage(langCode);
  }
}

void OptionsWidget::accurateChanged(bool checked) {
  m_settings->setUseAccurateRecognition(checked);
}

void OptionsWidget::clearResultClicked() {
  m_settings->clearOcrResult(m_pageId);
  updateDisplay();
}

void OptionsWidget::clearAllClicked() {
  m_settings->clearAllResults();
  updateDisplay();
}

}  // namespace ocr
