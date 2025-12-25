// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "OptionsWidget.h"

#include <QColorDialog>
#include <QDebug>
#include <QToolTip>
#include <utility>

#include "../../Utils.h"
#include "filters/finalize/Settings.h"
#include "ApplyColorsDialog.h"
#include "ChangeDewarpingDialog.h"
#include "ChangeDpiDialog.h"
#include "FillZoneComparator.h"
#include "OtsuBinarizationOptionsWidget.h"
#include "PictureZoneComparator.h"
#include "SauvolaBinarizationOptionsWidget.h"
#include "WolfBinarizationOptionsWidget.h"

using namespace core;

namespace output {
OptionsWidget::OptionsWidget(std::shared_ptr<Settings> settings, const PageSelectionAccessor& pageSelectionAccessor)
    : m_settings(std::move(settings)),
      m_pageSelectionAccessor(pageSelectionAccessor),
      m_despeckleLevel(1.0),
      m_lastTab(TAB_OUTPUT),
      m_connectionManager(std::bind(&OptionsWidget::setupUiConnections, this)) {
  setupUi(this);

  m_delayedReloadRequest.setSingleShot(true);

  depthPerceptionSlider->setMinimum(qRound(DepthPerception::minValue() * 10));
  depthPerceptionSlider->setMaximum(qRound(DepthPerception::maxValue() * 10));

  despeckleSlider->setMinimum(qRound(1.0 * 10));
  despeckleSlider->setMaximum(qRound(3.0 * 10));

  colorModeSelector->addItem(tr("Black and White"), BLACK_AND_WHITE);
  colorModeSelector->addItem(tr("Color"), COLOR);
  colorModeSelector->addItem(tr("Grayscale"), GRAYSCALE);
  colorModeSelector->addItem(tr("Mixed"), MIXED);

  thresholdMethodBox->addItem(tr("Otsu"), T_OTSU);
  thresholdMethodBox->addItem(tr("Sauvola"), T_SAUVOLA);
  thresholdMethodBox->addItem(tr("Wolf"), T_WOLF);
  thresholdMethodBox->addItem(tr("Bradley"), T_BRADLEY);
  thresholdMethodBox->addItem(tr("Grad"), T_GRAD);
  thresholdMethodBox->addItem(tr("EdgePlus"), T_EDGEPLUS);
  thresholdMethodBox->addItem(tr("BlurDiv"), T_BLURDIV);
  thresholdMethodBox->addItem(tr("EdgeDiv"), T_EDGEDIV);
  thresholdMethodBox->addItem(tr("Niblack"), T_NIBLACK);
  thresholdMethodBox->addItem(tr("N.I.C.K"), T_NICK);
  thresholdMethodBox->addItem(tr("Singh"), T_SINGH);
  thresholdMethodBox->addItem(tr("WAN"), T_WAN);
  thresholdMethodBox->addItem(tr("MultiScale"), T_MSCALE);
  thresholdMethodBox->addItem(tr("Robust"), T_ROBUST);
  thresholdMethodBox->addItem(tr("Gatos"), T_GATOS);
  thresholdMethodBox->addItem(tr("Window"), T_WINDOW);
  thresholdMethodBox->addItem(tr("Fox"), T_FOX);
  thresholdMethodBox->addItem(tr("Engraving"), T_ENGRAVING);
  thresholdMethodBox->addItem(tr("BiModal"), T_BIMODAL);
  thresholdMethodBox->addItem(tr("Mean"), T_MEAN);
  thresholdMethodBox->addItem(tr("Grain"), T_GRAIN);

  // Filling color radio buttons are set up in UI - White is default

  QPointer<BinarizationOptionsWidget> otsuBinarizationOptionsWidget = new OtsuBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> sauvolaBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> wolfBinarizationOptionsWidget = new WolfBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> bradleyBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> gradBinarizationOptionsWidget = new WolfBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> edgeplusBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> blurdivBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> edgedivBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  // New algorithms - Niblack uses Sauvola-style options (window size + k coefficient)
  QPointer<BinarizationOptionsWidget> niblackBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> nickBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> singhBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  // WAN, MultiScale, Robust, Gatos - all use Sauvola-style options (window size + k coefficient)
  QPointer<BinarizationOptionsWidget> wanBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> mscaleBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> robustBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> gatosBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  // Window, Fox, Engraving - all use Sauvola-style options (window size + k coefficient)
  QPointer<BinarizationOptionsWidget> windowBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> foxBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> engravingBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);
  // BiModal and Mean use Otsu-style options (no window size, just delta)
  QPointer<BinarizationOptionsWidget> bimodalBinarizationOptionsWidget
      = new OtsuBinarizationOptionsWidget(m_settings);
  QPointer<BinarizationOptionsWidget> meanBinarizationOptionsWidget
      = new OtsuBinarizationOptionsWidget(m_settings);
  // Grain uses Sauvola-style options (window size + k coefficient)
  QPointer<BinarizationOptionsWidget> grainBinarizationOptionsWidget
      = new SauvolaBinarizationOptionsWidget(m_settings);

  while (binarizationOptions->count() != 0) {
    binarizationOptions->removeWidget(binarizationOptions->widget(0));
  }
  addBinarizationOptionsWidget(otsuBinarizationOptionsWidget);
  addBinarizationOptionsWidget(sauvolaBinarizationOptionsWidget);
  addBinarizationOptionsWidget(wolfBinarizationOptionsWidget);
  addBinarizationOptionsWidget(bradleyBinarizationOptionsWidget);
  addBinarizationOptionsWidget(gradBinarizationOptionsWidget);
  addBinarizationOptionsWidget(edgeplusBinarizationOptionsWidget);
  addBinarizationOptionsWidget(blurdivBinarizationOptionsWidget);
  addBinarizationOptionsWidget(edgedivBinarizationOptionsWidget);
  addBinarizationOptionsWidget(niblackBinarizationOptionsWidget);
  addBinarizationOptionsWidget(nickBinarizationOptionsWidget);
  addBinarizationOptionsWidget(singhBinarizationOptionsWidget);
  addBinarizationOptionsWidget(wanBinarizationOptionsWidget);
  addBinarizationOptionsWidget(mscaleBinarizationOptionsWidget);
  addBinarizationOptionsWidget(robustBinarizationOptionsWidget);
  addBinarizationOptionsWidget(gatosBinarizationOptionsWidget);
  addBinarizationOptionsWidget(windowBinarizationOptionsWidget);
  addBinarizationOptionsWidget(foxBinarizationOptionsWidget);
  addBinarizationOptionsWidget(engravingBinarizationOptionsWidget);
  addBinarizationOptionsWidget(bimodalBinarizationOptionsWidget);
  addBinarizationOptionsWidget(meanBinarizationOptionsWidget);
  addBinarizationOptionsWidget(grainBinarizationOptionsWidget);
  updateBinarizationOptionsDisplay(binarizationOptions->currentIndex());

  pictureShapeSelector->addItem(tr("Off"), OFF_SHAPE);
  pictureShapeSelector->addItem(tr("Free"), FREE_SHAPE);
  pictureShapeSelector->addItem(tr("Rectangular"), RECTANGULAR_SHAPE);

  updateDpiDisplay();
  updateColorsDisplay();
  updateDewarpingDisplay();

  connect(binarizationOptions, SIGNAL(currentChanged(int)), this, SLOT(updateBinarizationOptionsDisplay(int)));

  setupUiConnections();
}

OptionsWidget::~OptionsWidget() = default;

void OptionsWidget::setFinalizeSettings(std::shared_ptr<finalize::Settings> finalizeSettings) {
  m_finalizeSettings = std::move(finalizeSettings);
}

void OptionsWidget::preUpdateUI(const PageId& pageId) {
  auto block = m_connectionManager.getScopedBlock();

  // Just get existing params - don't trigger Vision detection on page click
  // Detection happens during batch processing or Task execution
  const Params params = m_settings->getParams(pageId);
  m_pageId = pageId;
  m_outputDpi = params.outputDpi();
  m_colorParams = params.colorParams();
  m_splittingOptions = params.splittingOptions();
  m_pictureShapeOptions = params.pictureShapeOptions();
  m_dewarpingOptions = params.dewarpingOptions();
  m_depthPerception = params.depthPerception();
  m_despeckleLevel = params.despeckleLevel();

  updateSelectionIndicator();
  updateDpiDisplay();
  updateColorsDisplay();
  updateDewarpingDisplay();
}

void OptionsWidget::postUpdateUI() {
  auto block = m_connectionManager.getScopedBlock();

  m_dewarpingOptions = m_settings->getParams(m_pageId).dewarpingOptions();
  updateDewarpingDisplay();
}

void OptionsWidget::tabChanged(const ImageViewTab tab) {
  m_lastTab = tab;
  updateDpiDisplay();
  updateColorsDisplay();
  updateDewarpingDisplay();
  reloadIfNecessary();
}

void OptionsWidget::distortionModelChanged(const dewarping::DistortionModel& model) {
  m_settings->setDistortionModel(m_pageId, model);

  m_dewarpingOptions.setDewarpingMode(MANUAL);
  m_settings->setDewarpingOptions(m_pageId, m_dewarpingOptions);
  updateDewarpingDisplay();
}

void OptionsWidget::colorModeChanged(const int idx) {
  const int mode = colorModeSelector->itemData(idx).toInt();

  // User selected a mode from the dropdown
  m_colorParams.setColorMode((ColorMode) mode);
  m_colorParams.setColorModeUserSet(true);  // Mark as user-set to prevent auto-detection override
  m_settings->setColorParams(m_pageId, m_colorParams);
  updateColorsDisplay();

  // Also update finalize settings so finalize filter stays in sync
  if (m_finalizeSettings) {
    finalize::ColorMode finalizeMode;
    switch ((ColorMode) mode) {
      case BLACK_AND_WHITE:
        finalizeMode = finalize::ColorMode::BlackAndWhite;
        break;
      case GRAYSCALE:
        finalizeMode = finalize::ColorMode::Grayscale;
        break;
      case COLOR:
      default:
        finalizeMode = finalize::ColorMode::Color;
        break;
    }
    m_finalizeSettings->setColorMode(m_pageId, finalizeMode);
    qDebug() << "Output: User set color mode to" << mode << "for page" << m_pageId.imageId().filePath()
             << "- synced to finalize filter";
  }

  // Apply to all selected pages
  applyColorParamsToSelectedPages();

  emit invalidateThumbnail(m_pageId);
  emit reloadRequested();
}

void OptionsWidget::thresholdMethodChanged(int idx) {
  const BinarizationMethod method = (BinarizationMethod) thresholdMethodBox->itemData(idx).toInt();
  BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
  blackWhiteOptions.setBinarizationMethod(method);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  emit invalidateThumbnail(m_pageId);
  emit reloadRequested();
}

void OptionsWidget::fillingColorChanged() {
  // Determine color from radio buttons
  FillingColor color = FILL_WHITE;
  if (fillBackgroundRB->isChecked()) {
    color = FILL_BACKGROUND;
  }

  ColorCommonOptions colorCommonOptions(m_colorParams.colorCommonOptions());
  colorCommonOptions.setFillingColor(color);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  emit invalidateThumbnail(m_pageId);
  emit reloadRequested();
}

void OptionsWidget::pictureShapeChanged(const int idx) {
  const auto shapeMode = static_cast<PictureShape>(pictureShapeSelector->itemData(idx).toInt());
  m_pictureShapeOptions.setPictureShape(shapeMode);
  m_settings->setPictureShapeOptions(m_pageId, m_pictureShapeOptions);

  pictureShapeSensitivityOptions->setVisible(shapeMode == RECTANGULAR_SHAPE);
  higherSearchSensitivityCB->setVisible(shapeMode != OFF_SHAPE);

  emit reloadRequested();
}

void OptionsWidget::pictureShapeSensitivityChanged(int value) {
  m_pictureShapeOptions.setSensitivity(value);
  m_settings->setPictureShapeOptions(m_pageId, m_pictureShapeOptions);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::higherSearchSensivityToggled(const bool checked) {
  m_pictureShapeOptions.setHigherSearchSensitivity(checked);
  m_settings->setPictureShapeOptions(m_pageId, m_pictureShapeOptions);

  emit reloadRequested();
}

void OptionsWidget::fillMarginsToggled(const bool checked) {
  ColorCommonOptions colorCommonOptions(m_colorParams.colorCommonOptions());
  colorCommonOptions.setFillMargins(checked);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::fillOffcutToggled(const bool checked) {
  ColorCommonOptions colorCommonOptions(m_colorParams.colorCommonOptions());
  colorCommonOptions.setFillOffcut(checked);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::equalizeIlluminationToggled(const bool checked) {
  BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
  blackWhiteOptions.setNormalizeIllumination(checked);

  if (m_colorParams.colorMode() == MIXED) {
    if (!checked) {
      ColorCommonOptions colorCommonOptions(m_colorParams.colorCommonOptions());
      colorCommonOptions.setNormalizeIllumination(false);
      equalizeIlluminationColorCB->setChecked(false);
      m_colorParams.setColorCommonOptions(colorCommonOptions);
    }
    equalizeIlluminationColorCB->setEnabled(checked);
  }

  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);
  // Update visibility of paper detection controls
  paperDetectionWidget->setVisible(checked || equalizeIlluminationColorCB->isChecked());
  emit reloadRequested();
}

void OptionsWidget::equalizeIlluminationColorToggled(const bool checked) {
  ColorCommonOptions opt(m_colorParams.colorCommonOptions());
  opt.setNormalizeIllumination(checked);
  m_colorParams.setColorCommonOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  // Update visibility of paper detection controls
  paperDetectionWidget->setVisible(checked || equalizeIlluminationCB->isChecked());
  emit reloadRequested();
}

void OptionsWidget::paperBrightnessChanged(int value) {
  ColorCommonOptions opt(m_colorParams.colorCommonOptions());
  opt.setPaperBrightnessThreshold(value);
  m_colorParams.setColorCommonOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::paperSaturationChanged(int value) {
  ColorCommonOptions opt(m_colorParams.colorCommonOptions());
  opt.setPaperSaturationThreshold(value);
  m_colorParams.setColorCommonOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::paperCoverageChanged(double value) {
  ColorCommonOptions opt(m_colorParams.colorCommonOptions());
  opt.setPaperCoverageThreshold(value / 100.0);  // Convert from percentage to fraction
  m_colorParams.setColorCommonOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::adaptiveDetectionToggled(bool checked) {
  ColorCommonOptions opt(m_colorParams.colorCommonOptions());
  opt.setUseAdaptiveDetection(checked);
  m_colorParams.setColorCommonOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::forceWhiteBalanceToggled(const bool checked) {
  m_settings->setForceWhiteBalance(m_pageId, checked);
  emit reloadRequested();
}

void OptionsWidget::pickPaperColorClicked() {
  // Use native macOS color dialog which has a good eyedropper
  QColor currentColor = m_settings->getManualWhiteBalanceColor(m_pageId);
  if (!currentColor.isValid()) {
    currentColor = QColor(240, 230, 210);  // Start with a typical yellowed paper color
  }

  QColor color = QColorDialog::getColor(currentColor, this, tr("Pick Paper Color"));
  if (color.isValid()) {
    qDebug() << "OptionsWidget: User picked paper color:" << color;
    m_settings->setManualWhiteBalanceColor(m_pageId, color);
    updatePaperColorSwatch();
    emit reloadRequested();
  }
}

void OptionsWidget::clearPaperColorClicked() {
  m_settings->clearManualWhiteBalanceColor(m_pageId);
  updatePaperColorSwatch();
  emit reloadRequested();
}

void OptionsWidget::updatePaperColorSwatch() {
  QColor color = m_settings->getManualWhiteBalanceColor(m_pageId);
  if (color.isValid()) {
    QString styleSheet = QString("background-color: rgb(%1, %2, %3);")
                             .arg(color.red())
                             .arg(color.green())
                             .arg(color.blue());
    paperColorSwatch->setStyleSheet(styleSheet);
    paperColorSwatch->setToolTip(tr("Paper color: R=%1 G=%2 B=%3")
                                     .arg(color.red())
                                     .arg(color.green())
                                     .arg(color.blue()));
    clearPaperColorBtn->setEnabled(true);
  } else {
    paperColorSwatch->setStyleSheet("");
    paperColorSwatch->setToolTip(tr("No paper color set"));
    clearPaperColorBtn->setEnabled(false);
  }
}

void OptionsWidget::brightnessChanged(int value) {
  // Snap to center if within ±5 of zero for easy reset
  if (value > -5 && value < 5 && value != 0) {
    brightnessSlider->blockSignals(true);
    brightnessSlider->setValue(0);
    brightnessSlider->blockSignals(false);
    value = 0;
  }
  qDebug() << "OptionsWidget::brightnessChanged slider=" << value << "normalized=" << (value / 100.0);
  OutputProcessingParams opp = m_settings->getOutputProcessingParams(m_pageId);
  opp.setBrightness(value / 100.0);  // expand effective range
  m_settings->setOutputProcessingParams(m_pageId, opp);
  emit reloadRequested();
  emit invalidateThumbnail(m_pageId);
}

void OptionsWidget::contrastChanged(int value) {
  // Snap to center if within ±5 of zero for easy reset
  if (value > -5 && value < 5 && value != 0) {
    contrastSlider->blockSignals(true);
    contrastSlider->setValue(0);
    contrastSlider->blockSignals(false);
    value = 0;
  }
  qDebug() << "OptionsWidget::contrastChanged slider=" << value << "normalized=" << (value / 100.0);
  OutputProcessingParams opp = m_settings->getOutputProcessingParams(m_pageId);
  opp.setContrast(value / 100.0);  // expand effective range
  m_settings->setOutputProcessingParams(m_pageId, opp);
  emit reloadRequested();
  emit invalidateThumbnail(m_pageId);
}

void OptionsWidget::binarizationSettingsChanged() {
  emit reloadRequested();
  emit invalidateThumbnail(m_pageId);
}

void OptionsWidget::changeDpiButtonClicked() {
  auto* dialog = new ChangeDpiDialog(this, m_outputDpi, m_pageId, m_pageSelectionAccessor);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&, const Dpi&)), this,
          SLOT(dpiChanged(const std::set<PageId>&, const Dpi&)));
  dialog->show();
}

void OptionsWidget::applyColorsButtonClicked() {
  auto* dialog = new ApplyColorsDialog(this, m_pageId, m_pageSelectionAccessor,
                                       m_colorParams.colorMode(), m_settings);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&)), this, SLOT(applyColorsConfirmed(const std::set<PageId>&)));
  dialog->show();
}

void OptionsWidget::dpiChanged(const std::set<PageId>& pages, const Dpi& dpi) {
  for (const PageId& pageId : pages) {
    m_settings->setDpi(pageId, dpi);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  if (pages.find(m_pageId) != pages.end()) {
    m_outputDpi = dpi;
    updateDpiDisplay();
    emit reloadRequested();
  }
}

void OptionsWidget::applyColorsConfirmed(const std::set<PageId>& pages) {
  // Get current page's white balance settings to apply to other pages
  const bool forceWB = m_settings->getForceWhiteBalance(m_pageId);
  const QColor wbColor = m_settings->getManualWhiteBalanceColor(m_pageId);

  for (const PageId& pageId : pages) {
    m_settings->setColorParams(pageId, m_colorParams);
    m_settings->setPictureShapeOptions(pageId, m_pictureShapeOptions);
    // Also apply white balance settings
    m_settings->setForceWhiteBalance(pageId, forceWB);
    m_settings->setManualWhiteBalanceColor(pageId, wbColor);
  }

  // Request batch processing of ALL pages (including current if present)
  if (!pages.empty()) {
    emit batchProcessingRequested(pages);
  }
}

void OptionsWidget::applySplittingButtonClicked() {
  auto* dialog = new ApplyColorsDialog(this, m_pageId, m_pageSelectionAccessor,
                                       m_colorParams.colorMode(), m_settings);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("Apply Splitting Settings"));
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&)), this,
          SLOT(applySplittingOptionsConfirmed(const std::set<PageId>&)));
  dialog->show();
}

void OptionsWidget::applySplittingOptionsConfirmed(const std::set<PageId>& pages) {
  for (const PageId& pageId : pages) {
    m_settings->setSplittingOptions(pageId, m_splittingOptions);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  if (pages.find(m_pageId) != pages.end()) {
    emit reloadRequested();
  }
}

void OptionsWidget::despeckleToggled(bool checked) {
  if (checked) {
    handleDespeckleLevelChange(0.1 * despeckleSlider->value());
  } else {
    handleDespeckleLevelChange(0);
  };

  despeckleSlider->setEnabled(checked);
}

void OptionsWidget::despeckleSliderReleased() {
  const double value = 0.1 * despeckleSlider->value();
  handleDespeckleLevelChange(value);
}

void OptionsWidget::despeckleSliderValueChanged(int value) {
  const double newValue = 0.1 * value;

  const QString tooltipText(QString::number(newValue));
  despeckleSlider->setToolTip(tooltipText);

  // Show the tooltip immediately.
  const QPoint center(despeckleSlider->rect().center());
  QPoint tooltipPos(despeckleSlider->mapFromGlobal(QCursor::pos()));
  tooltipPos.setY(center.y());
  tooltipPos.setX(qBound(0, tooltipPos.x(), despeckleSlider->width()));
  tooltipPos = despeckleSlider->mapToGlobal(tooltipPos);
  QToolTip::showText(tooltipPos, tooltipText, despeckleSlider);

  if (despeckleSlider->isSliderDown()) {
    return;
  }

  handleDespeckleLevelChange(newValue, true);
}

void OptionsWidget::handleDespeckleLevelChange(const double level, const bool delay) {
  m_despeckleLevel = level;
  m_settings->setDespeckleLevel(m_pageId, level);

  bool handled = false;
  emit despeckleLevelChanged(level, &handled);

  if (handled) {
    // This means we are on the "Despeckling" tab.
    emit invalidateThumbnail(m_pageId);
  } else {
    if (delay) {
      m_delayedReloadRequest.start(750);
    } else {
      emit reloadRequested();
    }
  }
}

void OptionsWidget::applyDespeckleButtonClicked() {
  auto* dialog = new ApplyColorsDialog(this, m_pageId, m_pageSelectionAccessor,
                                       m_colorParams.colorMode(), m_settings);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("Apply Despeckling Level"));
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&)), this,
          SLOT(applyDespeckleConfirmed(const std::set<PageId>&)));
  dialog->show();
}

void OptionsWidget::applyDespeckleConfirmed(const std::set<PageId>& pages) {
  for (const PageId& pageId : pages) {
    m_settings->setDespeckleLevel(pageId, m_despeckleLevel);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  if (pages.find(m_pageId) != pages.end()) {
    emit reloadRequested();
  }
}

void OptionsWidget::changeDewarpingButtonClicked() {
  auto* dialog = new ChangeDewarpingDialog(this, m_pageId, m_dewarpingOptions, m_pageSelectionAccessor);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&, const DewarpingOptions&)), this,
          SLOT(dewarpingChanged(const std::set<PageId>&, const DewarpingOptions&)));
  dialog->show();
}

void OptionsWidget::dewarpingChanged(const std::set<PageId>& pages, const DewarpingOptions& opt) {
  for (const PageId& pageId : pages) {
    m_settings->setDewarpingOptions(pageId, opt);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  if (pages.find(m_pageId) != pages.end()) {
    if (m_dewarpingOptions != opt) {
      m_dewarpingOptions = opt;


      // We also have to reload if we are currently on the "Fill Zones" tab,
      // as it makes use of original <-> dewarped coordinate mapping,
      // which is too hard to update without reloading.  For consistency,
      // we reload not just on TAB_FILL_ZONES but on all tabs except TAB_DEWARPING.
      // PS: the static original <-> dewarped mappings are constructed
      // in Task::UiUpdater::updateUI().  Look for "new DewarpingPointMapper" there.
      if ((opt.dewarpingMode() == AUTO) || (m_lastTab != TAB_DEWARPING) || (opt.dewarpingMode() == MARGINAL)) {
        // Switch to the Output tab after reloading.
        m_lastTab = TAB_OUTPUT;
        // These depend on the value of m_lastTab.
        updateDpiDisplay();
        updateColorsDisplay();
        updateDewarpingDisplay();

        emit reloadRequested();
      } else {
        // This one we have to call anyway, as it depends on m_dewarpingMode.
        updateDewarpingDisplay();
      }
    }
  }
}  // OptionsWidget::dewarpingChanged

void OptionsWidget::applyDepthPerceptionButtonClicked() {
  auto* dialog = new ApplyColorsDialog(this, m_pageId, m_pageSelectionAccessor,
                                       m_colorParams.colorMode(), m_settings);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("Apply Depth Perception"));
  connect(dialog, SIGNAL(accepted(const std::set<PageId>&)), this,
          SLOT(applyDepthPerceptionConfirmed(const std::set<PageId>&)));
  dialog->show();
}

void OptionsWidget::applyDepthPerceptionConfirmed(const std::set<PageId>& pages) {
  for (const PageId& pageId : pages) {
    m_settings->setDepthPerception(pageId, m_depthPerception);
  }

  if (pages.size() > 1) {
    emit invalidateAllThumbnails();
  } else {
    for (const PageId& pageId : pages) {
      emit invalidateThumbnail(pageId);
    }
  }

  if (pages.find(m_pageId) != pages.end()) {
    emit reloadRequested();
  }
}

void OptionsWidget::depthPerceptionChangedSlot(int val) {
  m_depthPerception.setValue(0.1 * val);
  const QString tooltipText(QString::number(m_depthPerception.value()));
  depthPerceptionSlider->setToolTip(tooltipText);

  // Show the tooltip immediately.
  const QPoint center(depthPerceptionSlider->rect().center());
  QPoint tooltipPos(depthPerceptionSlider->mapFromGlobal(QCursor::pos()));
  tooltipPos.setY(center.y());
  tooltipPos.setX(qBound(0, tooltipPos.x(), depthPerceptionSlider->width()));
  tooltipPos = depthPerceptionSlider->mapToGlobal(tooltipPos);
  QToolTip::showText(tooltipPos, tooltipText, depthPerceptionSlider);

  m_settings->setDepthPerception(m_pageId, m_depthPerception);
  // Propagate the signal.
  emit depthPerceptionChanged(m_depthPerception.value());
}

void OptionsWidget::reloadIfNecessary() {
  ZoneSet savedPictureZones;
  ZoneSet savedFillZones;
  DewarpingOptions savedDewarpingOptions;
  dewarping::DistortionModel savedDistortionModel;
  DepthPerception savedDepthPerception;
  double savedDespeckleLevel = 1.0;

  std::unique_ptr<OutputParams> outputParams(m_settings->getOutputParams(m_pageId));
  if (outputParams) {
    savedPictureZones = outputParams->pictureZones();
    savedFillZones = outputParams->fillZones();
    savedDewarpingOptions = outputParams->outputImageParams().dewarpingMode();
    savedDistortionModel = outputParams->outputImageParams().distortionModel();
    savedDepthPerception = outputParams->outputImageParams().depthPerception();
    savedDespeckleLevel = outputParams->outputImageParams().despeckleLevel();
  }

  if (!PictureZoneComparator::equal(savedPictureZones, m_settings->pictureZonesForPage(m_pageId))) {
    emit reloadRequested();
    return;
  } else if (!FillZoneComparator::equal(savedFillZones, m_settings->fillZonesForPage(m_pageId))) {
    emit reloadRequested();
    return;
  }

  const Params params(m_settings->getParams(m_pageId));

  if (savedDespeckleLevel != params.despeckleLevel()) {
    emit reloadRequested();
    return;
  }

  if ((savedDewarpingOptions.dewarpingMode() == OFF) && (params.dewarpingOptions().dewarpingMode() == OFF)) {
  } else if (savedDepthPerception.value() != params.depthPerception().value()) {
    emit reloadRequested();
    return;
  } else if ((savedDewarpingOptions.dewarpingMode() == AUTO) && (params.dewarpingOptions().dewarpingMode() == AUTO)) {
  } else if ((savedDewarpingOptions.dewarpingMode() == MARGINAL)
             && (params.dewarpingOptions().dewarpingMode() == MARGINAL)) {
  } else if (!savedDistortionModel.matches(params.distortionModel())) {
    emit reloadRequested();
    return;
  } else if ((savedDewarpingOptions.dewarpingMode() == OFF) != (params.dewarpingOptions().dewarpingMode() == OFF)) {
    emit reloadRequested();
    return;
  }
}  // OptionsWidget::reloadIfNecessary

void OptionsWidget::updateDpiDisplay() {
  if (m_outputDpi.horizontal() != m_outputDpi.vertical()) {
    dpiLabel->setText(QString::fromLatin1("%1 x %2 dpi").arg(m_outputDpi.horizontal()).arg(m_outputDpi.vertical()));
  } else {
    dpiLabel->setText(QString::fromLatin1("%1 dpi").arg(m_outputDpi.horizontal()));
  }
}

void OptionsWidget::updateColorsDisplay() {
  colorModeSelector->blockSignals(true);

  ColorMode colorMode = m_colorParams.colorMode();
  // Map legacy COLOR_GRAYSCALE to COLOR for backward compatibility
  if (colorMode == COLOR_GRAYSCALE) {
    colorMode = COLOR;
  }
  const int colorModeIdx = colorModeSelector->findData(colorMode);
  colorModeSelector->setCurrentIndex(colorModeIdx);

  bool thresholdOptionsVisible = false;
  bool pictureShapeVisible = false;
  bool splittingOptionsVisible = false;
  switch (colorMode) {
    case MIXED:
      pictureShapeVisible = true;
      splittingOptionsVisible = true;
      // fall through
    case BLACK_AND_WHITE:
      thresholdOptionsVisible = true;
      // fall through
    case COLOR_GRAYSCALE:
    case COLOR:
    case GRAYSCALE:
      break;
    case AUTO_DETECT:
      // Should not be stored, but handle it
      break;
  }

  commonOptions->setVisible(true);
  ColorCommonOptions colorCommonOptions(m_colorParams.colorCommonOptions());
  BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());

  if (!blackWhiteOptions.normalizeIllumination() && colorMode == MIXED) {
    colorCommonOptions.setNormalizeIllumination(false);
  }
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  fillMarginsCB->setChecked(colorCommonOptions.fillMargins());
  fillMarginsCB->setVisible(true);
  fillOffcutCB->setChecked(colorCommonOptions.fillOffcut());
  fillOffcutCB->setVisible(true);
  equalizeIlluminationCB->setChecked(blackWhiteOptions.normalizeIllumination());
  // Hide BW illumination option for color/grayscale modes
  const bool isColorOrGrayscale = (colorMode == COLOR_GRAYSCALE || colorMode == COLOR || colorMode == GRAYSCALE);
  equalizeIlluminationCB->setVisible(!isColorOrGrayscale);
  equalizeIlluminationColorCB->setChecked(colorCommonOptions.normalizeIllumination());
  equalizeIlluminationColorCB->setVisible(colorMode != BLACK_AND_WHITE);
  // Allow illumination equalization in Color / Grayscale / Mixed as well (previously disabled in pure Color).
  equalizeIlluminationColorCB->setEnabled(isColorOrGrayscale || blackWhiteOptions.normalizeIllumination());
  // Force white balance - only for color modes
  forceWhiteBalanceCB->setChecked(m_settings->getForceWhiteBalance(m_pageId));
  forceWhiteBalanceCB->setVisible(colorMode != BLACK_AND_WHITE);
  // Paper color picker - only for color modes
  const bool showPaperColorPicker = (colorMode != BLACK_AND_WHITE);
  pickPaperColorBtn->setVisible(showPaperColorPicker);
  paperColorSwatch->setVisible(showPaperColorPicker);
  clearPaperColorBtn->setVisible(showPaperColorPicker);
  if (showPaperColorPicker) {
    updatePaperColorSwatch();
  }

  const OutputProcessingParams& opp = m_settings->getOutputProcessingParams(m_pageId);
  brightnessSlider->setValue(static_cast<int>(opp.brightness() * 100.0));
  contrastSlider->setValue(static_cast<int>(opp.contrast() * 100.0));

  // Paper detection thresholds - populate from settings (using colorCommonOptions from line 794)
  paperBrightnessSB->setValue(colorCommonOptions.paperBrightnessThreshold());
  paperSaturationSB->setValue(colorCommonOptions.paperSaturationThreshold());
  paperCoverageSB->setValue(colorCommonOptions.paperCoverageThreshold() * 100.0);  // Display as percentage
  adaptiveDetectionCB->setChecked(colorCommonOptions.useAdaptiveDetection());
  // Show paper detection controls when either illumination equalization is enabled
  const bool showPaperDetection = equalizeIlluminationCB->isChecked() || equalizeIlluminationColorCB->isChecked();
  paperDetectionWidget->setVisible(showPaperDetection);

  savitzkyGolaySmoothingCB->setChecked(blackWhiteOptions.isSavitzkyGolaySmoothingEnabled());
  savitzkyGolaySmoothingCB->setVisible(thresholdOptionsVisible);
  morphologicalSmoothingCB->setChecked(blackWhiteOptions.isMorphologicalSmoothingEnabled());
  morphologicalSmoothingCB->setVisible(thresholdOptionsVisible);

  modePanel->setVisible(m_lastTab != TAB_DEWARPING);
  pictureShapeOptions->setVisible(pictureShapeVisible);
  thresholdOptions->setVisible(thresholdOptionsVisible);
  despecklePanel->setVisible(thresholdOptionsVisible && m_lastTab != TAB_DEWARPING);
  // Color/grayscale operations only visible for those modes
  colorOperationsOptions->setVisible(isColorOrGrayscale && m_lastTab != TAB_DEWARPING);

  splittingOptions->setVisible(splittingOptionsVisible);
  splittingCB->setChecked(m_splittingOptions.isSplitOutput());
  switch (m_splittingOptions.getSplittingMode()) {
    case BLACK_AND_WHITE_FOREGROUND:
      bwForegroundRB->setChecked(true);
      break;
    case COLOR_FOREGROUND:
      colorForegroundRB->setChecked(true);
      break;
  }
  originalBackgroundCB->setChecked(m_splittingOptions.isOriginalBackgroundEnabled());
  colorForegroundRB->setEnabled(m_splittingOptions.isSplitOutput());
  bwForegroundRB->setEnabled(m_splittingOptions.isSplitOutput());
  originalBackgroundCB->setEnabled(m_splittingOptions.isSplitOutput()
                                   && (m_splittingOptions.getSplittingMode() == BLACK_AND_WHITE_FOREGROUND));

  thresholdMethodBox->setCurrentIndex((int) blackWhiteOptions.getBinarizationMethod());
  binarizationOptions->setCurrentIndex((int) blackWhiteOptions.getBinarizationMethod());

  // Set radio buttons based on filling color (visible for non-B&W modes)
  const FillingColor fillingColor = colorCommonOptions.getFillingColor();
  fillWhiteRB->setChecked(fillingColor == FILL_WHITE || fillingColor == FILL_BLACK);
  fillBackgroundRB->setChecked(fillingColor == FILL_BACKGROUND);
  fillWhiteRB->setVisible(colorMode != BLACK_AND_WHITE);
  fillBackgroundRB->setVisible(colorMode != BLACK_AND_WHITE);

  colorSegmentationCB->setVisible(thresholdOptionsVisible);
  segmenterOptionsWidget->setVisible(thresholdOptionsVisible);
  segmenterOptionsWidget->setEnabled(blackWhiteOptions.getColorSegmenterOptions().isEnabled());
  if (thresholdOptionsVisible) {
    posterizeCB->setEnabled(blackWhiteOptions.getColorSegmenterOptions().isEnabled());
    posterizeOptionsWidget->setEnabled(blackWhiteOptions.getColorSegmenterOptions().isEnabled()
                                       && colorCommonOptions.getPosterizationOptions().isEnabled());
  } else {
    posterizeCB->setEnabled(true);
    posterizeOptionsWidget->setEnabled(colorCommonOptions.getPosterizationOptions().isEnabled());
  }
  wienerCoef->setValue(colorCommonOptions.wienerCoef());
  wienerWindowSize->setValue(colorCommonOptions.wienerWindowSize());
  colorSegmentationCB->setChecked(blackWhiteOptions.getColorSegmenterOptions().isEnabled());
  reduceNoiseSB->setValue(blackWhiteOptions.getColorSegmenterOptions().getNoiseReduction());
  redAdjustmentSB->setValue(blackWhiteOptions.getColorSegmenterOptions().getRedThresholdAdjustment());
  greenAdjustmentSB->setValue(blackWhiteOptions.getColorSegmenterOptions().getGreenThresholdAdjustment());
  blueAdjustmentSB->setValue(blackWhiteOptions.getColorSegmenterOptions().getBlueThresholdAdjustment());
  posterizeCB->setChecked(colorCommonOptions.getPosterizationOptions().isEnabled());
  posterizeLevelSB->setValue(colorCommonOptions.getPosterizationOptions().getLevel());
  posterizeNormalizationCB->setChecked(colorCommonOptions.getPosterizationOptions().isNormalizationEnabled());
  posterizeForceBwCB->setChecked(colorCommonOptions.getPosterizationOptions().isForceBlackAndWhite());

  if (pictureShapeVisible) {
    const int pictureShapeIdx = pictureShapeSelector->findData(m_pictureShapeOptions.getPictureShape());
    pictureShapeSelector->setCurrentIndex(pictureShapeIdx);
    pictureShapeSensitivitySB->setValue(m_pictureShapeOptions.getSensitivity());
    pictureShapeSensitivityOptions->setVisible(m_pictureShapeOptions.getPictureShape() == RECTANGULAR_SHAPE);
    higherSearchSensitivityCB->setChecked(m_pictureShapeOptions.isHigherSearchSensitivity());
    higherSearchSensitivityCB->setVisible(m_pictureShapeOptions.getPictureShape() != OFF_SHAPE);
  }

  if (thresholdOptionsVisible) {
    if (m_despeckleLevel != 0) {
      despeckleCB->setChecked(true);
      despeckleSlider->setValue(qRound(10 * m_despeckleLevel));
    } else {
      despeckleCB->setChecked(false);
    }
    despeckleSlider->setEnabled(m_despeckleLevel != 0);
    despeckleSlider->setToolTip(QString::number(0.1 * despeckleSlider->value()));

    for (int i = 0; i < binarizationOptions->count(); i++) {
      auto* widget = dynamic_cast<BinarizationOptionsWidget*>(binarizationOptions->widget(i));
      widget->updateUi(m_pageId);
    }
  }

  colorModeSelector->blockSignals(false);
}  // OptionsWidget::updateColorsDisplay

void OptionsWidget::updateDewarpingDisplay() {
  depthPerceptionPanel->setVisible(m_lastTab == TAB_DEWARPING);

  switch (m_dewarpingOptions.dewarpingMode()) {
    case OFF:
      dewarpingStatusLabel->setText(tr("Off"));
      break;
    case AUTO:
      dewarpingStatusLabel->setText(tr("Auto"));
      break;
    case MANUAL:
      dewarpingStatusLabel->setText(tr("Manual"));
      break;
    case MARGINAL:
      dewarpingStatusLabel->setText(tr("Marginal"));
      break;
  }

  if ((m_dewarpingOptions.dewarpingMode() == MANUAL) || (m_dewarpingOptions.dewarpingMode() == MARGINAL)) {
    QString dewarpingStatus = dewarpingStatusLabel->text();
    if (m_dewarpingOptions.needPostDeskew()) {
      const double deskewAngle = -std::round(m_dewarpingOptions.getPostDeskewAngle() * 100) / 100;
      dewarpingStatus += " (" + tr("deskew") + ": " + QString::number(deskewAngle) + QChar(0x00B0) + ")";
    } else {
      dewarpingStatus += " (" + tr("deskew disabled") + ")";
    }
    dewarpingStatusLabel->setText(dewarpingStatus);
  }

  depthPerceptionSlider->blockSignals(true);
  depthPerceptionSlider->setValue(qRound(m_depthPerception.value() * 10));
  depthPerceptionSlider->blockSignals(false);
}

void OptionsWidget::savitzkyGolaySmoothingToggled(bool checked) {
  BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
  opt.setSavitzkyGolaySmoothingEnabled(checked);
  m_colorParams.setBlackWhiteOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::morphologicalSmoothingToggled(bool checked) {
  BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
  opt.setMorphologicalSmoothingEnabled(checked);
  m_colorParams.setBlackWhiteOptions(opt);
  m_settings->setColorParams(m_pageId, m_colorParams);
  emit reloadRequested();
}

void OptionsWidget::bwForegroundToggled(bool checked) {
  if (!checked) {
    return;
  }

  originalBackgroundCB->setEnabled(checked);

  m_splittingOptions.setSplittingMode(BLACK_AND_WHITE_FOREGROUND);
  m_settings->setSplittingOptions(m_pageId, m_splittingOptions);
  emit reloadRequested();
}

void OptionsWidget::colorForegroundToggled(bool checked) {
  if (!checked) {
    return;
  }

  originalBackgroundCB->setEnabled(!checked);

  m_splittingOptions.setSplittingMode(COLOR_FOREGROUND);
  m_settings->setSplittingOptions(m_pageId, m_splittingOptions);
  emit reloadRequested();
}

void OptionsWidget::splittingToggled(bool checked) {
  m_splittingOptions.setSplitOutput(checked);

  bwForegroundRB->setEnabled(checked);
  colorForegroundRB->setEnabled(checked);
  originalBackgroundCB->setEnabled(checked && bwForegroundRB->isChecked());

  m_settings->setSplittingOptions(m_pageId, m_splittingOptions);
  emit reloadRequested();
}

void OptionsWidget::originalBackgroundToggled(bool checked) {
  m_splittingOptions.setOriginalBackgroundEnabled(checked);

  m_settings->setSplittingOptions(m_pageId, m_splittingOptions);
  emit reloadRequested();
}

void OptionsWidget::wienerCoefChanged(double value) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  colorCommonOptions.setWienerCoef(value);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::wienerWindowSizeChanged(int value) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  colorCommonOptions.setWienerWindowSize(value);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::colorSegmentationToggled(bool checked) {
  BlackWhiteOptions blackWhiteOptions = m_colorParams.blackWhiteOptions();
  BlackWhiteOptions::ColorSegmenterOptions segmenterOptions = blackWhiteOptions.getColorSegmenterOptions();
  segmenterOptions.setEnabled(checked);
  blackWhiteOptions.setColorSegmenterOptions(segmenterOptions);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  segmenterOptionsWidget->setEnabled(checked);
  if ((m_colorParams.colorMode() == BLACK_AND_WHITE) || (m_colorParams.colorMode() == MIXED)) {
    posterizeCB->setEnabled(checked);
    posterizeOptionsWidget->setEnabled(checked && posterizeCB->isChecked());
  }

  emit reloadRequested();
}

void OptionsWidget::reduceNoiseChanged(int value) {
  BlackWhiteOptions blackWhiteOptions = m_colorParams.blackWhiteOptions();
  BlackWhiteOptions::ColorSegmenterOptions segmenterOptions = blackWhiteOptions.getColorSegmenterOptions();
  segmenterOptions.setNoiseReduction(value);
  blackWhiteOptions.setColorSegmenterOptions(segmenterOptions);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::redAdjustmentChanged(int value) {
  BlackWhiteOptions blackWhiteOptions = m_colorParams.blackWhiteOptions();
  BlackWhiteOptions::ColorSegmenterOptions segmenterOptions = blackWhiteOptions.getColorSegmenterOptions();
  segmenterOptions.setRedThresholdAdjustment(value);
  blackWhiteOptions.setColorSegmenterOptions(segmenterOptions);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::greenAdjustmentChanged(int value) {
  BlackWhiteOptions blackWhiteOptions = m_colorParams.blackWhiteOptions();
  BlackWhiteOptions::ColorSegmenterOptions segmenterOptions = blackWhiteOptions.getColorSegmenterOptions();
  segmenterOptions.setGreenThresholdAdjustment(value);
  blackWhiteOptions.setColorSegmenterOptions(segmenterOptions);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::blueAdjustmentChanged(int value) {
  BlackWhiteOptions blackWhiteOptions = m_colorParams.blackWhiteOptions();
  BlackWhiteOptions::ColorSegmenterOptions segmenterOptions = blackWhiteOptions.getColorSegmenterOptions();
  segmenterOptions.setBlueThresholdAdjustment(value);
  blackWhiteOptions.setColorSegmenterOptions(segmenterOptions);
  m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::posterizeToggled(bool checked) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  ColorCommonOptions::PosterizationOptions posterizationOptions = colorCommonOptions.getPosterizationOptions();
  posterizationOptions.setEnabled(checked);
  colorCommonOptions.setPosterizationOptions(posterizationOptions);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  posterizeOptionsWidget->setEnabled(checked);

  emit reloadRequested();
}

void OptionsWidget::posterizeLevelChanged(int value) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  ColorCommonOptions::PosterizationOptions posterizationOptions = colorCommonOptions.getPosterizationOptions();
  posterizationOptions.setLevel(value);
  colorCommonOptions.setPosterizationOptions(posterizationOptions);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  m_delayedReloadRequest.start(750);
}

void OptionsWidget::posterizeNormalizationToggled(bool checked) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  ColorCommonOptions::PosterizationOptions posterizationOptions = colorCommonOptions.getPosterizationOptions();
  posterizationOptions.setNormalizationEnabled(checked);
  colorCommonOptions.setPosterizationOptions(posterizationOptions);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  emit reloadRequested();
}

void OptionsWidget::posterizeForceBwToggled(bool checked) {
  ColorCommonOptions colorCommonOptions = m_colorParams.colorCommonOptions();
  ColorCommonOptions::PosterizationOptions posterizationOptions = colorCommonOptions.getPosterizationOptions();
  posterizationOptions.setForceBlackAndWhite(checked);
  colorCommonOptions.setPosterizationOptions(posterizationOptions);
  m_colorParams.setColorCommonOptions(colorCommonOptions);
  m_settings->setColorParams(m_pageId, m_colorParams);

  emit reloadRequested();
}

void OptionsWidget::updateBinarizationOptionsDisplay(int idx) {
  for (int i = 0; i < binarizationOptions->count(); i++) {
    QWidget* currentWidget = binarizationOptions->widget(i);
    currentWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    currentWidget->resize(0, 0);

    disconnect(currentWidget, SIGNAL(stateChanged()), this, SLOT(binarizationSettingsChanged()));
  }

  QWidget* widget = binarizationOptions->widget(idx);
  widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  widget->adjustSize();
  binarizationOptions->adjustSize();

  connect(widget, SIGNAL(stateChanged()), this, SLOT(binarizationSettingsChanged()));
}

void OptionsWidget::addBinarizationOptionsWidget(BinarizationOptionsWidget* widget) {
  widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  binarizationOptions->addWidget(widget);
}

void OptionsWidget::sendReloadRequested() {
  emit reloadRequested();
}

#define CONNECT(...) m_connectionManager.addConnection(connect(__VA_ARGS__))

void OptionsWidget::setupUiConnections() {
  CONNECT(changeDpiButton, SIGNAL(clicked()), this, SLOT(changeDpiButtonClicked()));
  CONNECT(colorModeSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(colorModeChanged(int)));
  CONNECT(thresholdMethodBox, SIGNAL(currentIndexChanged(int)), this, SLOT(thresholdMethodChanged(int)));
  CONNECT(fillWhiteRB, SIGNAL(clicked()), this, SLOT(fillingColorChanged()));
  CONNECT(fillBackgroundRB, SIGNAL(clicked()), this, SLOT(fillingColorChanged()));
  CONNECT(pictureShapeSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(pictureShapeChanged(int)));
  CONNECT(pictureShapeSensitivitySB, SIGNAL(valueChanged(int)), this, SLOT(pictureShapeSensitivityChanged(int)));
  CONNECT(higherSearchSensitivityCB, SIGNAL(clicked(bool)), this, SLOT(higherSearchSensivityToggled(bool)));

  CONNECT(wienerCoef, SIGNAL(valueChanged(double)), this, SLOT(wienerCoefChanged(double)));
  CONNECT(wienerWindowSize, SIGNAL(valueChanged(int)), this, SLOT(wienerWindowSizeChanged(int)));
  CONNECT(colorSegmentationCB, SIGNAL(clicked(bool)), this, SLOT(colorSegmentationToggled(bool)));
  CONNECT(reduceNoiseSB, SIGNAL(valueChanged(int)), this, SLOT(reduceNoiseChanged(int)));
  CONNECT(redAdjustmentSB, SIGNAL(valueChanged(int)), this, SLOT(redAdjustmentChanged(int)));
  CONNECT(greenAdjustmentSB, SIGNAL(valueChanged(int)), this, SLOT(greenAdjustmentChanged(int)));
  CONNECT(blueAdjustmentSB, SIGNAL(valueChanged(int)), this, SLOT(blueAdjustmentChanged(int)));
  CONNECT(posterizeCB, SIGNAL(clicked(bool)), this, SLOT(posterizeToggled(bool)));
  CONNECT(posterizeLevelSB, SIGNAL(valueChanged(int)), this, SLOT(posterizeLevelChanged(int)));
  CONNECT(posterizeNormalizationCB, SIGNAL(clicked(bool)), this, SLOT(posterizeNormalizationToggled(bool)));
  CONNECT(posterizeForceBwCB, SIGNAL(clicked(bool)), this, SLOT(posterizeForceBwToggled(bool)));

  CONNECT(fillMarginsCB, SIGNAL(clicked(bool)), this, SLOT(fillMarginsToggled(bool)));
  CONNECT(fillOffcutCB, SIGNAL(clicked(bool)), this, SLOT(fillOffcutToggled(bool)));
  CONNECT(equalizeIlluminationCB, SIGNAL(clicked(bool)), this, SLOT(equalizeIlluminationToggled(bool)));
  CONNECT(equalizeIlluminationColorCB, SIGNAL(clicked(bool)), this, SLOT(equalizeIlluminationColorToggled(bool)));
  CONNECT(paperBrightnessSB, SIGNAL(valueChanged(int)), this, SLOT(paperBrightnessChanged(int)));
  CONNECT(paperSaturationSB, SIGNAL(valueChanged(int)), this, SLOT(paperSaturationChanged(int)));
  CONNECT(paperCoverageSB, SIGNAL(valueChanged(double)), this, SLOT(paperCoverageChanged(double)));
  CONNECT(adaptiveDetectionCB, SIGNAL(clicked(bool)), this, SLOT(adaptiveDetectionToggled(bool)));
  CONNECT(forceWhiteBalanceCB, SIGNAL(clicked(bool)), this, SLOT(forceWhiteBalanceToggled(bool)));
  CONNECT(pickPaperColorBtn, SIGNAL(clicked()), this, SLOT(pickPaperColorClicked()));
  CONNECT(clearPaperColorBtn, SIGNAL(clicked()), this, SLOT(clearPaperColorClicked()));
  CONNECT(brightnessSlider, SIGNAL(valueChanged(int)), this, SLOT(brightnessChanged(int)));
  CONNECT(contrastSlider, SIGNAL(valueChanged(int)), this, SLOT(contrastChanged(int)));
  CONNECT(savitzkyGolaySmoothingCB, SIGNAL(clicked(bool)), this, SLOT(savitzkyGolaySmoothingToggled(bool)));
  CONNECT(morphologicalSmoothingCB, SIGNAL(clicked(bool)), this, SLOT(morphologicalSmoothingToggled(bool)));
  CONNECT(splittingCB, SIGNAL(clicked(bool)), this, SLOT(splittingToggled(bool)));
  CONNECT(bwForegroundRB, SIGNAL(clicked(bool)), this, SLOT(bwForegroundToggled(bool)));
  CONNECT(colorForegroundRB, SIGNAL(clicked(bool)), this, SLOT(colorForegroundToggled(bool)));
  CONNECT(originalBackgroundCB, SIGNAL(clicked(bool)), this, SLOT(originalBackgroundToggled(bool)));
  CONNECT(applyColorsButton, SIGNAL(clicked()), this, SLOT(applyColorsButtonClicked()));

  CONNECT(applySplittingButton, SIGNAL(clicked()), this, SLOT(applySplittingButtonClicked()));

  CONNECT(changeDewarpingButton, SIGNAL(clicked()), this, SLOT(changeDewarpingButtonClicked()));

  CONNECT(applyDepthPerceptionButton, SIGNAL(clicked()), this, SLOT(applyDepthPerceptionButtonClicked()));

  CONNECT(despeckleCB, SIGNAL(clicked(bool)), this, SLOT(despeckleToggled(bool)));
  CONNECT(despeckleSlider, SIGNAL(sliderReleased()), this, SLOT(despeckleSliderReleased()));
  CONNECT(despeckleSlider, SIGNAL(valueChanged(int)), this, SLOT(despeckleSliderValueChanged(int)));
  CONNECT(applyDespeckleButton, SIGNAL(clicked()), this, SLOT(applyDespeckleButtonClicked()));
  CONNECT(depthPerceptionSlider, SIGNAL(valueChanged(int)), this, SLOT(depthPerceptionChangedSlot(int)));
  CONNECT(&m_delayedReloadRequest, SIGNAL(timeout()), this, SLOT(sendReloadRequested()));

}

#undef CONNECT

ImageViewTab OptionsWidget::lastTab() const {
  return m_lastTab;
}

const DepthPerception& OptionsWidget::depthPerception() const {
  return m_depthPerception;
}

void OptionsWidget::applyColorParamsToSelectedPages() {
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();

  // Only apply to multiple pages if current page is in selection and there are multiple selected
  if (selectedPages.size() > 1 && selectedPages.find(m_pageId) != selectedPages.end()) {
    std::set<PageId> pagesToReprocess;

    for (const PageId& pageId : selectedPages) {
      if (pageId == m_pageId) {
        // Include current page in batch - it will be processed along with others
        pagesToReprocess.insert(pageId);
        continue;
      }

      m_settings->setColorParams(pageId, m_colorParams);
      pagesToReprocess.insert(pageId);

      // Also update finalize settings so finalize filter stays in sync
      if (m_finalizeSettings) {
        finalize::ColorMode finalizeMode;
        switch (m_colorParams.colorMode()) {
          case BLACK_AND_WHITE:
            finalizeMode = finalize::ColorMode::BlackAndWhite;
            break;
          case GRAYSCALE:
            finalizeMode = finalize::ColorMode::Grayscale;
            break;
          case COLOR:
          default:
            finalizeMode = finalize::ColorMode::Color;
            break;
        }
        m_finalizeSettings->setColorMode(pageId, finalizeMode);
      }
    }

    // Request batch processing of ALL selected pages (including current)
    if (!pagesToReprocess.empty()) {
      emit batchProcessingRequested(pagesToReprocess);
    }
  }
}

void OptionsWidget::updateSelectionIndicator() {
  const std::set<PageId> selectedPages = m_pageSelectionAccessor.selectedPages();
  if (selectedPages.size() > 1 && selectedPages.find(m_pageId) != selectedPages.end()) {
    selectionIndicatorLabel->setText(tr("Editing %1 pages").arg(selectedPages.size()));
    selectionIndicatorLabel->setStyleSheet("QLabel { color: #4a90d9; font-weight: bold; }");
    selectionIndicatorLabel->show();
  } else {
    selectionIndicatorLabel->hide();
  }
}

}  // namespace output
