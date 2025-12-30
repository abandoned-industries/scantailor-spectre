// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <QDebug>
#include <QString>

#include "../../Utils.h"
#include "AbstractRelinker.h"
#include "FillColorProperty.h"
#include "LeptonicaDetector.h"
#include "ImageLoader.h"
#include "ImageTypeDetector.h"
#include "PageSequence.h"
#include "PictureLayerProperty.h"
#include "RelinkablePath.h"

using namespace core;

namespace output {
Settings::Settings()
    : m_defaultPictureZoneProps(initialPictureZoneProps()), m_defaultFillZoneProps(initialFillZoneProps()) {}

Settings::~Settings() = default;

void Settings::clear() {
  const QMutexLocker locker(&m_mutex);

  initialPictureZoneProps().swap(m_defaultPictureZoneProps);
  initialFillZoneProps().swap(m_defaultFillZoneProps);
  m_perPageParams.clear();
  m_perPageOutputParams.clear();
  m_perPagePictureZones.clear();
  m_perPageFillZones.clear();
  m_perPageOutputProcessingParams.clear();
  m_colorModeDetectedPages.clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  const QMutexLocker locker(&m_mutex);

  PerPageParams newParams;
  PerPageOutputParams newOutputParams;
  PerPageZones newPictureZones;
  PerPageZones newFillZones;
  PerPageOutputProcessingParams newOutputProcessingParams;

  for (const PerPageParams::value_type& kv : m_perPageParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newParams.insert(PerPageParams::value_type(newPageId, kv.second));
  }

  for (const PerPageOutputParams::value_type& kv : m_perPageOutputParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newOutputParams.insert(PerPageOutputParams::value_type(newPageId, kv.second));
  }

  for (const PerPageZones::value_type& kv : m_perPagePictureZones) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newPictureZones.insert(PerPageZones::value_type(newPageId, kv.second));
  }

  for (const PerPageZones::value_type& kv : m_perPageFillZones) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newFillZones.insert(PerPageZones::value_type(newPageId, kv.second));
  }

  for (const PerPageOutputProcessingParams::value_type& kv : m_perPageOutputProcessingParams) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newOutputProcessingParams.insert(PerPageOutputProcessingParams::value_type(newPageId, kv.second));
  }

  m_perPageParams.swap(newParams);
  m_perPageOutputParams.swap(newOutputParams);
  m_perPagePictureZones.swap(newPictureZones);
  m_perPageFillZones.swap(newFillZones);
  m_perPageOutputProcessingParams.swap(newOutputProcessingParams);
}  // Settings::performRelinking

Params Settings::getParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it != m_perPageParams.end()) {
    return it->second;
  } else {
    // Return default params with project-level DPI
    Params params;
    params.setOutputDpi(m_defaultDpi);
    return params;
  }
}

Params Settings::getParamsOrDetect(const PageId& pageId, const QString& sourceImagePath) {
  // First check if params exist (quick lock)
  {
    const QMutexLocker locker(&m_mutex);
    const auto it(m_perPageParams.find(pageId));
    if (it != m_perPageParams.end()) {
      return it->second;
    }
  }
  // Mutex released here - Vision can run without blocking other threads

  // No existing params - detect color mode from source image
  const ImageId& imageId = pageId.imageId();
  qDebug() << "Auto-detecting color mode for:" << imageId.filePath() << "page:" << imageId.page();

  Params params;
  ColorParams colorParams;

  // Load the actual page image using ImageLoader (properly handles PDFs and multi-page files)
  QImage image = ImageLoader::load(imageId);
  if (image.isNull()) {
    qDebug() << "getParamsOrDetect: Failed to load image, defaulting to COLOR_GRAYSCALE";
    colorParams.setColorMode(COLOR_GRAYSCALE);
    params.setColorParams(colorParams);

    // Store the params
    {
      const QMutexLocker locker(&m_mutex);
      const auto it(m_perPageParams.find(pageId));
      if (it != m_perPageParams.end()) {
        return it->second;
      }
      m_perPageParams.insert(PerPageParams::value_type(pageId, params));
    }
    return params;
  }

  // Use Leptonica for document color type detection
  const LeptonicaDetector::ColorType colorType = LeptonicaDetector::detect(image);

  switch (colorType) {
    case LeptonicaDetector::ColorType::BlackWhite:
      colorParams.setColorMode(BLACK_AND_WHITE);
      qDebug() << "Leptonica detected B&W (getParamsOrDetect):" << imageId.filePath() << "page:" << imageId.page();
      break;
    case LeptonicaDetector::ColorType::Color:
      colorParams.setColorMode(COLOR);
      qDebug() << "Leptonica detected Color (getParamsOrDetect):" << imageId.filePath() << "page:" << imageId.page();
      break;
    case LeptonicaDetector::ColorType::Grayscale:
    default:
      colorParams.setColorMode(GRAYSCALE);
      qDebug() << "Leptonica detected Grayscale (getParamsOrDetect):" << imageId.filePath() << "page:" << imageId.page();
      break;
  }
  params.setColorParams(colorParams);

  // Re-acquire lock to store params
  {
    const QMutexLocker locker(&m_mutex);
    const auto it(m_perPageParams.find(pageId));
    if (it != m_perPageParams.end()) {
      // Params already exist - another thread may have stored results
      // Only update if we detected COLOR_GRAYSCALE (safer) or the existing is B&W default
      // This prevents a late-finishing thread with B&W from overwriting a correct COLOR_GRAYSCALE
      const ColorMode existingMode = it->second.colorParams().colorMode();
      const ColorMode detectedMode = colorParams.colorMode();

      // NEVER overwrite user-set color mode
      if (it->second.colorParams().isColorModeUserSet()) {
        qDebug() << "getParamsOrDetect: NOT overwriting USER-SET mode" << static_cast<int>(existingMode)
                 << "with detected mode" << static_cast<int>(detectedMode);
        return it->second;
      }

      // Update only if:
      // 1. We detected COLOR_GRAYSCALE (always safe, preserves tones)
      // 2. OR we detected MIXED (document with images - also safe)
      // 3. OR the existing mode is B&W (might be wrong auto-detection)
      bool shouldUpdate = (detectedMode == COLOR_GRAYSCALE) ||
                          (detectedMode == MIXED) ||
                          (existingMode == BLACK_AND_WHITE);

      if (shouldUpdate) {
        qDebug() << "getParamsOrDetect: Updating existing params with detected color mode:"
                 << static_cast<int>(detectedMode) << "(existing was:" << static_cast<int>(existingMode) << ")";
        it->second.setColorParams(colorParams);
      } else {
        qDebug() << "getParamsOrDetect: NOT overwriting existing mode" << static_cast<int>(existingMode)
                 << "with detected mode" << static_cast<int>(detectedMode);
      }
      return it->second;
    }
    // Store the detected params so they persist
    m_perPageParams.insert(PerPageParams::value_type(pageId, params));
  }

  return params;
}

void Settings::setParams(const PageId& pageId, const Params& params) {
  const QMutexLocker locker(&m_mutex);
  qDebug() << "setParams called for page - params being stored externally";
  Utils::mapSetValue(m_perPageParams, pageId, params);
}

void Settings::setColorParams(const PageId& pageId, const ColorParams& prms) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    qDebug() << "setColorParams creating NEW params for page";
    Params params;
    params.setColorParams(prms);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    qDebug() << "setColorParams updating existing params";
    it->second.setColorParams(prms);
  }
}

void Settings::setPictureShapeOptions(const PageId& pageId, PictureShapeOptions pictureShapeOptions) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setPictureShapeOptions(pictureShapeOptions);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setPictureShapeOptions(pictureShapeOptions);
  }
}

void Settings::setDpi(const PageId& pageId, const Dpi& dpi) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setOutputDpi(dpi);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setOutputDpi(dpi);
  }
}

void Settings::setDewarpingOptions(const PageId& pageId, const DewarpingOptions& opt) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDewarpingOptions(opt);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDewarpingOptions(opt);
  }
}

void Settings::setSplittingOptions(const PageId& pageId, const SplittingOptions& opt) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setSplittingOptions(opt);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setSplittingOptions(opt);
  }
}

void Settings::setDistortionModel(const PageId& pageId, const dewarping::DistortionModel& model) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDistortionModel(model);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDistortionModel(model);
  }
}

void Settings::setDepthPerception(const PageId& pageId, const DepthPerception& depthPerception) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDepthPerception(depthPerception);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDepthPerception(depthPerception);
  }
}

void Settings::setDespeckleLevel(const PageId& pageId, double level) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setDespeckleLevel(level);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setDespeckleLevel(level);
  }
}

std::unique_ptr<OutputParams> Settings::getOutputParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageOutputParams.find(pageId));
  if (it != m_perPageOutputParams.end()) {
    return std::make_unique<OutputParams>(it->second);
  } else {
    return nullptr;
  }
}

void Settings::removeOutputParams(const PageId& pageId) {
  const QMutexLocker locker(&m_mutex);
  m_perPageOutputParams.erase(pageId);
}

void Settings::setOutputParams(const PageId& pageId, const OutputParams& params) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageOutputParams, pageId, params);
}

ZoneSet Settings::pictureZonesForPage(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPagePictureZones.find(pageId));
  if (it != m_perPagePictureZones.end()) {
    return it->second;
  } else {
    return ZoneSet();
  }
}

ZoneSet Settings::fillZonesForPage(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageFillZones.find(pageId));
  if (it != m_perPageFillZones.end()) {
    return it->second;
  } else {
    return ZoneSet();
  }
}

void Settings::setPictureZones(const PageId& pageId, const ZoneSet& zones) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPagePictureZones, pageId, zones);
}

void Settings::setFillZones(const PageId& pageId, const ZoneSet& zones) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageFillZones, pageId, zones);
}

PropertySet Settings::defaultPictureZoneProperties() const {
  const QMutexLocker locker(&m_mutex);
  return m_defaultPictureZoneProps;
}

PropertySet Settings::defaultFillZoneProperties() const {
  const QMutexLocker locker(&m_mutex);
  return m_defaultFillZoneProps;
}

void Settings::setDefaultPictureZoneProperties(const PropertySet& props) {
  const QMutexLocker locker(&m_mutex);
  m_defaultPictureZoneProps = props;
}

void Settings::setDefaultFillZoneProperties(const PropertySet& props) {
  const QMutexLocker locker(&m_mutex);
  m_defaultFillZoneProps = props;
}

PropertySet Settings::initialPictureZoneProps() {
  PropertySet props;
  props.locateOrCreate<PictureLayerProperty>()->setLayer(PictureLayerProperty::ZONEPAINTER2);
  return props;
}

PropertySet Settings::initialFillZoneProps() {
  PropertySet props;
  props.locateOrCreate<FillColorProperty>()->setColor(Qt::white);
  return props;
}

OutputProcessingParams Settings::getOutputProcessingParams(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageOutputProcessingParams.find(pageId));
  if (it != m_perPageOutputProcessingParams.end()) {
    return it->second;
  } else {
    return OutputProcessingParams();
  }
}

void Settings::setOutputProcessingParams(const PageId& pageId, const OutputProcessingParams& outputProcessingParams) {
  const QMutexLocker locker(&m_mutex);
  Utils::mapSetValue(m_perPageOutputProcessingParams, pageId, outputProcessingParams);
}

bool Settings::isParamsNull(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);
  return m_perPageParams.find(pageId) == m_perPageParams.end();
}

void Settings::setBlackOnWhite(const PageId& pageId, const bool blackOnWhite) {
  const QMutexLocker locker(&m_mutex);

  const auto it(m_perPageParams.find(pageId));
  if (it == m_perPageParams.end()) {
    Params params;
    params.setBlackOnWhite(blackOnWhite);
    m_perPageParams.insert(it, PerPageParams::value_type(pageId, params));
  } else {
    it->second.setBlackOnWhite(blackOnWhite);
  }
}

void Settings::setForceWhiteBalance(const PageId& pageId, bool force) {
  const QMutexLocker locker(&m_mutex);
  if (force) {
    // Default is ON, so remove from disabled set
    m_forceWhiteBalanceDisabled.erase(pageId);
  } else {
    // Explicitly disable for this page
    m_forceWhiteBalanceDisabled.insert(pageId);
  }
}

bool Settings::getForceWhiteBalance(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);
  // Default is ON - return false only if explicitly disabled
  return m_forceWhiteBalanceDisabled.count(pageId) == 0;
}

void Settings::setManualWhiteBalanceColor(const PageId& pageId, const QColor& color) {
  const QMutexLocker locker(&m_mutex);
  if (color.isValid()) {
    m_manualWhiteBalanceColors[pageId] = color;
  } else {
    m_manualWhiteBalanceColors.erase(pageId);
  }
}

QColor Settings::getManualWhiteBalanceColor(const PageId& pageId) const {
  const QMutexLocker locker(&m_mutex);
  const auto it = m_manualWhiteBalanceColors.find(pageId);
  if (it != m_manualWhiteBalanceColors.end()) {
    return it->second;
  }
  return QColor();  // Invalid color means no manual WB set
}

void Settings::clearManualWhiteBalanceColor(const PageId& pageId) {
  const QMutexLocker locker(&m_mutex);
  m_manualWhiteBalanceColors.erase(pageId);
}

Params Settings::detectColorMode(const PageId& pageId, const QString& sourceImagePath) {
  const QMutexLocker locker(&m_mutex);

  const ImageId& imageId = pageId.imageId();
  qDebug() << "detectColorMode: Force re-detecting color mode for:" << imageId.filePath()
           << "page:" << imageId.page();

  // Get existing params to preserve other settings, or create new ones
  Params params;
  const auto it(m_perPageParams.find(pageId));
  if (it != m_perPageParams.end()) {
    params = it->second;
  }

  // Detect color mode from source image
  ColorParams colorParams = params.colorParams();

  // Load the actual page image using ImageLoader (properly handles PDFs and multi-page files)
  QImage image = ImageLoader::load(imageId);
  if (image.isNull()) {
    qDebug() << "detectColorMode: Failed to load image, defaulting to COLOR_GRAYSCALE";
    colorParams.setColorMode(COLOR_GRAYSCALE);
    params.setColorParams(colorParams);
    Utils::mapSetValue(m_perPageParams, pageId, params);
    m_colorModeDetectedPages.insert(pageId);
    return params;
  }

  // Use Leptonica for document color type detection
  const LeptonicaDetector::ColorType colorType = LeptonicaDetector::detect(image);

  switch (colorType) {
    case LeptonicaDetector::ColorType::BlackWhite:
      colorParams.setColorMode(BLACK_AND_WHITE);
      qDebug() << "Leptonica detected B&W:" << imageId.filePath() << "page:" << imageId.page();
      break;
    case LeptonicaDetector::ColorType::Color:
      colorParams.setColorMode(COLOR_GRAYSCALE);
      qDebug() << "Leptonica detected Color:" << imageId.filePath() << "page:" << imageId.page();
      break;
    case LeptonicaDetector::ColorType::Grayscale:
    default:
      colorParams.setColorMode(COLOR_GRAYSCALE);
      qDebug() << "Leptonica detected Grayscale:" << imageId.filePath() << "page:" << imageId.page();
      break;
  }
  params.setColorParams(colorParams);

  // Store the detected params
  Utils::mapSetValue(m_perPageParams, pageId, params);

  // Mark this page as having had color mode detection done
  m_colorModeDetectedPages.insert(pageId);

  return params;
}

void Settings::batchDetectColorModes(const PageSequence& pages) {
  qDebug() << "batchDetectColorModes: Starting batch detection for" << pages.numPages() << "pages";

  int detected = 0;
  int skipped = 0;

  for (const PageInfo& pageInfo : pages) {
    const PageId& pageId = pageInfo.id();
    const QString sourceImagePath = pageId.imageId().filePath();

    // Check if we've already done Vision color detection for this page
    // (not just whether params exist - params may exist with default values from other settings)
    {
      const QMutexLocker locker(&m_mutex);
      if (m_colorModeDetectedPages.find(pageId) != m_colorModeDetectedPages.end()) {
        skipped++;
        continue;
      }
    }

    // Run Vision detection for this page
    detectColorMode(pageId, sourceImagePath);
    detected++;
  }

  qDebug() << "batchDetectColorModes: Completed - detected:" << detected << "skipped:" << skipped;
}
}  // namespace output