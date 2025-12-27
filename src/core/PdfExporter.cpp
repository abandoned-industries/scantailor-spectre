// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfExporter.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMap>
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

// RAII wrapper for HPDF_Doc to ensure proper cleanup on all exit paths
class HpdfDocGuard {
 public:
  explicit HpdfDocGuard(HPDF_Error_Handler errorHandler, void* userData)
      : m_doc(HPDF_New(errorHandler, userData)) {}
  ~HpdfDocGuard() {
    if (m_doc) {
      HPDF_Free(m_doc);
    }
  }
  HpdfDocGuard(const HpdfDocGuard&) = delete;
  HpdfDocGuard& operator=(const HpdfDocGuard&) = delete;
  HPDF_Doc get() const { return m_doc; }
  explicit operator bool() const { return m_doc != nullptr; }

 private:
  HPDF_Doc m_doc;
};

bool exportWithLibHaru(const QStringList& imagePaths,
                       const QString& outputPdfPath,
                       const QString& title,
                       int jpegQuality,
                       bool compressGrayscale,
                       int maxDpi,
                       const QMap<QString, PdfExporter::OcrTextData>& ocrData,
                       const PdfExporter::ProgressCallback& progressCallback) {
  HpdfDocGuard pdfGuard(haruErrorHandler, nullptr);
  if (!pdfGuard) {
    qDebug() << "PdfExporter: Failed to create PDF document";
    return false;
  }
  HPDF_Doc pdf = pdfGuard.get();

  HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

  if (!title.isEmpty()) {
    const QByteArray titleUtf8 = title.toUtf8();
    HPDF_SetInfoAttr(pdf, HPDF_INFO_TITLE, titleUtf8.constData());
  }
  HPDF_SetInfoAttr(pdf, HPDF_INFO_CREATOR, "ScanTailor Spectre");

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
        return false;  // RAII cleanup handles HPDF_Free
      }
    }

    QImage image(imagePath);
    if (image.isNull()) {
      qDebug() << "PdfExporter: Failed to load image:" << imagePath;
      continue;
    }

    int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
    int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;
    const int originalDpi = qMax(dpiX, dpiY);

    // Detect image type BEFORE downsampling (need to know if color to skip downsampling)
    ImageType type = detectImageType(image);

    // Log first page dimensions to help debug file size issues
    if (currentPage == 1) {
      qDebug() << "PdfExporter: First image dimensions:" << image.width() << "x" << image.height()
               << "at" << dpiX << "DPI" << (maxDpi > 0 ? QString("(max: %1)").arg(maxDpi) : QString());
    }

    // Skip downsampling for B&W pages - they need high DPI for sharp text edges
    // Only downsample grayscale/color images which have anti-aliasing
    const bool shouldDownsample = (maxDpi > 0 && originalDpi > maxDpi && type != ImageType::Mono);

    if (shouldDownsample) {
      const double scale = static_cast<double>(maxDpi) / originalDpi;
      const int newWidth = qRound(image.width() * scale);
      const int newHeight = qRound(image.height() * scale);
      image = image.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
      dpiX = maxDpi;
      dpiY = maxDpi;
      // Update DPI in image metadata
      image.setDotsPerMeterX(qRound(maxDpi / 0.0254));
      image.setDotsPerMeterY(qRound(maxDpi / 0.0254));
    }

    float pageWidth = image.width() * 72.0f / dpiX;
    float pageHeight = image.height() * 72.0f / dpiY;

    HPDF_Page page = HPDF_AddPage(pdf);
    if (!page) {
      qWarning() << "PdfExporter: Failed to add page" << currentPage;
      continue;
    }
    HPDF_Page_SetWidth(page, pageWidth);
    HPDF_Page_SetHeight(page, pageHeight);

    HPDF_Image pdfImage = nullptr;

    // Use JPEG for grayscale if compressGrayscale is enabled, otherwise use Flate (lossless)
    const bool useJpegForGray = compressGrayscale && (type == ImageType::Grayscale);

    if (type == ImageType::Mono) {
      // Use 1-bit encoding for pure B&W - very efficient
      ++monoCount;
      QImage monoImage = image.convertToFormat(QImage::Format_Mono);

      // Pack bits into bytes (8 pixels per byte)
      // Note: libharu expects 0=black, 1=white, but Qt's Format_Mono may have inverted polarity
      // We need to invert the bits if Qt's color table has white at index 0
      const bool needsInvert = (monoImage.color(0) == 0xFFFFFFFF);  // white at index 0?

      const int bytesPerLine = (monoImage.width() + 7) / 8;
      QByteArray rawData;
      rawData.reserve(bytesPerLine * monoImage.height());

      for (int y = 0; y < monoImage.height(); ++y) {
        const uchar* line = monoImage.constScanLine(y);
        if (needsInvert) {
          for (int x = 0; x < bytesPerLine; ++x) {
            rawData.append(static_cast<char>(~line[x]));  // invert bits
          }
        } else {
          rawData.append(reinterpret_cast<const char*>(line), bytesPerLine);
        }
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
      if (!buffer.open(QIODevice::WriteOnly)) {
        qWarning() << "PdfExporter: Failed to open buffer for page" << currentPage;
        continue;
      }
      // Use higher quality for color pages (covers often have fine line art)
      const int effectiveQuality = (type == ImageType::Color) ? qMax(jpegQuality, 95) : jpegQuality;
      if (!outImage.save(&buffer, "JPEG", effectiveQuality)) {
        qWarning() << "PdfExporter: Failed to save JPEG for page" << currentPage;
        buffer.close();
        continue;
      }
      buffer.close();

      // Log first few JPEG sizes to help debug
      if (currentPage <= 3) {
        qDebug() << "PdfExporter: Page" << currentPage << "JPEG size:" << (jpegData.size() / 1024) << "KB";
      }

      totalDataSize += jpegData.size();
      pdfImage = HPDF_LoadJpegImageFromMem(pdf, reinterpret_cast<const HPDF_BYTE*>(jpegData.constData()),
                                           jpegData.size());
    }

    if (!pdfImage) {
      qWarning() << "PdfExporter: Failed to create PDF image for:" << imagePath;
      continue;
    }

    HPDF_Page_DrawImage(page, pdfImage, 0, 0, pageWidth, pageHeight);

    // Add invisible text layer for OCR if available
    auto ocrIt = ocrData.find(imagePath);
    if (ocrIt != ocrData.end() && !ocrIt->words.isEmpty()) {
      const PdfExporter::OcrTextData& ocr = ocrIt.value();

      // Calculate scale factors from image coords to PDF coords
      // PDF coords: origin at bottom-left, units in points (72 per inch)
      const float scaleX = (ocr.imageWidth > 0) ? pageWidth / ocr.imageWidth : 1.0f;
      const float scaleY = (ocr.imageHeight > 0) ? pageHeight / ocr.imageHeight : 1.0f;

      // Load a font for the text layer (use a standard font)
      HPDF_Font font = HPDF_GetFont(pdf, "Helvetica", nullptr);

      for (const auto& word : ocr.words) {
        // Skip empty text
        if (word.text.isEmpty()) {
          continue;
        }

        // Convert image coords (Y from top) to PDF coords (Y from bottom)
        // Image: (x, y) where y=0 is top
        // PDF: (x, y) where y=0 is bottom
        const float pdfX = word.bounds.x() * scaleX;
        const float pdfY = pageHeight - (word.bounds.y() + word.bounds.height()) * scaleY;

        // Calculate font size to match word height
        const float wordHeight = word.bounds.height() * scaleY;
        // Use 80% of box height as font size (typical line height ratio)
        const float fontSize = wordHeight * 0.8f;

        if (fontSize < 1.0f || fontSize > 1000.0f) {
          continue;  // Skip invalid sizes
        }

        // Each word gets its own text object for absolute positioning
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, fontSize);
        // Set text to invisible (render mode 3 = neither fill nor stroke)
        HPDF_Page_SetTextRenderingMode(page, HPDF_INVISIBLE);
        HPDF_Page_MoveTextPos(page, pdfX, pdfY);

        // Convert to UTF-8 for PDF
        const QByteArray textUtf8 = word.text.toUtf8();
        HPDF_Page_ShowText(page, textUtf8.constData());
        HPDF_Page_EndText(page);
      }

      qDebug() << "PdfExporter: Added" << ocr.words.size() << "text blocks for OCR";
    }

    ++pagesWritten;
  }

  if (pagesWritten == 0) {
    return false;  // RAII cleanup handles HPDF_Free
  }

  // Signal that we're now saving the PDF (all pages processed)
  if (progressCallback) {
    progressCallback(totalPages, totalPages);
  }

  const QByteArray outputPathUtf8 = outputPdfPath.toUtf8();
  HPDF_STATUS status = HPDF_SaveToFile(pdf, outputPathUtf8.constData());
  // RAII cleanup handles HPDF_Free

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

  // Signal that we're now saving the PDF (all pages processed)
  if (progressCallback) {
    progressCallback(totalPages, totalPages);
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
                              int maxDpi,
                              const QMap<QString, OcrTextData>& ocrData,
                              ProgressCallback progressCallback) {
  if (imagePaths.isEmpty()) {
    qDebug() << "PdfExporter: No images to export";
    return false;
  }

  const int jpegQuality = qualityToJpegPercent(quality);

#ifdef HAVE_LIBHARU
  return exportWithLibHaru(imagePaths, outputPdfPath, title, jpegQuality, compressGrayscale, maxDpi, ocrData,
                           progressCallback);
#else
  Q_UNUSED(jpegQuality);
  Q_UNUSED(compressGrayscale);
  Q_UNUSED(maxDpi);
  Q_UNUSED(ocrData);
  qDebug() << "PdfExporter: Using Qt fallback (libharu not available)";
  return exportWithQt(imagePaths, outputPdfPath, title, progressCallback);
#endif
}
