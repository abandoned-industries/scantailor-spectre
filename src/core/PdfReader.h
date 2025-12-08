// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PDFREADER_H_
#define SCANTAILOR_CORE_PDFREADER_H_

#include "ImageMetadataLoader.h"
#include "VirtualFunction.h"

class QByteArray;
class QIODevice;
class QImage;
class ImageMetadata;
class QString;

class PdfReader {
 public:
  // Default DPI for rendering PDF pages (300 is standard scanning resolution)
  static constexpr int DEFAULT_RENDER_DPI = 300;

  static bool canRead(QIODevice& device);

  static bool canRead(const QString& filePath);

  static ImageMetadataLoader::Status readMetadata(QIODevice& device,
                                                  const VirtualFunction<void, const ImageMetadata&>& out);

  static ImageMetadataLoader::Status readMetadata(const QString& filePath,
                                                  const VirtualFunction<void, const ImageMetadata&>& out);

  /**
   * \brief Renders a PDF page to a QImage.
   *
   * \param filePath The path to the PDF file.
   * \param pageNum A zero-based page number within the PDF.
   * \param dpi The resolution to render at (default 300 DPI).
   * \return The resulting image, or a null image in case of failure.
   */
  static QImage readImage(const QString& filePath, int pageNum = 0, int dpi = DEFAULT_RENDER_DPI);

 private:
  static bool checkMagic(const QByteArray& data);
};

#endif  // SCANTAILOR_CORE_PDFREADER_H_
