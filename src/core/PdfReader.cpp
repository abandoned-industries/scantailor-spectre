// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfReader.h"

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPdfDocument>
#include <QSizeF>

#include "Dpi.h"
#include "ImageMetadata.h"

// Global mutex to serialize PDF operations.
// QtPdf (Chromium-based PDF renderer) is not thread-safe and will crash
// if multiple threads attempt to load/render PDFs concurrently.
static QMutex s_pdfMutex;

bool PdfReader::checkMagic(const QByteArray& data) {
  // PDF files start with "%PDF-"
  return data.size() >= 5 && data.startsWith("%PDF-");
}

bool PdfReader::canRead(QIODevice& device) {
  if (!device.isOpen()) {
    if (!device.open(QIODevice::ReadOnly)) {
      return false;
    }
  }

  const qint64 origPos = device.pos();
  device.seek(0);
  const QByteArray header = device.read(5);
  device.seek(origPos);

  return checkMagic(header);
}

bool PdfReader::canRead(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray header = file.read(5);
  return checkMagic(header);
}

ImageMetadataLoader::Status PdfReader::readMetadata(QIODevice& device,
                                                    const VirtualFunction<void, const ImageMetadata&>& out) {
  // QPdfDocument requires a file path, not a QIODevice
  // So we need to get the file path from the device if possible
  auto* file = qobject_cast<QFile*>(&device);
  if (!file) {
    return ImageMetadataLoader::FORMAT_NOT_RECOGNIZED;
  }

  return readMetadata(file->fileName(), out);
}

ImageMetadataLoader::Status PdfReader::readMetadata(const QString& filePath,
                                                    const VirtualFunction<void, const ImageMetadata&>& out) {
  if (!canRead(filePath)) {
    return ImageMetadataLoader::FORMAT_NOT_RECOGNIZED;
  }

  QMutexLocker lock(&s_pdfMutex);

  QPdfDocument doc;
  if (doc.load(filePath) != QPdfDocument::Error::None) {
    qDebug() << "PdfReader: Failed to load PDF:" << filePath;
    return ImageMetadataLoader::GENERIC_ERROR;
  }

  const int pageCount = doc.pageCount();
  if (pageCount <= 0) {
    qDebug() << "PdfReader: PDF has no pages:" << filePath;
    return ImageMetadataLoader::NO_IMAGES;
  }

  // Report metadata for each page
  for (int i = 0; i < pageCount; ++i) {
    // Get page size in points (1/72 inch)
    const QSizeF pageSizePoints = doc.pagePointSize(i);

    // Calculate pixel dimensions at default render DPI
    const int widthPx = static_cast<int>(pageSizePoints.width() * DEFAULT_RENDER_DPI / 72.0 + 0.5);
    const int heightPx = static_cast<int>(pageSizePoints.height() * DEFAULT_RENDER_DPI / 72.0 + 0.5);

    const QSize size(widthPx, heightPx);
    const Dpi dpi(DEFAULT_RENDER_DPI, DEFAULT_RENDER_DPI);

    out(ImageMetadata(size, dpi));
  }

  return ImageMetadataLoader::LOADED;
}

QImage PdfReader::readImage(const QString& filePath, int pageNum, int dpi) {
  QMutexLocker lock(&s_pdfMutex);

  QPdfDocument doc;
  if (doc.load(filePath) != QPdfDocument::Error::None) {
    qDebug() << "PdfReader: Failed to load PDF for rendering:" << filePath;
    return QImage();
  }

  if (pageNum < 0 || pageNum >= doc.pageCount()) {
    qDebug() << "PdfReader: Invalid page number:" << pageNum << "for PDF with" << doc.pageCount() << "pages";
    return QImage();
  }

  // Get page size in points and calculate pixel size at requested DPI
  const QSizeF pageSizePoints = doc.pagePointSize(pageNum);
  const QSize renderSize(static_cast<int>(pageSizePoints.width() * dpi / 72.0 + 0.5),
                         static_cast<int>(pageSizePoints.height() * dpi / 72.0 + 0.5));

  // Render the page (returns ARGB32 with transparency)
  QImage pdfImage = doc.render(pageNum, renderSize);

  if (pdfImage.isNull()) {
    qDebug() << "PdfReader: Failed to render page" << pageNum << "of" << filePath;
    return QImage();
  }

  // Create a white background image and composite the PDF page onto it
  // This handles PDFs with transparent backgrounds correctly
  QImage image(renderSize, QImage::Format_RGB32);
  image.fill(Qt::white);

  QPainter painter(&image);
  painter.drawImage(0, 0, pdfImage);
  painter.end();

  // Set the DPI in the image metadata
  const int dotsPerMeter = static_cast<int>(dpi / 0.0254 + 0.5);
  image.setDotsPerMeterX(dotsPerMeter);
  image.setDotsPerMeterY(dotsPerMeter);

  return image;
}
