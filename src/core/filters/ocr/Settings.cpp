// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <QCoreApplication>
#include <QMutexLocker>

#include "AbstractRelinker.h"
#include "PageSequence.h"
#include "RelinkablePath.h"

namespace ocr {

QStringList supportedLanguageCodes() {
  // Apple Vision supported languages (as of macOS 13+)
  // Empty string represents auto-detect
  return QStringList() << ""  // Auto-detect
                       << "en-US"
                       << "fr-FR"
                       << "de-DE"
                       << "it-IT"
                       << "pt-BR"
                       << "es-ES"
                       << "zh-Hans"
                       << "zh-Hant"
                       << "ja-JP"
                       << "ko-KR"
                       << "ru-RU"
                       << "uk-UA"
                       << "pl-PL"
                       << "cs-CZ"
                       << "nl-NL"
                       << "da-DK"
                       << "fi-FI"
                       << "nb-NO"
                       << "sv-SE"
                       << "tr-TR"
                       << "el-GR"
                       << "hu-HU"
                       << "ro-RO"
                       << "th-TH"
                       << "vi-VN";
}

QString languageDisplayName(const QString& code) {
  if (code.isEmpty()) {
    return QCoreApplication::translate("ocr::Settings", "Auto-detect");
  }
  if (code == "en-US") {
    return QCoreApplication::translate("ocr::Settings", "English");
  }
  if (code == "fr-FR") {
    return QCoreApplication::translate("ocr::Settings", "French");
  }
  if (code == "de-DE") {
    return QCoreApplication::translate("ocr::Settings", "German");
  }
  if (code == "it-IT") {
    return QCoreApplication::translate("ocr::Settings", "Italian");
  }
  if (code == "pt-BR") {
    return QCoreApplication::translate("ocr::Settings", "Portuguese");
  }
  if (code == "es-ES") {
    return QCoreApplication::translate("ocr::Settings", "Spanish");
  }
  if (code == "zh-Hans") {
    return QCoreApplication::translate("ocr::Settings", "Chinese (Simplified)");
  }
  if (code == "zh-Hant") {
    return QCoreApplication::translate("ocr::Settings", "Chinese (Traditional)");
  }
  if (code == "ja-JP") {
    return QCoreApplication::translate("ocr::Settings", "Japanese");
  }
  if (code == "ko-KR") {
    return QCoreApplication::translate("ocr::Settings", "Korean");
  }
  if (code == "ru-RU") {
    return QCoreApplication::translate("ocr::Settings", "Russian");
  }
  if (code == "uk-UA") {
    return QCoreApplication::translate("ocr::Settings", "Ukrainian");
  }
  if (code == "pl-PL") {
    return QCoreApplication::translate("ocr::Settings", "Polish");
  }
  if (code == "cs-CZ") {
    return QCoreApplication::translate("ocr::Settings", "Czech");
  }
  if (code == "nl-NL") {
    return QCoreApplication::translate("ocr::Settings", "Dutch");
  }
  if (code == "da-DK") {
    return QCoreApplication::translate("ocr::Settings", "Danish");
  }
  if (code == "fi-FI") {
    return QCoreApplication::translate("ocr::Settings", "Finnish");
  }
  if (code == "nb-NO") {
    return QCoreApplication::translate("ocr::Settings", "Norwegian");
  }
  if (code == "sv-SE") {
    return QCoreApplication::translate("ocr::Settings", "Swedish");
  }
  if (code == "tr-TR") {
    return QCoreApplication::translate("ocr::Settings", "Turkish");
  }
  if (code == "el-GR") {
    return QCoreApplication::translate("ocr::Settings", "Greek");
  }
  if (code == "hu-HU") {
    return QCoreApplication::translate("ocr::Settings", "Hungarian");
  }
  if (code == "ro-RO") {
    return QCoreApplication::translate("ocr::Settings", "Romanian");
  }
  if (code == "th-TH") {
    return QCoreApplication::translate("ocr::Settings", "Thai");
  }
  if (code == "vi-VN") {
    return QCoreApplication::translate("ocr::Settings", "Vietnamese");
  }
  // Fallback: return the code itself
  return code;
}

Settings::Settings() = default;

Settings::~Settings() = default;

void Settings::clear() {
  QMutexLocker locker(&m_mutex);
  m_perPageResults.clear();
}

void Settings::performRelinking(const AbstractRelinker& relinker) {
  QMutexLocker locker(&m_mutex);
  std::unordered_map<PageId, OcrResult> newResults;
  for (auto& kv : m_perPageResults) {
    const RelinkablePath oldPath(kv.first.imageId().filePath(), RelinkablePath::File);
    PageId newPageId(kv.first);
    newPageId.imageId().setFilePath(relinker.substitutionPathFor(oldPath));
    newResults[newPageId] = std::move(kv.second);
  }
  m_perPageResults = std::move(newResults);
}

bool Settings::ocrEnabled() const {
  QMutexLocker locker(&m_mutex);
  return m_ocrEnabled;
}

void Settings::setOcrEnabled(bool enabled) {
  QMutexLocker locker(&m_mutex);
  m_ocrEnabled = enabled;
}

QString Settings::language() const {
  QMutexLocker locker(&m_mutex);
  return m_language;
}

void Settings::setLanguage(const QString& lang) {
  QMutexLocker locker(&m_mutex);
  m_language = lang;
}

bool Settings::useAccurateRecognition() const {
  QMutexLocker locker(&m_mutex);
  return m_useAccurateRecognition;
}

void Settings::setUseAccurateRecognition(bool accurate) {
  QMutexLocker locker(&m_mutex);
  m_useAccurateRecognition = accurate;
}

void Settings::setOcrResult(const PageId& pageId, const OcrResult& result) {
  QMutexLocker locker(&m_mutex);
  m_perPageResults[pageId] = result;
}

std::unique_ptr<OcrResult> Settings::getOcrResult(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);
  auto it = m_perPageResults.find(pageId);
  if (it == m_perPageResults.end()) {
    return nullptr;
  }
  return std::make_unique<OcrResult>(it->second);
}

bool Settings::hasOcrResult(const PageId& pageId) const {
  QMutexLocker locker(&m_mutex);
  return m_perPageResults.find(pageId) != m_perPageResults.end();
}

void Settings::clearOcrResult(const PageId& pageId) {
  QMutexLocker locker(&m_mutex);
  m_perPageResults.erase(pageId);
}

void Settings::clearAllResults() {
  QMutexLocker locker(&m_mutex);
  m_perPageResults.clear();
}

bool Settings::checkEverythingDefined(const PageSequence& pages, const PageId* ignore) const {
  QMutexLocker locker(&m_mutex);

  if (!m_ocrEnabled) {
    // If OCR is disabled, we consider everything "defined"
    return true;
  }

  const size_t numPages = pages.numPages();
  for (size_t i = 0; i < numPages; ++i) {
    const PageId& pageId = pages.pageAt(i).id();
    if (ignore && *ignore == pageId) {
      continue;
    }
    if (m_perPageResults.find(pageId) == m_perPageResults.end()) {
      return false;
    }
  }
  return true;
}

}  // namespace ocr
