// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PDFEXPORTER_H_
#define SCANTAILOR_CORE_PDFEXPORTER_H_

#include <QString>
#include <QStringList>

class PdfExporter {
 public:
  /**
   * \brief Combines multiple image files into a single PDF.
   *
   * \param imagePaths List of paths to image files (in order).
   * \param outputPdfPath Path where the PDF will be saved.
   * \param title Optional PDF title metadata.
   * \return true if successful, false otherwise.
   */
  static bool exportToPdf(const QStringList& imagePaths, const QString& outputPdfPath, const QString& title = QString());
};

#endif  // SCANTAILOR_CORE_PDFEXPORTER_H_
