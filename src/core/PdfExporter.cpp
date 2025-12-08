// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfExporter.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#ifdef HAVE_LIBHARU
#include <hpdf.h>
#include <cstring>
#endif

namespace {

// JPEG quality for color/mixed images (95 = visually lossless)
constexpr int JPEG_QUALITY = 95;

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

enum class ImageType { Mono, Grayscale, Color };

ImageType detectImageType(const QImage& image) {
  const QImage::Format fmt = image.format();

  // Check format first
  if (fmt == QImage::Format_Mono || fmt == QImage::Format_MonoLSB) {
    return ImageType::Mono;
  }
  if (fmt == QImage::Format_Grayscale8 || fmt == QImage::Format_Grayscale16) {
    return ImageType::Grayscale;
  }

  // Analyze content for other formats
  if (isEffectivelyMono(image)) {
    return ImageType::Mono;
  }
  if (isEffectivelyGrayscale(image)) {
    return ImageType::Grayscale;
  }
  return ImageType::Color;
}

#ifdef HAVE_LIBHARU

void haruErrorHandler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* user_data) {
  Q_UNUSED(user_data);
  qWarning() << "LibHaru error:" << Qt::hex << error_no << "detail:" << detail_no;
}

/**
 * Export using libharu with optimized compression:
 * - B&W: CCITT Group 4 (extremely efficient for 1-bit images)
 * - Grayscale: Flate compression (lossless)
 * - Color: JPEG at high quality (visually lossless)
 */
bool exportWithLibHaru(const QStringList& imagePaths, const QString& outputPdfPath, const QString& title) {
  HPDF_Doc pdf = HPDF_New(haruErrorHandler, nullptr);
  if (!pdf) {
    qDebug() << "PdfExporter: Failed to create PDF document";
    return false;
  }

  // Enable compression
  HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

  // Set document info
  if (!title.isEmpty()) {
    HPDF_SetInfoAttr(pdf, HPDF_INFO_TITLE, title.toUtf8().constData());
  }
  HPDF_SetInfoAttr(pdf, HPDF_INFO_CREATOR, "ScanTailor Advanced");

  int pagesWritten = 0;

  for (const QString& imagePath : imagePaths) {
    QImage image(imagePath);
    if (image.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    // Get image DPI
    int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;

    // Calculate page size in points (1/72 inch)
    float pageWidth = image.width() * 72.0f / dpiX;
    float pageHeight = image.height() * 72.0f / dpiY;

    // Create page
    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetWidth(page, pageWidth);
    HPDF_Page_SetHeight(page, pageHeight);

    HPDF_Image pdfImage = nullptr;
    ImageType type = detectImageType(image);

    if (type == ImageType::Mono) {
      // Convert to 1-bit and use CCITT Group 4 compression
      qDebug() << "PdfExporter: Using CCITT G4 for B&W image:" << imagePath;
      QImage monoImage = image.convertToFormat(QImage::Format_Mono);

      // libharu expects raw 1-bit data, MSB first
      // Create raw bitmap data
      int bytesPerLine = (monoImage.width() + 7) / 8;
      QByteArray rawData;
      rawData.reserve(bytesPerLine * monoImage.height());

      for (int y = 0; y < monoImage.height(); ++y) {
        const uchar* line = monoImage.constScanLine(y);
        rawData.append(reinterpret_cast<const char*>(line), bytesPerLine);
      }

      pdfImage = HPDF_LoadRawImageFromMem(
          pdf,
          reinterpret_cast<const HPDF_BYTE*>(rawData.constData()),
          monoImage.width(),
          monoImage.height(),
          HPDF_CS_DEVICE_GRAY,
          1);  // 1 bit per component

      if (pdfImage) {
        HPDF_Image_SetFilter(pdfImage, HPDF_STREAM_FILTER_CCITT_DECODE);
      }
    } else if (type == ImageType::Grayscale) {
      // Use Flate compression for grayscale (lossless)
      qDebug() << "PdfExporter: Using Flate for grayscale image:" << imagePath;
      QImage grayImage = image.convertToFormat(QImage::Format_Grayscale8);

      QByteArray rawData;
      rawData.reserve(grayImage.width() * grayImage.height());

      for (int y = 0; y < grayImage.height(); ++y) {
        const uchar* line = grayImage.constScanLine(y);
        rawData.append(reinterpret_cast<const char*>(line), grayImage.width());
      }

      pdfImage = HPDF_LoadRawImageFromMem(
          pdf,
          reinterpret_cast<const HPDF_BYTE*>(rawData.constData()),
          grayImage.width(),
          grayImage.height(),
          HPDF_CS_DEVICE_GRAY,
          8);  // 8 bits per component

      if (pdfImage) {
        HPDF_Image_SetFilter(pdfImage, HPDF_STREAM_FILTER_FLATE_DECODE);
      }
    } else {
      // Use JPEG for color images (high quality, visually lossless)
      qDebug() << "PdfExporter: Using JPEG Q" << JPEG_QUALITY << "for color image:" << imagePath;

      // Convert to RGB and save as JPEG to memory
      QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
      QByteArray jpegData;
      QBuffer buffer(&jpegData);
      buffer.open(QIODevice::WriteOnly);
      rgbImage.save(&buffer, "JPEG", JPEG_QUALITY);
      buffer.close();

      pdfImage = HPDF_LoadJpegImageFromMem(
          pdf,
          reinterpret_cast<const HPDF_BYTE*>(jpegData.constData()),
          jpegData.size());
    }

    if (!pdfImage) {
      qWarning() << "PdfExporter: Failed to create PDF image for:" << imagePath;
      continue;
    }

    // Draw image on page (fill entire page)
    HPDF_Page_DrawImage(page, pdfImage, 0, 0, pageWidth, pageHeight);
    ++pagesWritten;
  }

  if (pagesWritten == 0) {
    HPDF_Free(pdf);
    return false;
  }

  // Save to file
  HPDF_STATUS status = HPDF_SaveToFile(pdf, outputPdfPath.toUtf8().constData());
  HPDF_Free(pdf);

  if (status != HPDF_OK) {
    qDebug() << "PdfExporter: Failed to save PDF file";
    return false;
  }

  qDebug() << "PdfExporter: Exported" << pagesWritten << "pages with optimized compression";
  return true;
}

#endif  // HAVE_LIBHARU

/**
 * Fallback Qt-based PDF export (larger files, but no extra dependencies)
 */
bool exportWithQt(const QStringList& imagePaths, const QString& outputPdfPath, const QString& title) {
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
    QImage image(imagePath);
    if (image.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    // Get the image DPI (default to 300 if not set)
    int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;

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

  return !firstPage;
}

}  // namespace

bool PdfExporter::exportToPdf(const QStringList& imagePaths, const QString& outputPdfPath, const QString& title) {
  if (imagePaths.isEmpty()) {
    qDebug() << "PdfExporter: No images to export";
    return false;
  }

#ifdef HAVE_LIBHARU
  return exportWithLibHaru(imagePaths, outputPdfPath, title);
#else
  qDebug() << "PdfExporter: Using Qt fallback (libharu not available)";
  return exportWithQt(imagePaths, outputPdfPath, title);
#endif
}
