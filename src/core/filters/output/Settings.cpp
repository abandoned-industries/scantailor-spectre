// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <QDebug>
#include <QString>

#include "../../Utils.h"
#include "AbstractRelinker.h"
#include "AppleVisionDetector.h"
#include "FillColorProperty.h"
#include "ImageTypeDetector.h"
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
    return Params();
  }
}

Params Settings::getParamsOrDetect(const PageId& pageId, const QString& sourceImagePath) {
  const QMutexLocker locker(&m_mutex);

  qDebug() << "getParamsOrDetect called for:" << sourceImagePath;

  const auto it(m_perPageParams.find(pageId));
  if (it != m_perPageParams.end()) {
    qDebug() << "  -> Using existing params";
    return it->second;
  }

  qDebug() << "  -> No existing params, detecting...";
  qDebug() << "  -> Vision available:" << AppleVisionDetector::isAvailable();

  // No existing params - detect color mode from source image
  Params params;
  ColorParams colorParams;

  // Try Apple Vision first for intelligent content-aware detection
  if (AppleVisionDetector::isAvailable()) {
    const AppleVisionDetector::AnalysisResult analysis = AppleVisionDetector::analyzeFromFile(sourceImagePath);
    const QString suggestedMode = AppleVisionDetector::suggestColorMode(analysis);

    if (suggestedMode == QLatin1String("bw")) {
      colorParams.setColorMode(BLACK_AND_WHITE);
      qDebug() << "Vision detected B&W document:" << sourceImagePath
               << "content:" << static_cast<int>(analysis.contentType)
               << "text coverage:" << analysis.textCoverage;
    } else if (suggestedMode == QLatin1String("color")) {
      colorParams.setColorMode(COLOR_GRAYSCALE);  // Preserves color
      qDebug() << "Vision detected photo/color:" << sourceImagePath
               << "content:" << static_cast<int>(analysis.contentType);
    } else {
      colorParams.setColorMode(COLOR_GRAYSCALE);
      qDebug() << "Vision detected grayscale:" << sourceImagePath
               << "content:" << static_cast<int>(analysis.contentType)
               << "text coverage:" << analysis.textCoverage;
    }
  } else {
    // Fall back to basic pixel-based detection
    const ImageTypeDetector::Type imageType = ImageTypeDetector::detectFromFile(sourceImagePath);

    switch (imageType) {
      case ImageTypeDetector::Type::Mono:
        colorParams.setColorMode(BLACK_AND_WHITE);
        qDebug() << "Auto-detected B&W mode for:" << sourceImagePath;
        break;
      case ImageTypeDetector::Type::Grayscale:
        colorParams.setColorMode(COLOR_GRAYSCALE);
        qDebug() << "Auto-detected Grayscale mode for:" << sourceImagePath;
        break;
      case ImageTypeDetector::Type::Color:
        colorParams.setColorMode(COLOR_GRAYSCALE);
        qDebug() << "Auto-detected Color mode for:" << sourceImagePath;
        break;
    }
  }
  params.setColorParams(colorParams);

  // Store the detected params so they persist
  m_perPageParams.insert(PerPageParams::value_type(pageId, params));

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
}  // namespace output