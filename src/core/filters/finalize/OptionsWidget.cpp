// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <QDebug>

#include "PageInfo.h"
#include "filters/output/ColorParams.h"
#include "filters/output/Settings.h"

namespace finalize {

OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)), m_pageSelectionAccessor(pageSelectionAccessor) {
  setupUi(this);

  // Set up color mode combo box
  colorModeCombo->addItem(tr("Black and White"), static_cast<int>(ColorMode::BlackAndWhite));
  colorModeCombo->addItem(tr("Grayscale"), static_cast<int>(ColorMode::Grayscale));
  colorModeCombo->addItem(tr("Color"), static_cast<int>(ColorMode::Color));

  connect(colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &OptionsWidget::colorModeChanged);
  connect(thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &OptionsWidget::thresholdChanged);
  connect(clearCacheBtn, &QPushButton::clicked, this, &OptionsWidget::clearCacheClicked);
  connect(clearAllCacheBtn, &QPushButton::clicked, this, &OptionsWidget::clearAllCacheClicked);

  // Initialize threshold from settings
  thresholdSpinBox->setValue(m_settings->midtoneThreshold());
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::setOutputSettings(std::shared_ptr<output::Settings> outputSettings) {
  m_outputSettings = std::move(outputSettings);
}

void OptionsWidget::preUpdateUI(const PageInfo& pageInfo) {
  m_pageId = pageInfo.id();
  updateDisplay();
}

void OptionsWidget::postUpdateUI(const PageId& pageId) {
  m_pageId = pageId;
  updateDisplay();
}

void OptionsWidget::updateDisplay() {
  // Block signals while updating UI
  const bool blocked = colorModeCombo->blockSignals(true);

  const ColorMode mode = m_settings->getColorMode(m_pageId);
  const int index = colorModeCombo->findData(static_cast<int>(mode));
  if (index >= 0) {
    colorModeCombo->setCurrentIndex(index);
  }

  // Show detection status
  const std::unique_ptr<Params> params = m_settings->getParams(m_pageId);
  if (params && params->isColorModeDetected()) {
    statusLabel->setText(tr("Auto-detected"));
  } else {
    statusLabel->setText(tr("Not yet processed"));
  }

  colorModeCombo->blockSignals(blocked);
}

void OptionsWidget::colorModeChanged(int index) {
  if (m_pageId.isNull()) {
    return;
  }

  const ColorMode mode = static_cast<ColorMode>(colorModeCombo->itemData(index).toInt());
  m_settings->setColorMode(m_pageId, mode);

  // Also update output::Settings so the output filter uses this mode
  // Mark as user-set so it won't be overwritten by auto-detection
  if (m_outputSettings) {
    output::ColorParams colorParams;
    output::ColorMode outputMode;
    switch (mode) {
      case ColorMode::BlackAndWhite:
        outputMode = output::BLACK_AND_WHITE;
        break;
      case ColorMode::Grayscale:
        outputMode = output::GRAYSCALE;
        break;
      case ColorMode::Color:
      default:
        outputMode = output::COLOR;
        break;
    }
    colorParams.setColorMode(outputMode);
    colorParams.setColorModeUserSet(true);  // CRITICAL: Mark as user-set to prevent auto-detection override
    m_outputSettings->setColorParams(m_pageId, colorParams);
    qDebug() << "Finalize: User set color mode to" << static_cast<int>(outputMode) << "for page"
             << m_pageId.imageId().filePath();
  }

  emit invalidateThumbnail(m_pageId);
  emit reloadRequested();
}

void OptionsWidget::thresholdChanged(int value) {
  m_settings->setMidtoneThreshold(value);
  qDebug() << "Midtone threshold changed to" << value << "%";
}

void OptionsWidget::clearCacheClicked() {
  if (m_pageId.isNull()) {
    return;
  }

  qDebug() << "Clearing detection cache for page:" << m_pageId.imageId().filePath();
  m_settings->clearDetectionCacheForPage(m_pageId);
  updateDisplay();

  emit invalidateThumbnail(m_pageId);
  emit reloadRequested();
}

void OptionsWidget::clearAllCacheClicked() {
  qDebug() << "Clearing detection cache for ALL pages";
  m_settings->clearDetectionCache();

  emit invalidateAllThumbnails();
  emit reloadRequested();
}

}  // namespace finalize
