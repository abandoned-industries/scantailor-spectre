// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_SETTINGS_H_
#define SCANTAILOR_FINALIZE_SETTINGS_H_

#include <QDomElement>
#include <QMutex>
#include <unordered_map>
#include <memory>

#include "NonCopyable.h"
#include "PageId.h"

class AbstractRelinker;
class PageSequence;

namespace finalize {

// Color mode for a page
enum class ColorMode {
  BlackAndWhite,
  Grayscale,
  Color
};

// Parameters stored per page
class Params {
 public:
  Params();
  explicit Params(const QDomElement& el);

  QDomElement toXml(QDomDocument& doc, const QString& name) const;

  ColorMode colorMode() const { return m_colorMode; }
  void setColorMode(ColorMode mode) { m_colorMode = mode; }

  bool isColorModeDetected() const { return m_colorModeDetected; }
  void setColorModeDetected(bool detected) { m_colorModeDetected = detected; }

  bool isProcessed() const { return m_processed; }
  void setProcessed(bool processed) { m_processed = processed; }

 private:
  ColorMode m_colorMode;
  bool m_colorModeDetected;
  bool m_processed;
};

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  Settings();
  ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  void setParams(const PageId& pageId, const Params& params);
  std::unique_ptr<Params> getParams(const PageId& pageId) const;

  void setColorMode(const PageId& pageId, ColorMode mode);
  ColorMode getColorMode(const PageId& pageId) const;

  void setProcessed(const PageId& pageId, bool processed);
  bool isProcessed(const PageId& pageId) const;

  void clearDetectionCache();
  void clearDetectionCacheForPage(const PageId& pageId);

  bool checkEverythingDefined(const PageSequence& pages, const PageId* ignore = nullptr) const;

  // Global detection threshold (percentage, e.g., 8 means 8%)
  int midtoneThreshold() const { return m_midtoneThreshold; }
  void setMidtoneThreshold(int threshold) { m_midtoneThreshold = threshold; }

 private:
  using PerPageParams = std::unordered_map<PageId, Params>;

  mutable QMutex m_mutex;
  PerPageParams m_perPageParams;
  int m_midtoneThreshold = 8;  // Default 8%
};

}  // namespace finalize
#endif  // SCANTAILOR_FINALIZE_SETTINGS_H_
