// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "AppleVisionDetector.h"

#include <QtGlobal>  // Must include before checking Q_OS_MACOS

#ifdef Q_OS_MACOS

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <AppKit/AppKit.h>
#include <QImageReader>

namespace {

// Convert QImage to CGImage for Vision framework
CGImageRef createCGImageFromQImage(const QImage& qimage) {
  QImage img = qimage;
  if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied) {
    img = img.convertToFormat(QImage::Format_ARGB32);
  }

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      (void*)img.bits(),
      img.width(),
      img.height(),
      8,
      img.bytesPerLine(),
      colorSpace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

  CGImageRef cgImage = CGBitmapContextCreateImage(context);

  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

  return cgImage;
}

// Perform Vision request synchronously
NSArray* performVisionRequest(VNRequest* request, CGImageRef cgImage) {
  VNImageRequestHandler* handler = [[VNImageRequestHandler alloc]
      initWithCGImage:cgImage
              options:@{}];

  NSError* error = nil;
  [handler performRequests:@[request] error:&error];

  if (error) {
    NSLog(@"Vision request failed: %@", error.localizedDescription);
    return nil;
  }

  return request.results;
}

}  // namespace

bool AppleVisionDetector::isAvailable() {
  // Vision framework is available on macOS 10.13+
  if (@available(macOS 10.15, *)) {
    return true;
  }
  return false;
}

AppleVisionDetector::DocumentBounds AppleVisionDetector::detectDocumentBounds(const QImage& image) {
  DocumentBounds result;
  result.detected = false;
  result.confidence = 0.0f;

  if (!isAvailable() || image.isNull()) {
    return result;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return result;
    }

    if (@available(macOS 11.0, *)) {
      VNDetectDocumentSegmentationRequest* request = [[VNDetectDocumentSegmentationRequest alloc] init];

      NSArray* results = performVisionRequest(request, cgImage);

      if (results.count > 0) {
        VNDetectedObjectObservation* observation = results.firstObject;

        // Convert normalized coordinates to image coordinates
        CGRect bbox = observation.boundingBox;
        qreal imgWidth = image.width();
        qreal imgHeight = image.height();

        result.boundingRect = QRectF(
            bbox.origin.x * imgWidth,
            (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,  // Flip Y axis
            bbox.size.width * imgWidth,
            bbox.size.height * imgHeight);

        result.corners.clear();
        result.corners << result.boundingRect.topLeft()
                       << result.boundingRect.topRight()
                       << result.boundingRect.bottomRight()
                       << result.boundingRect.bottomLeft();

        result.confidence = observation.confidence;
        result.detected = true;
      }

      CGImageRelease(cgImage);
    } else {
      // Fallback for older macOS: use rectangle detection
      VNDetectRectanglesRequest* request = [[VNDetectRectanglesRequest alloc] init];
      request.minimumSize = 0.3f;  // At least 30% of image
      request.maximumObservations = 1;

      NSArray* results = performVisionRequest(request, cgImage);
      CGImageRelease(cgImage);

      if (results.count > 0) {
        VNRectangleObservation* rect = results.firstObject;

        qreal imgWidth = image.width();
        qreal imgHeight = image.height();

        // Convert normalized Vision coordinates to image coordinates
        auto convertPoint = [imgWidth, imgHeight](CGPoint p) {
          return QPointF(p.x * imgWidth, (1.0 - p.y) * imgHeight);
        };

        result.corners.clear();
        result.corners << convertPoint(rect.topLeft)
                       << convertPoint(rect.topRight)
                       << convertPoint(rect.bottomRight)
                       << convertPoint(rect.bottomLeft);

        result.boundingRect = result.corners.boundingRect();
        result.confidence = rect.confidence;
        result.detected = true;
      }
    }
  }

  return result;
}

QVector<AppleVisionDetector::TextRegion> AppleVisionDetector::detectTextRegions(const QImage& image) {
  QVector<TextRegion> regions;

  if (!isAvailable() || image.isNull()) {
    return regions;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return regions;
    }

    if (@available(macOS 10.15, *)) {
      VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];
      request.recognitionLevel = VNRequestTextRecognitionLevelFast;
      request.usesLanguageCorrection = NO;  // Faster without correction

      NSArray* results = performVisionRequest(request, cgImage);
      CGImageRelease(cgImage);

      qreal imgWidth = image.width();
      qreal imgHeight = image.height();

      for (VNRecognizedTextObservation* observation in results) {
        TextRegion region;

        CGRect bbox = observation.boundingBox;
        region.bounds = QRectF(
            bbox.origin.x * imgWidth,
            (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,
            bbox.size.width * imgWidth,
            bbox.size.height * imgHeight);

        region.confidence = observation.confidence;

        // Get the recognized text
        NSArray<VNRecognizedText*>* candidates = [observation topCandidates:1];
        if (candidates.count > 0) {
          region.text = QString::fromNSString(candidates.firstObject.string);
        }

        regions.append(region);
      }
    }
  }

  return regions;
}

AppleVisionDetector::ContentType AppleVisionDetector::classifyContent(const QImage& image) {
  if (!isAvailable() || image.isNull()) {
    return ContentType::Unknown;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return ContentType::Unknown;
    }

    // Use text detection to estimate content type
    QVector<TextRegion> textRegions = detectTextRegions(image);
    CGImageRelease(cgImage);

    // Calculate text coverage
    qreal totalTextArea = 0;
    for (const TextRegion& region : textRegions) {
      totalTextArea += region.bounds.width() * region.bounds.height();
    }
    qreal imageArea = image.width() * image.height();
    float textCoverage = static_cast<float>(totalTextArea / imageArea);

    // Classify based on text coverage
    if (textCoverage > 0.4f) {
      return ContentType::Document;
    } else if (textCoverage > 0.15f) {
      return ContentType::Mixed;
    } else if (textCoverage > 0.02f) {
      // Some text, likely illustration with labels
      return ContentType::Illustration;
    } else {
      return ContentType::Photo;
    }
  }
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyze(const QImage& image) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.isHighContrast = false;
  result.documentBounds.detected = false;

  if (image.isNull()) {
    return result;
  }

  // Detect document bounds
  result.documentBounds = detectDocumentBounds(image);

  // Detect text regions
  result.textRegions = detectTextRegions(image);

  // Calculate text coverage
  qreal totalTextArea = 0;
  for (const TextRegion& region : result.textRegions) {
    totalTextArea += region.bounds.width() * region.bounds.height();
  }
  qreal imageArea = image.width() * image.height();
  result.textCoverage = static_cast<float>(totalTextArea / imageArea);

  // Analyze contrast by sampling pixels
  int highContrastPixels = 0;
  int sampleCount = 0;
  const int step = qMax(1, (image.width() * image.height()) / 10000);

  for (int i = 0; i < image.width() * image.height(); i += step) {
    int x = i % image.width();
    int y = i / image.width();
    QRgb pixel = image.pixel(x, y);
    int gray = qGray(pixel);
    if (gray < 50 || gray > 205) {
      highContrastPixels++;
    }
    sampleCount++;
  }
  result.isHighContrast = (sampleCount > 0) &&
                          (static_cast<float>(highContrastPixels) / sampleCount > 0.7f);

  // Determine content type
  if (result.textCoverage > 0.4f) {
    result.contentType = ContentType::Document;
  } else if (result.textCoverage > 0.15f) {
    result.contentType = ContentType::Mixed;
  } else if (result.isHighContrast && result.textCoverage < 0.05f) {
    result.contentType = ContentType::Illustration;
  } else {
    result.contentType = ContentType::Photo;
  }

  return result;
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyzeFromFile(const QString& imagePath) {
  QImageReader reader(imagePath);
  if (!reader.canRead()) {
    AnalysisResult result;
    result.contentType = ContentType::Unknown;
    result.textCoverage = 0.0f;
    result.isHighContrast = false;
    result.documentBounds.detected = false;
    return result;
  }

  // Scale down for faster analysis
  const QSize originalSize = reader.size();
  const int maxDim = 1200;  // Larger than ImageTypeDetector for better Vision accuracy
  if (originalSize.width() > maxDim || originalSize.height() > maxDim) {
    const qreal scale = qMin(static_cast<qreal>(maxDim) / originalSize.width(),
                             static_cast<qreal>(maxDim) / originalSize.height());
    reader.setScaledSize(QSize(qRound(originalSize.width() * scale),
                               qRound(originalSize.height() * scale)));
  }

  const QImage image = reader.read();
  return analyze(image);
}

QString AppleVisionDetector::suggestColorMode(const AnalysisResult& result) {
  // High-contrast documents with lots of text -> B&W
  if (result.contentType == ContentType::Document && result.isHighContrast) {
    return QStringLiteral("bw");
  }

  // Illustrations often work well in B&W
  if (result.contentType == ContentType::Illustration && result.isHighContrast) {
    return QStringLiteral("bw");
  }

  // Photos should preserve tones
  if (result.contentType == ContentType::Photo) {
    return QStringLiteral("color");
  }

  // Mixed content - preserve grayscale at minimum
  if (result.contentType == ContentType::Mixed) {
    return QStringLiteral("grayscale");
  }

  // Default to grayscale for documents (preserves subtle details)
  if (result.contentType == ContentType::Document) {
    return QStringLiteral("grayscale");
  }

  return QStringLiteral("grayscale");
}

#else  // Non-macOS platforms

bool AppleVisionDetector::isAvailable() {
  return false;
}

AppleVisionDetector::DocumentBounds AppleVisionDetector::detectDocumentBounds(const QImage&) {
  DocumentBounds result;
  result.detected = false;
  result.confidence = 0.0f;
  return result;
}

QVector<AppleVisionDetector::TextRegion> AppleVisionDetector::detectTextRegions(const QImage&) {
  return QVector<TextRegion>();
}

AppleVisionDetector::ContentType AppleVisionDetector::classifyContent(const QImage&) {
  return ContentType::Unknown;
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyze(const QImage&) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.isHighContrast = false;
  result.documentBounds.detected = false;
  return result;
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyzeFromFile(const QString&) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.isHighContrast = false;
  result.documentBounds.detected = false;
  return result;
}

QString AppleVisionDetector::suggestColorMode(const AnalysisResult&) {
  return QStringLiteral("grayscale");
}

#endif  // Q_OS_MACOS
