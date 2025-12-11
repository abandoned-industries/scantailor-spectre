// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_SETTINGS_H_
#define SCANTAILOR_EXPORT_SETTINGS_H_

#include <QMutex>

#include "NonCopyable.h"
#include "PdfExporter.h"

namespace export_ {

class Settings {
  DECLARE_NON_COPYABLE(Settings)

 public:
  Settings();

  ~Settings() = default;

  // No DPI limit - use output resolution as-is
  bool noDpiLimit() const;
  void setNoDpiLimit(bool value);

  // Max DPI when limit is enabled (default 400)
  int maxDpi() const;
  void setMaxDpi(int dpi);

  // Compress grayscale images with JPEG (lossy but smaller)
  bool compressGrayscale() const;
  void setCompressGrayscale(bool value);

  // JPEG quality preset
  PdfExporter::Quality quality() const;
  void setQuality(PdfExporter::Quality quality);

 private:
  mutable QMutex m_mutex;
  bool m_noDpiLimit;
  int m_maxDpi;
  bool m_compressGrayscale;
  PdfExporter::Quality m_quality;
};

}  // namespace export_

#endif  // SCANTAILOR_EXPORT_SETTINGS_H_
