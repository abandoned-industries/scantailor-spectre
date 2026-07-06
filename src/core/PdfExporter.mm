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

#include "TiffReader.h"

#ifdef __APPLE__
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>
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

/**
 * Load an image from the given path, using TiffReader for TIFF files.
 */
QImage loadImage(const QString& imagePath) {
  QImage image;
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == "tif" || ext == "tiff") {
    QFile file(imagePath);
    if (file.open(QIODevice::ReadOnly)) {
      image = TiffReader::readImage(file);
      file.close();
    }
  } else {
    image = QImage(imagePath);
  }
  return image;
}

#ifdef __APPLE__

/**
 * CGPDFContext-based PDF exporter. Writes incrementally per-page so memory
 * usage is O(1) regardless of document size. Replaces the libharu exporter
 * which accumulated all image data in memory before writing.
 */
bool exportWithCoreGraphics(const QStringList& imagePaths,
                            const QString& outputPdfPath,
                            const QString& title,
                            const QString& author,
                            int jpegQuality,
                            bool compressGrayscale,
                            int maxDpi,
                            const QMap<QString, PdfExporter::OcrTextData>& ocrData,
                            const PdfExporter::ProgressCallback& progressCallback) {
  @autoreleasepool {
    // Create PDF context writing to the output file
    NSString* nsPath = [NSString stringWithUTF8String:outputPdfPath.toUtf8().constData()];
    NSURL* url = [NSURL fileURLWithPath:nsPath];

    NSMutableDictionary* auxInfo = [NSMutableDictionary dictionary];
    if (!title.isEmpty()) {
      auxInfo[(NSString*)kCGPDFContextTitle] =
          [NSString stringWithUTF8String:title.toUtf8().constData()];
    }
    if (!author.isEmpty()) {
      auxInfo[(NSString*)kCGPDFContextAuthor] =
          [NSString stringWithUTF8String:author.toUtf8().constData()];
    }
    auxInfo[(NSString*)kCGPDFContextCreator] = @"ScanTailor Spectre";

    CGContextRef pdfCtx = CGPDFContextCreateWithURL((__bridge CFURLRef)url, NULL,
                                                     (__bridge CFDictionaryRef)auxInfo);
    if (!pdfCtx) {
      qWarning() << "PdfExporter: Failed to create CGPDFContext";
      return false;
    }

    const int totalPages = imagePaths.size();
    int pagesWritten = 0;
    int currentPage = 0;
    int monoCount = 0, grayCount = 0, colorCount = 0;

    // Create a reusable font for OCR text
    CTFontRef baseFont = CTFontCreateWithName(CFSTR("Helvetica"), 12.0, NULL);

    for (const QString& imagePath : imagePaths) {
      @autoreleasepool {
        ++currentPage;

        // Report progress and check for cancellation
        if (progressCallback) {
          if (!progressCallback(currentPage, totalPages)) {
            qDebug() << "PdfExporter: Export cancelled by user";
            CFRelease(baseFont);
            CGPDFContextClose(pdfCtx);
            CGContextRelease(pdfCtx);
            // Remove partial file
            QFile::remove(outputPdfPath);
            return false;
          }
        }

        // Load image
        QImage image = loadImage(imagePath);
        if (image.isNull()) {
          qDebug() << "PdfExporter: Failed to load image:" << imagePath;
          continue;
        }

        int dpiX = image.dotsPerMeterX() > 0 ? qRound(image.dotsPerMeterX() * 0.0254) : 300;
        int dpiY = image.dotsPerMeterY() > 0 ? qRound(image.dotsPerMeterY() * 0.0254) : 300;
        const int originalDpi = qMax(dpiX, dpiY);

        ImageType type = detectImageType(image);

        if (currentPage == 1) {
          qDebug() << "PdfExporter: First image dimensions:" << image.width() << "x" << image.height()
                   << "at" << dpiX << "DPI" << (maxDpi > 0 ? QString("(max: %1)").arg(maxDpi) : QString());
        }

        // Downsample grayscale/color if needed (never downsample mono)
        const bool shouldDownsample = (maxDpi > 0 && originalDpi > maxDpi && type != ImageType::Mono);
        if (shouldDownsample) {
          const double scale = static_cast<double>(maxDpi) / originalDpi;
          const int newWidth = qRound(image.width() * scale);
          const int newHeight = qRound(image.height() * scale);
          image = image.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
          dpiX = maxDpi;
          dpiY = maxDpi;
        }

        const CGFloat pageWidth = image.width() * 72.0 / dpiX;
        const CGFloat pageHeight = image.height() * 72.0 / dpiY;

        // Begin page
        CGRect mediaBox = CGRectMake(0, 0, pageWidth, pageHeight);
        CGContextBeginPage(pdfCtx, &mediaBox);

        // Draw the image
        const bool useJpegForGray = compressGrayscale && (type == ImageType::Grayscale);
        CGImageRef cgImage = NULL;
        CGColorSpaceRef colorSpace = NULL;
        CGDataProviderRef provider = NULL;
        CFDataRef cfJpegData = NULL;
        QByteArray rawData;       // must outlive CGImage
        QByteArray jpegData;      // must outlive CGImage
        QImage convertedImage;    // must outlive CGImage

        if (type == ImageType::Mono) {
          // 1-bit B&W: create from raw mono data
          ++monoCount;
          convertedImage = image.convertToFormat(QImage::Format_Mono);

          // CGImage 1-bit DeviceGray: 0=black, 1=white
          // Qt Format_Mono: depends on color table
          const bool needsInvert = (convertedImage.color(0) == 0xFFFFFFFF);
          const int bytesPerLine = (convertedImage.width() + 7) / 8;

          rawData.reserve(bytesPerLine * convertedImage.height());
          for (int y = 0; y < convertedImage.height(); ++y) {
            const uchar* line = convertedImage.constScanLine(y);
            if (needsInvert) {
              for (int x = 0; x < bytesPerLine; ++x) {
                rawData.append(static_cast<char>(~line[x]));
              }
            } else {
              rawData.append(reinterpret_cast<const char*>(line), bytesPerLine);
            }
          }

          colorSpace = CGColorSpaceCreateDeviceGray();
          provider = CGDataProviderCreateWithData(NULL, rawData.constData(), rawData.size(), NULL);
          cgImage = CGImageCreate(
              convertedImage.width(), convertedImage.height(),
              1, 1, bytesPerLine,
              colorSpace,
              kCGBitmapByteOrderDefault,
              provider, NULL, false, kCGRenderingIntentDefault);

        } else if (type == ImageType::Grayscale && !useJpegForGray) {
          // 8-bit grayscale lossless
          ++grayCount;
          convertedImage = image.convertToFormat(QImage::Format_Grayscale8);

          colorSpace = CGColorSpaceCreateDeviceGray();
          provider = CGDataProviderCreateWithData(
              NULL, convertedImage.constBits(), convertedImage.sizeInBytes(), NULL);
          cgImage = CGImageCreate(
              convertedImage.width(), convertedImage.height(),
              8, 8, convertedImage.bytesPerLine(),
              colorSpace,
              kCGBitmapByteOrderDefault,
              provider, NULL, false, kCGRenderingIntentDefault);

        } else {
          // JPEG path for color and compressed grayscale
          // CGImageCreateWithJPEGDataProvider passes JPEG through to PDF (no recompression)
          if (type == ImageType::Grayscale) {
            ++grayCount;
            convertedImage = image.convertToFormat(QImage::Format_Grayscale8);
          } else {
            ++colorCount;
            convertedImage = image.convertToFormat(QImage::Format_RGB888);
          }

          QBuffer buffer(&jpegData);
          if (!buffer.open(QIODevice::WriteOnly)) {
            qWarning() << "PdfExporter: Failed to open buffer for page" << currentPage;
            CGContextEndPage(pdfCtx);
            continue;
          }
          const int effectiveQuality = (type == ImageType::Color) ? qMax(jpegQuality, 95) : jpegQuality;
          if (!convertedImage.save(&buffer, "JPEG", effectiveQuality)) {
            qWarning() << "PdfExporter: Failed to save JPEG for page" << currentPage;
            buffer.close();
            CGContextEndPage(pdfCtx);
            continue;
          }
          buffer.close();

          if (currentPage <= 3) {
            qDebug() << "PdfExporter: Page" << currentPage << "JPEG size:" << (jpegData.size() / 1024) << "KB";
          }

          cfJpegData = CFDataCreateWithBytesNoCopy(
              kCFAllocatorDefault,
              reinterpret_cast<const UInt8*>(jpegData.constData()),
              jpegData.size(),
              kCFAllocatorNull);
          provider = CGDataProviderCreateWithCFData(cfJpegData);
          cgImage = CGImageCreateWithJPEGDataProvider(
              provider, NULL, false, kCGRenderingIntentDefault);
        }

        if (cgImage) {
          CGContextDrawImage(pdfCtx, mediaBox, cgImage);
          CGImageRelease(cgImage);
        } else {
          qWarning() << "PdfExporter: Failed to create CGImage for page" << currentPage;
        }

        // Draw invisible OCR text layer
        auto ocrIt = ocrData.find(imagePath);
        if (ocrIt != ocrData.end() && !ocrIt->words.isEmpty()) {
          const PdfExporter::OcrTextData& ocr = ocrIt.value();

          const CGFloat scaleX = (ocr.imageWidth > 0) ? pageWidth / ocr.imageWidth : 1.0;
          const CGFloat scaleY = (ocr.imageHeight > 0) ? pageHeight / ocr.imageHeight : 1.0;

          for (const auto& word : ocr.words) {
            if (word.text.isEmpty()) continue;

            // Convert image coords (Y from top) to PDF coords (Y from bottom)
            const CGFloat pdfX = word.bounds.x() * scaleX;
            const CGFloat pdfY = pageHeight - (word.bounds.y() + word.bounds.height()) * scaleY;
            const CGFloat targetWidth = word.bounds.width() * scaleX;
            const CGFloat targetHeight = word.bounds.height() * scaleY;
            const CGFloat fontSize = targetHeight;

            if (fontSize < 1.0 || fontSize > 1000.0) continue;

            // Create sized font
            CTFontRef sizedFont = CTFontCreateCopyWithAttributes(baseFont, fontSize, NULL, NULL);

            NSString* nsText = [NSString stringWithUTF8String:word.text.toUtf8().constData()];
            if (!nsText || nsText.length == 0) {
              CFRelease(sizedFont);
              continue;
            }

            NSDictionary* attrs = @{
              (NSString*)kCTFontAttributeName: (__bridge id)sizedFont,
            };
            NSAttributedString* attrStr = [[NSAttributedString alloc]
                initWithString:nsText attributes:attrs];

            CTLineRef line = CTLineCreateWithAttributedString(
                (__bridge CFAttributedStringRef)attrStr);

            // Measure natural width for horizontal scaling
            CGFloat naturalWidth = CTLineGetTypographicBounds(line, NULL, NULL, NULL);

            CGContextSaveGState(pdfCtx);
            CGContextSetTextDrawingMode(pdfCtx, kCGTextInvisible);

            // Position and scale to match word bounds
            CGContextTranslateCTM(pdfCtx, pdfX, pdfY);
            if (naturalWidth > 0.01) {
              CGFloat hScale = targetWidth / naturalWidth;
              hScale = fmax(0.5, fmin(hScale, 2.0));
              CGContextScaleCTM(pdfCtx, hScale, 1.0);
            }

            CGContextSetTextPosition(pdfCtx, 0, 0);
            CTLineDraw(line, pdfCtx);

            CGContextRestoreGState(pdfCtx);

            CFRelease(line);
            CFRelease(sizedFont);
          }

          qDebug() << "PdfExporter: Added" << ocr.words.size() << "text blocks for OCR";
        }

        // End page - flushes to disk, memory for this page can be reclaimed
        CGContextEndPage(pdfCtx);

        // Clean up per-page resources
        if (provider) CGDataProviderRelease(provider);
        if (colorSpace) CGColorSpaceRelease(colorSpace);
        if (cfJpegData) CFRelease(cfJpegData);

        ++pagesWritten;
      }  // @autoreleasepool per page
    }

    CFRelease(baseFont);

    // Signal save phase
    if (progressCallback) {
      progressCallback(totalPages, totalPages);
    }

    CGPDFContextClose(pdfCtx);
    CGContextRelease(pdfCtx);

    if (pagesWritten == 0) {
      QFile::remove(outputPdfPath);
      return false;
    }

    qDebug() << "PdfExporter: Exported" << pagesWritten << "pages"
             << "(B&W:" << monoCount << "Gray:" << grayCount << "Color:" << colorCount << ")"
             << "using CGPDFContext (incremental write)";
    return true;
  }  // @autoreleasepool
}

#endif  // __APPLE__

bool exportWithQt(const QStringList& imagePaths,
                  const QString& outputPdfPath,
                  const QString& title,
                  const QString& author,
                  const PdfExporter::ProgressCallback& progressCallback) {
  // QPdfWriter has no author metadata API; author is intentionally unused here.
  Q_UNUSED(author);
  QFile file(outputPdfPath);
  if (!file.open(QIODevice::WriteOnly)) {
    qDebug() << "PdfExporter: Failed to open output file:" << outputPdfPath;
    return false;
  }

  QPdfWriter pdfWriter(&file);
  pdfWriter.setCreator("ScanTailor Spectre");
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

    QImage image = loadImage(imagePath);
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
                              const QString& author,
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

#ifdef __APPLE__
  return exportWithCoreGraphics(imagePaths, outputPdfPath, title, author, jpegQuality, compressGrayscale, maxDpi,
                                ocrData, progressCallback);
#else
  Q_UNUSED(jpegQuality);
  Q_UNUSED(compressGrayscale);
  Q_UNUSED(maxDpi);
  Q_UNUSED(ocrData);
  qDebug() << "PdfExporter: Using Qt fallback (CoreGraphics not available)";
  return exportWithQt(imagePaths, outputPdfPath, title, author, progressCallback);
#endif
}
