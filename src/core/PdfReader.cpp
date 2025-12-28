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

#include <limits>

#include "Dpi.h"
#include "ImageMetadata.h"

// Thread-safe PDF document manager.
// QtPdf (Chromium-based PDF renderer) is not thread-safe and will crash
// if multiple threads attempt to load/render PDFs concurrently.
// This class encapsulates the shared state with proper RAII semantics.
class PdfDocumentManager {
 public:
  static PdfDocumentManager& instance() {
    static PdfDocumentManager s_instance;
    return s_instance;
  }

  QMutex& mutex() { return m_mutex; }

  bool ensureLoaded(const QString& filePath) {
    // If already loaded, return success
    if (m_loadedFilePath == filePath && m_doc.status() == QPdfDocument::Status::Ready) {
      return true;
    }

    // Close any previously loaded document
    m_doc.close();
    m_loadedFilePath.clear();

    // Load the new document
    if (m_doc.load(filePath) != QPdfDocument::Error::None) {
      qDebug() << "PdfReader: Failed to load PDF:" << filePath;
      return false;
    }

    m_loadedFilePath = filePath;
    return true;
  }

  QPdfDocument& document() { return m_doc; }

 private:
  PdfDocumentManager() = default;
  ~PdfDocumentManager() = default;

  // Non-copyable, non-movable
  PdfDocumentManager(const PdfDocumentManager&) = delete;
  PdfDocumentManager& operator=(const PdfDocumentManager&) = delete;

  QMutex m_mutex;
  QPdfDocument m_doc;
  QString m_loadedFilePath;
};

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

  PdfDocumentManager& mgr = PdfDocumentManager::instance();
  QMutexLocker lock(&mgr.mutex());

  if (!mgr.ensureLoaded(filePath)) {
    return ImageMetadataLoader::GENERIC_ERROR;
  }

  QPdfDocument& doc = mgr.document();
  const int pageCount = doc.pageCount();
  if (pageCount <= 0) {
    qDebug() << "PdfReader: PDF has no pages:" << filePath;
    return ImageMetadataLoader::NO_IMAGES;
  }

  // Report metadata for each page
  constexpr double maxDimension = static_cast<double>(std::numeric_limits<int>::max());

  for (int i = 0; i < pageCount; ++i) {
    // Get page size in points (1/72 inch)
    const QSizeF pageSizePoints = doc.pagePointSize(i);

    // Calculate pixel dimensions at default render DPI with overflow protection
    const double calcWidth = pageSizePoints.width() * DEFAULT_RENDER_DPI / 72.0 + 0.5;
    const double calcHeight = pageSizePoints.height() * DEFAULT_RENDER_DPI / 72.0 + 0.5;

    if (calcWidth <= 0 || calcHeight <= 0 || calcWidth > maxDimension || calcHeight > maxDimension) {
      qWarning() << "PdfReader: Invalid page dimensions for page" << i << "- skipping";
      continue;
    }

    const int widthPx = static_cast<int>(calcWidth);
    const int heightPx = static_cast<int>(calcHeight);

    const QSize size(widthPx, heightPx);
    const Dpi dpi(DEFAULT_RENDER_DPI, DEFAULT_RENDER_DPI);

    out(ImageMetadata(size, dpi));
  }

  return ImageMetadataLoader::LOADED;
}

QImage PdfReader::readImage(const QString& filePath, int pageNum, int dpi) {
  PdfDocumentManager& mgr = PdfDocumentManager::instance();
  QMutexLocker lock(&mgr.mutex());

  if (!mgr.ensureLoaded(filePath)) {
    qDebug() << "PdfReader: Failed to load PDF for rendering:" << filePath;
    return QImage();
  }

  QPdfDocument& doc = mgr.document();

  if (pageNum < 0 || pageNum >= doc.pageCount()) {
    qDebug() << "PdfReader: Invalid page number:" << pageNum << "for PDF with" << doc.pageCount() << "pages";
    return QImage();
  }

  // Get page size in points and calculate pixel size at requested DPI with overflow protection
  const QSizeF pageSizePoints = doc.pagePointSize(pageNum);
  constexpr double maxDim = static_cast<double>(std::numeric_limits<int>::max());
  const double calcWidth = pageSizePoints.width() * dpi / 72.0 + 0.5;
  const double calcHeight = pageSizePoints.height() * dpi / 72.0 + 0.5;

  if (calcWidth <= 0 || calcHeight <= 0 || calcWidth > maxDim || calcHeight > maxDim) {
    qWarning() << "PdfReader: Invalid render dimensions for page" << pageNum;
    return QImage();
  }

  const QSize renderSize(static_cast<int>(calcWidth), static_cast<int>(calcHeight));

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
