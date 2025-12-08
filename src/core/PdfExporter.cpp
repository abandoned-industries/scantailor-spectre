// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfExporter.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace {

/**
 * Sample pixels to determine if image is pure black and white.
 * Uses sampling for efficiency on large images.
 */
bool isEffectivelyMono(const QImage& image) {
  const int totalPixels = image.width() * image.height();
  const int sampleSize = qMin(10000, totalPixels);
  const int step = qMax(1, totalPixels / sampleSize);

  for (int i = 0; i < totalPixels; i += step) {
    const int x = i % image.width();
    const int y = i / image.width();
    const QRgb pixel = image.pixel(x, y);
    const int gray = qGray(pixel);
    // Check if pixel is pure black or white
    if (gray != 0 && gray != 255) {
      return false;
    }
    // Also verify it's actually grayscale (R=G=B)
    if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel)) {
      return false;
    }
  }
  return true;
}

/**
 * Sample pixels to determine if image is grayscale (R=G=B for all pixels).
 */
bool isEffectivelyGrayscale(const QImage& image) {
  const int totalPixels = image.width() * image.height();
  const int sampleSize = qMin(10000, totalPixels);
  const int step = qMax(1, totalPixels / sampleSize);

  for (int i = 0; i < totalPixels; i += step) {
    const int x = i % image.width();
    const int y = i / image.width();
    const QRgb pixel = image.pixel(x, y);
    if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel)) {
      return false;
    }
  }
  return true;
}

/**
 * Convert image to the most efficient format for PDF embedding.
 * - B&W images -> 1-bit mono (24x smaller than RGB)
 * - Grayscale images -> 8-bit grayscale (3x smaller than RGB)
 * - Color images -> RGB without alpha
 */
QImage optimizeForPdf(const QImage& image) {
  const QImage::Format fmt = image.format();

  // Already in an optimal format
  if (fmt == QImage::Format_Mono || fmt == QImage::Format_MonoLSB) {
    return image;
  }
  if (fmt == QImage::Format_Grayscale8 || fmt == QImage::Format_Grayscale16) {
    return image;
  }
  if (fmt == QImage::Format_Indexed8) {
    // Check if indexed image is effectively mono or grayscale
    // For now, keep as-is since it's already compact
    return image;
  }

  // For other formats, analyze content and convert to optimal format
  if (isEffectivelyMono(image)) {
    qDebug() << "PdfExporter: Converting to 1-bit mono for efficient storage";
    return image.convertToFormat(QImage::Format_Mono);
  }

  if (isEffectivelyGrayscale(image)) {
    qDebug() << "PdfExporter: Converting to 8-bit grayscale for efficient storage";
    return image.convertToFormat(QImage::Format_Grayscale8);
  }

  // Strip alpha channel if present (saves space and PDF doesn't need it for scans)
  if (image.hasAlphaChannel()) {
    return image.convertToFormat(QImage::Format_RGB888);
  }

  return image;
}

}  // namespace

bool PdfExporter::exportToPdf(const QStringList& imagePaths, const QString& outputPdfPath, const QString& title) {
  if (imagePaths.isEmpty()) {
    qDebug() << "PdfExporter: No images to export";
    return false;
  }

  QFile file(outputPdfPath);
  if (!file.open(QIODevice::WriteOnly)) {
    qDebug() << "PdfExporter: Failed to open output file:" << outputPdfPath;
    return false;
  }

  QPdfWriter pdfWriter(&file);
  pdfWriter.setCreator("ScanTailor Advanced");
  if (!title.isEmpty()) {
    pdfWriter.setTitle(title);
  }

  QPainter painter;
  bool firstPage = true;

  for (const QString& imagePath : imagePaths) {
    QImage originalImage(imagePath);
    if (originalImage.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    // Optimize image format for efficient PDF storage
    QImage image = optimizeForPdf(originalImage);

    // Get the image DPI (default to 300 if not set) - use original for accurate DPI
    int dpiX = originalImage.dotsPerMeterX() > 0 ? qRound(originalImage.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = originalImage.dotsPerMeterY() > 0 ? qRound(originalImage.dotsPerMeterY() * 0.0254) : 300;

    // Calculate page size in points (1/72 inch) from image dimensions
    const qreal pageWidthPoints = image.width() * 72.0 / dpiX;
    const qreal pageHeightPoints = image.height() * 72.0 / dpiY;

    // Set page size to match image dimensions
    QPageSize pageSize(QSizeF(pageWidthPoints, pageHeightPoints), QPageSize::Point);
    pdfWriter.setPageSize(pageSize);
    pdfWriter.setPageMargins(QMarginsF(0, 0, 0, 0));
    pdfWriter.setResolution(dpiX);

    if (firstPage) {
      if (!painter.begin(&pdfWriter)) {
        qDebug() << "PdfExporter: Failed to begin painting";
        return false;
      }
      firstPage = false;
    } else {
      pdfWriter.newPage();
    }

    // Draw the image to fill the page
    const QRect targetRect(0, 0, pdfWriter.width(), pdfWriter.height());
    painter.drawImage(targetRect, image);
  }

  if (!firstPage) {
    painter.end();
  }

  return !firstPage;  // Return true if at least one page was written
}
