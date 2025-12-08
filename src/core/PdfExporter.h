// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PDFEXPORTER_H_
#define SCANTAILOR_CORE_PDFEXPORTER_H_

#include <QString>
#include <QStringList>
#include <functional>

class PdfExporter {
 public:
  /**
   * Quality presets for PDF export.
   * Affects JPEG compression for color/mixed images (and grayscale if enabled).
   */
  enum class Quality {
    High,    // JPEG 95% - visually lossless, larger files
    Medium,  // JPEG 85% - good quality, balanced size
    Low      // JPEG 70% - smaller files, some visible compression
  };

  /**
   * Progress callback: (currentPage, totalPages) -> shouldContinue
   * Return false to cancel the export.
   */
  using ProgressCallback = std::function<bool(int, int)>;

  /**
   * \brief Combines multiple image files into a single PDF.
   *
   * \param imagePaths List of paths to image files (in order).
   * \param outputPdfPath Path where the PDF will be saved.
   * \param title Optional PDF title metadata.
   * \param quality Compression quality preset.
   * \param compressGrayscale If true, use JPEG for grayscale images too (smaller but lossy).
   * \param maxDpi Maximum resolution (0 = keep original). Images higher than this will be downsampled.
   * \param progressCallback Optional callback for progress updates.
   * \return true if successful, false otherwise.
   */
  static bool exportToPdf(const QStringList& imagePaths,
                          const QString& outputPdfPath,
                          const QString& title = QString(),
                          Quality quality = Quality::High,
                          bool compressGrayscale = false,
                          int maxDpi = 0,
                          ProgressCallback progressCallback = nullptr);
};

#endif  // SCANTAILOR_CORE_PDFEXPORTER_H_
