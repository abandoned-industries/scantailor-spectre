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

// Convert quality enum to JPEG quality percentage
int qualityToJpegPercent(PdfExporter::Quality quality) {
  switch (quality) {
    case PdfExporter::Quality::High:
      return 95;
    case PdfExporter::Quality::Medium:
      return 85;
    case PdfExporter::Quality::Low:
      return 70;
  }
  return 95;
}

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
    if (gray != 0 && gray != 255) {
      return false;
    }
    if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel)) {
      return false;
    }
  }
  return true;
}

/**
 * Sample pixels to determine if image is grayscale.
 * Tolerates slight color variations (e.g., yellowed paper) up to threshold.
 */
bool isEffectivelyGrayscale(const QImage& image, int tolerance = 8) {
  const int totalPixels = image.width() * image.height();
  const int sampleSize = qMin(10000, totalPixels);
  const int step = qMax(1, totalPixels / sampleSize);

  for (int i = 0; i < totalPixels; i += step) {
    const int x = i % image.width();
    const int y = i / image.width();
    const QRgb pixel = image.pixel(x, y);
    const int r = qRed(pixel);
    const int g = qGreen(pixel);
    const int b = qBlue(pixel);
    // Check if RGB values are within tolerance of each other
    const int maxDiff = qMax(qMax(qAbs(r - g), qAbs(g - b)), qAbs(r - b));
    if (maxDiff > tolerance) {
      return false;
    }
  }
  return true;
}

enum class ImageType { Mono, Grayscale, Color };

ImageType detectImageType(const QImage& image) {
  const QImage::Format fmt = image.format();

  if (fmt == QImage::Format_Mono || fmt == QImage::Format_MonoLSB) {
    return ImageType::Mono;
  }
  if (fmt == QImage::Format_Grayscale8 || fmt == QImage::Format_Grayscale16) {
    return ImageType::Grayscale;
  }

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

bool exportWithLibHaru(const QStringList& imagePaths,
                       const QString& outputPdfPath,
                       const QString& title,
                       int jpegQuality,
                       bool compressGrayscale,
                       const PdfExporter::ProgressCallback& progressCallback) {
  HPDF_Doc pdf = HPDF_New(haruErrorHandler, nullptr);
  if (!pdf) {
    qDebug() << "PdfExporter: Failed to create PDF document";
    return false;
  }

  HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

  if (!title.isEmpty()) {
    HPDF_SetInfoAttr(pdf, HPDF_INFO_TITLE, title.toUtf8().constData());
  }
  HPDF_SetInfoAttr(pdf, HPDF_INFO_CREATOR, "ScanTailor Advanced");

  const int totalPages = imagePaths.size();
  int pagesWritten = 0;
  int currentPage = 0;
  int monoCount = 0, grayCount = 0, colorCount = 0;
  qint64 totalDataSize = 0;

  for (const QString& imagePath : imagePaths) {
    ++currentPage;

    // Report progress and check for cancellation
    if (progressCallback) {
      if (!progressCallback(currentPage, totalPages)) {
        qDebug() << "PdfExporter: Export cancelled by user";
        HPDF_Free(pdf);
        return false;
      }
    }

    QImage image(imagePath);
    if (image.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;

    float pageWidth = image.width() * 72.0f / dpiX;
    float pageHeight = image.height() * 72.0f / dpiY;

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetWidth(page, pageWidth);
    HPDF_Page_SetHeight(page, pageHeight);

    HPDF_Image pdfImage = nullptr;
    ImageType type = detectImageType(image);

    // Use JPEG for grayscale if compressGrayscale is enabled, otherwise use Flate (lossless)
    const bool useJpegForGray = compressGrayscale && (type == ImageType::Grayscale);

    if (type == ImageType::Mono) {
      // Use 1-bit encoding for pure B&W - very efficient
      ++monoCount;
      QImage monoImage = image.convertToFormat(QImage::Format_Mono);

      // Pack bits into bytes (8 pixels per byte)
      const int bytesPerLine = (monoImage.width() + 7) / 8;
      QByteArray rawData;
      rawData.reserve(bytesPerLine * monoImage.height());

      for (int y = 0; y < monoImage.height(); ++y) {
        const uchar* line = monoImage.constScanLine(y);
        rawData.append(reinterpret_cast<const char*>(line), bytesPerLine);
      }

      totalDataSize += rawData.size();
      pdfImage = HPDF_LoadRawImageFromMem(pdf, reinterpret_cast<const HPDF_BYTE*>(rawData.constData()),
                                          monoImage.width(), monoImage.height(), HPDF_CS_DEVICE_GRAY, 1);
    } else if (type == ImageType::Grayscale && !useJpegForGray) {
      // Lossless Flate for grayscale
      ++grayCount;
      QImage grayImage = image.convertToFormat(QImage::Format_Grayscale8);

      QByteArray rawData;
      rawData.reserve(grayImage.width() * grayImage.height());

      for (int y = 0; y < grayImage.height(); ++y) {
        const uchar* line = grayImage.constScanLine(y);
        rawData.append(reinterpret_cast<const char*>(line), grayImage.width());
      }

      totalDataSize += rawData.size();
      pdfImage = HPDF_LoadRawImageFromMem(pdf, reinterpret_cast<const HPDF_BYTE*>(rawData.constData()),
                                          grayImage.width(), grayImage.height(), HPDF_CS_DEVICE_GRAY, 8);
    } else {
      // JPEG for color images, or grayscale if compressGrayscale is enabled
      if (type == ImageType::Grayscale) {
        ++grayCount;
      } else {
        ++colorCount;
      }

      QImage outImage;
      if (type == ImageType::Grayscale) {
        // Grayscale JPEG
        outImage = image.convertToFormat(QImage::Format_Grayscale8);
      } else {
        // Color JPEG
        outImage = image.convertToFormat(QImage::Format_RGB888);
      }

      QByteArray jpegData;
      QBuffer buffer(&jpegData);
      buffer.open(QIODevice::WriteOnly);
      outImage.save(&buffer, "JPEG", jpegQuality);
      buffer.close();

      totalDataSize += jpegData.size();
      pdfImage = HPDF_LoadJpegImageFromMem(pdf, reinterpret_cast<const HPDF_BYTE*>(jpegData.constData()),
                                           jpegData.size());
    }

    if (!pdfImage) {
      qWarning() << "PdfExporter: Failed to create PDF image for:" << imagePath;
      continue;
    }

    HPDF_Page_DrawImage(page, pdfImage, 0, 0, pageWidth, pageHeight);
    ++pagesWritten;
  }

  if (pagesWritten == 0) {
    HPDF_Free(pdf);
    return false;
  }

  HPDF_STATUS status = HPDF_SaveToFile(pdf, outputPdfPath.toUtf8().constData());
  HPDF_Free(pdf);

  if (status != HPDF_OK) {
    qDebug() << "PdfExporter: Failed to save PDF file";
    return false;
  }

  qDebug() << "PdfExporter: Exported" << pagesWritten << "pages"
           << "(B&W:" << monoCount << "Gray:" << grayCount << "Color:" << colorCount << ")"
           << "Data before PDF compression:" << (totalDataSize / 1024 / 1024) << "MB";
  return true;
}

#endif  // HAVE_LIBHARU

bool exportWithQt(const QStringList& imagePaths,
                  const QString& outputPdfPath,
                  const QString& title,
                  const PdfExporter::ProgressCallback& progressCallback) {
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
  const int totalPages = imagePaths.size();
  int currentPage = 0;

  for (const QString& imagePath : imagePaths) {
    ++currentPage;

    if (progressCallback) {
      if (!progressCallback(currentPage, totalPages)) {
        qDebug() << "PdfExporter: Export cancelled by user";
        return false;
      }
    }

    QImage image(imagePath);
    if (image.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;

    const qreal pageWidthPoints = image.width() * 72.0 / dpiX;
    const qreal pageHeightPoints = image.height() * 72.0 / dpiY;

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

    const QRect targetRect(0, 0, pdfWriter.width(), pdfWriter.height());
    painter.drawImage(targetRect, image);
  }

  if (!firstPage) {
    painter.end();
  }

  return !firstPage;
}

}  // namespace

bool PdfExporter::exportToPdf(const QStringList& imagePaths,
                              const QString& outputPdfPath,
                              const QString& title,
                              Quality quality,
                              bool compressGrayscale,
                              ProgressCallback progressCallback) {
  if (imagePaths.isEmpty()) {
    qDebug() << "PdfExporter: No images to export";
    return false;
  }

  const int jpegQuality = qualityToJpegPercent(quality);

#ifdef HAVE_LIBHARU
  return exportWithLibHaru(imagePaths, outputPdfPath, title, jpegQuality, compressGrayscale, progressCallback);
#else
  Q_UNUSED(jpegQuality);
  Q_UNUSED(compressGrayscale);
  qDebug() << "PdfExporter: Using Qt fallback (libharu not available)";
  return exportWithQt(imagePaths, outputPdfPath, title, progressCallback);
#endif
}
