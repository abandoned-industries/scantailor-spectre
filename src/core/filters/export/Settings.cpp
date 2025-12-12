// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Settings.h"

#include <QMutexLocker>

namespace export_ {

Settings::Settings()
    : m_noDpiLimit(true), m_maxDpi(400), m_compressGrayscale(false), m_quality(PdfExporter::Quality::Medium) {}

bool Settings::noDpiLimit() const {
  QMutexLocker locker(&m_mutex);
  return m_noDpiLimit;
}

void Settings::setNoDpiLimit(bool value) {
  QMutexLocker locker(&m_mutex);
  m_noDpiLimit = value;
}

int Settings::maxDpi() const {
  QMutexLocker locker(&m_mutex);
  return m_maxDpi;
}

void Settings::setMaxDpi(int dpi) {
  QMutexLocker locker(&m_mutex);
  m_maxDpi = dpi;
}

bool Settings::compressGrayscale() const {
  QMutexLocker locker(&m_mutex);
  return m_compressGrayscale;
}

void Settings::setCompressGrayscale(bool value) {
  QMutexLocker locker(&m_mutex);
  m_compressGrayscale = value;
}

PdfExporter::Quality Settings::quality() const {
  QMutexLocker locker(&m_mutex);
  return m_quality;
}

void Settings::setQuality(PdfExporter::Quality quality) {
  QMutexLocker locker(&m_mutex);
  m_quality = quality;
}

}  // namespace export_
