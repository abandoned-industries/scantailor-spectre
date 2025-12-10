// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OUTPUT_SETTINGS_H_
#define SCANTAILOR_OUTPUT_SETTINGS_H_

#include <DistortionModel.h>

#include <QMutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "ColorParams.h"
#include "DespeckleLevel.h"
#include "DewarpingOptions.h"
#include "Dpi.h"
#include "NonCopyable.h"
#include "OutputParams.h"
#include "OutputProcessingParams.h"
#include "PageId.h"
#include "Params.h"
#include "PropertySet.h"
#include "ZoneSet.h"

class AbstractRelinker;
class PageSequence;
class QString;

namespace output {
class Params;

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  Settings();

  virtual ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  Params getParams(const PageId& pageId) const;

  /**
   * Get params for a page, auto-detecting color mode if no params exist.
   * When params don't exist for a page, this method analyzes the source image
   * to detect if it's B&W, grayscale, or color, and sets an appropriate default
   * color mode instead of always defaulting to BLACK_AND_WHITE.
   *
   * @param pageId The page to get params for
   * @param sourceImagePath Path to the original source image for color detection
   * @return Params with auto-detected color mode if new, or existing params
   */
  Params getParamsOrDetect(const PageId& pageId, const QString& sourceImagePath);

  void setParams(const PageId& pageId, const Params& params);

  bool isParamsNull(const PageId& pageId) const;

  void setColorParams(const PageId& pageId, const ColorParams& prms);

  void setPictureShapeOptions(const PageId& pageId, PictureShapeOptions pictureShapeOptions);

  void setDpi(const PageId& pageId, const Dpi& dpi);

  void setDewarpingOptions(const PageId& pageId, const DewarpingOptions& opt);

  void setSplittingOptions(const PageId& pageId, const SplittingOptions& opt);

  void setDistortionModel(const PageId& pageId, const dewarping::DistortionModel& model);

  void setDepthPerception(const PageId& pageId, const DepthPerception& depthPerception);

  void setDespeckleLevel(const PageId& pageId, double level);

  std::unique_ptr<OutputParams> getOutputParams(const PageId& pageId) const;

  void removeOutputParams(const PageId& pageId);

  void setOutputParams(const PageId& pageId, const OutputParams& params);

  ZoneSet pictureZonesForPage(const PageId& pageId) const;

  ZoneSet fillZonesForPage(const PageId& pageId) const;

  void setPictureZones(const PageId& pageId, const ZoneSet& zones);

  void setFillZones(const PageId& pageId, const ZoneSet& zones);

  /**
   * For now, default zone properties are not persistent.
   * They may become persistent later though.
   */
  PropertySet defaultPictureZoneProperties() const;

  PropertySet defaultFillZoneProperties() const;

  void setDefaultPictureZoneProperties(const PropertySet& props);

  void setDefaultFillZoneProperties(const PropertySet& props);

  OutputProcessingParams getOutputProcessingParams(const PageId& pageId) const;

  void setOutputProcessingParams(const PageId& pageId, const OutputProcessingParams& outputProcessingParams);

  void setBlackOnWhite(const PageId& pageId, bool blackOnWhite);

  /**
   * Force re-detection of color mode for a page using Apple Vision or fallback detector.
   * Clears any existing params and runs detection fresh.
   *
   * @param pageId The page to detect color mode for
   * @param sourceImagePath Path to the original source image
   * @return The detected Params with appropriate color mode
   */
  Params detectColorMode(const PageId& pageId, const QString& sourceImagePath);

  /**
   * Batch detect color modes for all pages that don't have params yet.
   * Called when entering the Output filter to pre-populate color mode detection
   * for all pages, eliminating the need to detect on first view of each page.
   *
   * @param pages The PageSequence containing all pages to process
   */
  void batchDetectColorModes(const PageSequence& pages);

 private:
  using PerPageParams = std::unordered_map<PageId, Params>;
  using PerPageOutputParams = std::unordered_map<PageId, OutputParams>;
  using PerPageZones = std::unordered_map<PageId, ZoneSet>;
  using PerPageOutputProcessingParams = std::unordered_map<PageId, OutputProcessingParams>;

  static PropertySet initialPictureZoneProps();

  static PropertySet initialFillZoneProps();

  mutable QMutex m_mutex;
  PerPageParams m_perPageParams;
  PerPageOutputParams m_perPageOutputParams;
  PerPageZones m_perPagePictureZones;
  PerPageZones m_perPageFillZones;
  PropertySet m_defaultPictureZoneProps;
  PropertySet m_defaultFillZoneProps;
  PerPageOutputProcessingParams m_perPageOutputProcessingParams;
  std::unordered_set<PageId> m_colorModeDetectedPages;  // Track pages that have had Vision color detection
};
}  // namespace output
#endif  // ifndef SCANTAILOR_OUTPUT_SETTINGS_H_
