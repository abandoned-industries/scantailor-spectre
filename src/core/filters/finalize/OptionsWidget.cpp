// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <QDebug>
#include <QFileDialog>

#include "PageInfo.h"
#include "filters/output/ColorParams.h"
#include "filters/output/Settings.h"

namespace finalize {

OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)), m_pageSelectionAccessor(pageSelectionAccessor) {
  qDebug() << "Finalize OptionsWidget: constructor start";
  setupUi(this);
  qDebug() << "Finalize OptionsWidget: setupUi done";

  // Set up color mode combo box
  colorModeCombo->addItem(tr("Black and White"), static_cast<int>(ColorMode::BlackAndWhite));
  colorModeCombo->addItem(tr("Grayscale"), static_cast<int>(ColorMode::Grayscale));
  colorModeCombo->addItem(tr("Color"), static_cast<int>(ColorMode::Color));

  // Set up format combo box
  formatCombo->addItem(tr("TIFF"), static_cast<int>(OutputFormat::TIFF));
  formatCombo->addItem(tr("PNG"), static_cast<int>(OutputFormat::PNG));
  formatCombo->addItem(tr("JPEG"), static_cast<int>(OutputFormat::JPEG));

  // Set up compression combo box (for TIFF)
  compressionCombo->addItem(tr("LZW"), static_cast<int>(TiffCompression::LZW));
  compressionCombo->addItem(tr("Deflate"), static_cast<int>(TiffCompression::Deflate));

  connect(colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &OptionsWidget::colorModeChanged);
  connect(thresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &OptionsWidget::thresholdChanged);
  connect(clearCacheBtn, &QPushButton::clicked, this, &OptionsWidget::clearCacheClicked);
  connect(clearAllCacheBtn, &QPushButton::clicked, this, &OptionsWidget::clearAllCacheClicked);

  // Output settings connections
  connect(preserveOutputCB, &QCheckBox::toggled, this, &OptionsWidget::preserveOutputChanged);
  connect(chooseOutputFolderBtn, &QPushButton::clicked, this, &OptionsWidget::chooseOutputFolderClicked);
  connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsWidget::formatChanged);
  connect(compressionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &OptionsWidget::compressionChanged);
  connect(jpegQualitySlider, &QSlider::valueChanged, this, &OptionsWidget::jpegQualityChanged);

  // Initialize threshold from settings
  thresholdSpinBox->setValue(m_settings->midtoneThreshold());
  qDebug() << "Finalize OptionsWidget: about to call updateOutputUI";

  // Initialize output settings UI
  updateOutputUI();
  qDebug() << "Finalize OptionsWidget: constructor complete";
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

void OptionsWidget::preserveOutputChanged(bool checked) {
  m_settings->setPreserveOutput(checked);
  chooseOutputFolderBtn->setEnabled(checked);

  if (!checked) {
    outputPathLabel->setText(tr("Using temporary folder"));
    // Switch back to temp directory (empty string signals this)
    emit outputDirectoryChanged(QString());
  } else if (m_settings->outputPath().isEmpty()) {
    outputPathLabel->setText(tr("No folder selected"));
  } else {
    // Preserve enabled and we have a path - switch to it
    emit outputDirectoryChanged(m_settings->outputPath());
  }
  qDebug() << "Preserve output changed to" << checked;
}

void OptionsWidget::chooseOutputFolderClicked() {
  const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Output Folder"),
                                                        m_settings->outputPath().isEmpty()
                                                            ? QDir::homePath()
                                                            : m_settings->outputPath());
  if (!dir.isEmpty()) {
    m_settings->setOutputPath(dir);
    outputPathLabel->setText(dir);
    qDebug() << "Output folder set to" << dir;
    // Notify that output directory changed (if preserve is enabled, this is the new effective dir)
    if (m_settings->preserveOutput()) {
      emit outputDirectoryChanged(dir);
    }
  }
}

void OptionsWidget::formatChanged(int index) {
  const OutputFormat format = static_cast<OutputFormat>(formatCombo->itemData(index).toInt());
  m_settings->setOutputFormat(format);
  updateFormatOptions();
  emit outputFormatSettingChanged(static_cast<int>(format));
  qDebug() << "Output format changed to" << index;
}

void OptionsWidget::compressionChanged(int index) {
  const TiffCompression compression = static_cast<TiffCompression>(compressionCombo->itemData(index).toInt());
  m_settings->setTiffCompression(compression);
  emit tiffCompressionSettingChanged(static_cast<int>(compression));
  qDebug() << "TIFF compression changed to" << index;
}

void OptionsWidget::jpegQualityChanged(int value) {
  m_settings->setJpegQuality(value);
  jpegQualityValueLabel->setText(QString::number(value));
  emit jpegQualitySettingChanged(value);
  qDebug() << "JPEG quality changed to" << value;
}

void OptionsWidget::updateOutputUI() {
  // Block signals while updating UI
  const bool preserveBlocked = preserveOutputCB->blockSignals(true);
  const bool formatBlocked = formatCombo->blockSignals(true);
  const bool compressionBlocked = compressionCombo->blockSignals(true);
  const bool qualityBlocked = jpegQualitySlider->blockSignals(true);

  // Update checkbox
  preserveOutputCB->setChecked(m_settings->preserveOutput());
  chooseOutputFolderBtn->setEnabled(m_settings->preserveOutput());

  // Update path label
  if (!m_settings->preserveOutput()) {
    outputPathLabel->setText(tr("Using temporary folder"));
  } else if (m_settings->outputPath().isEmpty()) {
    outputPathLabel->setText(tr("No folder selected"));
  } else {
    outputPathLabel->setText(m_settings->outputPath());
  }

  // Update format combo
  const int formatIndex = formatCombo->findData(static_cast<int>(m_settings->outputFormat()));
  if (formatIndex >= 0) {
    formatCombo->setCurrentIndex(formatIndex);
  }

  // Update compression combo
  const int compressionIndex = compressionCombo->findData(static_cast<int>(m_settings->tiffCompression()));
  if (compressionIndex >= 0) {
    compressionCombo->setCurrentIndex(compressionIndex);
  }

  // Update JPEG quality
  jpegQualitySlider->setValue(m_settings->jpegQuality());
  jpegQualityValueLabel->setText(QString::number(m_settings->jpegQuality()));

  // Show/hide format-specific options
  updateFormatOptions();

  // Restore signal blocking
  preserveOutputCB->blockSignals(preserveBlocked);
  formatCombo->blockSignals(formatBlocked);
  compressionCombo->blockSignals(compressionBlocked);
  jpegQualitySlider->blockSignals(qualityBlocked);
}

void OptionsWidget::updateFormatOptions() {
  const OutputFormat format = m_settings->outputFormat();

  // Show compression options only for TIFF
  const bool showCompression = (format == OutputFormat::TIFF);
  compressionLabel->setVisible(showCompression);
  compressionCombo->setVisible(showCompression);

  // Show quality slider only for JPEG
  const bool showQuality = (format == OutputFormat::JPEG);
  jpegQualityLabel->setVisible(showQuality);
  jpegQualitySlider->setVisible(showQuality);
  jpegQualityValueLabel->setVisible(showQuality);
}

}  // namespace finalize
