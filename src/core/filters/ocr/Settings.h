// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_SETTINGS_H_
#define SCANTAILOR_OCR_SETTINGS_H_

#include <QMutex>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>

#include "NonCopyable.h"
#include "OcrResult.h"
#include "PageId.h"

class AbstractRelinker;
class PageSequence;

namespace ocr {

/**
 * Get list of supported OCR language codes for Apple Vision.
 */
QStringList supportedLanguageCodes();

/**
 * Get display name for a language code.
 */
QString languageDisplayName(const QString& code);

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  Settings();
  ~Settings();

  void clear();

  void performRelinking(const AbstractRelinker& relinker);

  // === Global Settings ===

  bool ocrEnabled() const;
  void setOcrEnabled(bool enabled);

  // Language code (empty = auto-detect)
  QString language() const;
  void setLanguage(const QString& lang);

  bool useAccurateRecognition() const;
  void setUseAccurateRecognition(bool accurate);

  // === Per-Page Results ===

  void setOcrResult(const PageId& pageId, const OcrResult& result);
  std::unique_ptr<OcrResult> getOcrResult(const PageId& pageId) const;
  bool hasOcrResult(const PageId& pageId) const;
  void clearOcrResult(const PageId& pageId);
  void clearAllResults();

  // Check if all pages have been processed (for batch readiness)
  bool checkEverythingDefined(const PageSequence& pages, const PageId* ignore = nullptr) const;

 private:
  mutable QMutex m_mutex;

  // Global settings
  bool m_ocrEnabled = false;  // Disabled by default
  QString m_language;         // Empty = auto-detect
  bool m_useAccurateRecognition = true;

  // Per-page OCR results
  std::unordered_map<PageId, OcrResult> m_perPageResults;
};

}  // namespace ocr

#endif  // SCANTAILOR_OCR_SETTINGS_H_
