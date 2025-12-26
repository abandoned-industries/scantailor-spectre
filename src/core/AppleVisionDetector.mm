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
// Creates an independent copy of the pixel data to ensure thread safety
CGImageRef createCGImageFromQImage(const QImage& qimage) {
  QImage img = qimage;
  if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_ARGB32_Premultiplied) {
    img = img.convertToFormat(QImage::Format_ARGB32);
  }

  // Create an immutable copy of the pixel data - this ensures the CGImage
  // owns its data and is thread-safe even after img goes out of scope
  CFDataRef pixelData = CFDataCreate(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(img.constBits()),
      img.sizeInBytes());

  if (!pixelData) {
    return nullptr;
  }

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(pixelData);
  CFRelease(pixelData);  // dataProvider now owns the data

  if (!dataProvider) {
    CGColorSpaceRelease(colorSpace);
    return nullptr;
  }

  CGImageRef cgImage = CGImageCreate(
      img.width(),
      img.height(),
      8,                          // bits per component
      32,                         // bits per pixel
      img.bytesPerLine(),
      colorSpace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
      dataProvider,
      nullptr,                    // decode array
      false,                      // should interpolate
      kCGRenderingIntentDefault);

  CGDataProviderRelease(dataProvider);
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

AppleVisionDetector::PageSplitResult AppleVisionDetector::detectPageSplit(const QImage& image) {
  PageSplitResult result;
  result.shouldSplit = false;
  result.splitLineX = 0.5;
  result.confidence = 0.0f;
  result.leftTextRegions = 0;
  result.rightTextRegions = 0;

  if (!isAvailable() || image.isNull()) {
    return result;
  }

  // Guard against division by zero
  if (image.height() == 0) {
    qDebug() << "PageSplit: image has zero height";
    return result;
  }

  // Check aspect ratio - but use a lower threshold to allow analysis
  // We'll use other signals (like page numbers) to validate
  const qreal aspectRatio = static_cast<qreal>(image.width()) / image.height();
  if (aspectRatio < 0.85) {
    // Clearly portrait - definitely not a two-page spread
    qDebug() << "PageSplit: aspect ratio" << aspectRatio << "too narrow for two-page spread (need > 0.85)";
    return result;
  }

  qDebug() << "PageSplit: analyzing image with aspect ratio" << aspectRatio;

  // Detect text regions
  QVector<TextRegion> regions = detectTextRegions(image);

  if (regions.isEmpty()) {
    qDebug() << "PageSplit: no text regions detected";
    return result;
  }

  qDebug() << "PageSplit: detected" << regions.size() << "text regions";

  const qreal imageWidth = image.width();
  const qreal imageHeight = image.height();

  // SMART FEATURE: Page number detection
  // Look for small text regions in bottom corners that contain numbers
  // This is a very strong signal of a two-page spread
  bool hasLeftPageNumber = false;
  bool hasRightPageNumber = false;
  QString leftPageNum, rightPageNum;

  // Define corner regions for page numbers (bottom 15% of page, outer 25% on each side)
  const qreal bottomThreshold = imageHeight * 0.85;
  const qreal leftCornerEnd = imageWidth * 0.25;
  const qreal rightCornerStart = imageWidth * 0.75;

  // Also check for page numbers near the center-bottom (common in some books)
  const qreal centerLeftStart = imageWidth * 0.35;
  const qreal centerLeftEnd = imageWidth * 0.48;
  const qreal centerRightStart = imageWidth * 0.52;
  const qreal centerRightEnd = imageWidth * 0.65;

  for (const TextRegion& region : regions) {
    // Check if region is in the bottom area
    if (region.bounds.top() < bottomThreshold) continue;

    // Check if the text looks like a page number (short, contains digits)
    const QString text = region.text.trimmed();
    if (text.isEmpty() || text.length() > 10) continue;

    // Check if it contains at least one digit
    bool hasDigit = false;
    for (const QChar& c : text) {
      if (c.isDigit()) {
        hasDigit = true;
        break;
      }
    }
    if (!hasDigit) continue;

    const qreal regionCenterX = region.bounds.center().x();

    // Check left side (outer corner or center-left)
    if ((regionCenterX < leftCornerEnd) ||
        (regionCenterX >= centerLeftStart && regionCenterX < centerLeftEnd)) {
      hasLeftPageNumber = true;
      leftPageNum = text;
      qDebug() << "PageSplit: found left page number candidate:" << text << "at x:" << regionCenterX;
    }
    // Check right side (outer corner or center-right)
    else if ((regionCenterX > rightCornerStart) ||
             (regionCenterX >= centerRightStart && regionCenterX <= centerRightEnd)) {
      hasRightPageNumber = true;
      rightPageNum = text;
      qDebug() << "PageSplit: found right page number candidate:" << text << "at x:" << regionCenterX;
    }
  }

  // If we found page numbers on both sides, that's a very strong signal
  const bool hasPageNumbers = hasLeftPageNumber && hasRightPageNumber;
  if (hasPageNumbers) {
    qDebug() << "PageSplit: STRONG SIGNAL - page numbers detected on both sides:"
             << leftPageNum << "/" << rightPageNum;
  }

  // Analyze horizontal distribution of text regions using zone-based clustering
  // The key insight: two-page spreads have text clustered in left and right zones
  // with relatively empty center (the gutter), while single pages with columns
  // have text distributed more evenly or in different patterns.

  // Define zones: left page (0%-48%), gutter (48%-52%), right page (52%-100%)
  // Wider page zones and narrower gutter to catch more text regions
  const qreal leftZoneEnd = imageWidth * 0.48;
  const qreal rightZoneStart = imageWidth * 0.52;
  const qreal gutterStart = imageWidth * 0.48;
  const qreal gutterEnd = imageWidth * 0.52;

  // Count regions in each zone
  int leftCount = 0;
  int rightCount = 0;
  int gutterCount = 0;

  for (const TextRegion& region : regions) {
    const qreal regionCenterX = region.bounds.center().x();

    if (regionCenterX < leftZoneEnd) {
      leftCount++;
    } else if (regionCenterX >= rightZoneStart) {
      rightCount++;
    } else if (regionCenterX >= gutterStart && regionCenterX <= gutterEnd) {
      gutterCount++;
    }
  }

  result.leftTextRegions = leftCount;
  result.rightTextRegions = rightCount;

  qDebug() << "PageSplit: left zone regions:" << leftCount
           << "gutter regions:" << gutterCount
           << "right zone regions:" << rightCount;

  // If we have page numbers on both sides, use relaxed criteria
  // Otherwise, use stricter criteria
  int minRegionsPerSide = hasPageNumbers ? 1 : 3;
  qreal maxGutterRatio = hasPageNumbers ? 0.25 : 0.15;
  qreal minBalance = hasPageNumbers ? 0.05 : 0.1;
  float minConfidence = hasPageNumbers ? 0.40f : 0.60f;

  // Determine if this looks like a two-page spread
  if (leftCount < minRegionsPerSide || rightCount < minRegionsPerSide) {
    qDebug() << "PageSplit: not enough regions on both sides (need" << minRegionsPerSide << "each)";
    return result;
  }

  // The gutter should be relatively empty compared to the page zones
  // If there's too much content in the gutter, it's probably a single page
  const int totalSideRegions = leftCount + rightCount;
  const qreal gutterRatio = static_cast<qreal>(gutterCount) / (totalSideRegions + gutterCount);
  if (gutterRatio > maxGutterRatio) {
    qDebug() << "PageSplit: too much content in gutter zone:" << (gutterRatio * 100) << "% (max:" << (maxGutterRatio * 100) << "%)";
    return result;
  }

  // The regions should have some balance between left and right
  // But we use a low threshold because one page might be mostly graphics
  const qreal balance = static_cast<qreal>(qMin(leftCount, rightCount)) / qMax(leftCount, rightCount);
  if (balance < minBalance) {
    qDebug() << "PageSplit: extremely unbalanced sides, ratio:" << balance << "(min:" << minBalance << ")";
    return result;
  }

  // Calculate confidence based on various factors
  float confidence = 0.0f;

  // Factor 1: Balance of content (more balanced = higher confidence)
  confidence += static_cast<float>(balance) * 0.20f;

  // Factor 2: Empty gutter (less content in gutter = higher confidence)
  const float gutterEmptiness = 1.0f - static_cast<float>(gutterRatio) / maxGutterRatio;
  confidence += gutterEmptiness * 0.20f;

  // Factor 3: Number of text regions (more regions = more confident in the analysis)
  const float regionFactor = qMin(totalSideRegions / 20.0f, 1.0f);  // Max out at 20 regions
  confidence += regionFactor * 0.20f;

  // Factor 4: Aspect ratio (wider images are more likely to be spreads)
  const float aspectFactor = qMin(qMax((static_cast<float>(aspectRatio) - 0.85f) / 0.5f, 0.0f), 1.0f);
  confidence += aspectFactor * 0.20f;

  // Factor 5: Page numbers (strong signal!)
  const float pageNumberFactor = hasPageNumbers ? 1.0f : 0.0f;
  confidence += pageNumberFactor * 0.20f;

  qDebug() << "PageSplit: confidence factors - balance:" << balance
           << "gutterEmpty:" << gutterEmptiness
           << "regions:" << regionFactor
           << "aspect:" << aspectFactor
           << "pageNumbers:" << pageNumberFactor
           << "total:" << confidence;

  // Check minimum confidence
  if (confidence < minConfidence) {
    qDebug() << "PageSplit: confidence too low:" << confidence << "(min:" << minConfidence << ")";
    return result;
  }

  // Split at the center of the image (the gutter)
  result.splitLineX = 0.5;  // Always split at center for two-page spreads
  result.shouldSplit = true;
  result.confidence = confidence;

  qDebug() << "PageSplit: detected two-page spread, split at:" << result.splitLineX
           << "confidence:" << confidence;

  return result;
}

AppleVisionDetector::PageSplitResult AppleVisionDetector::detectPageSplitFromFile(const QString& imagePath) {
  QImageReader reader(imagePath);
  if (!reader.canRead()) {
    PageSplitResult result;
    result.shouldSplit = false;
    result.splitLineX = 0.5;
    result.confidence = 0.0f;
    result.leftTextRegions = 0;
    result.rightTextRegions = 0;
    return result;
  }

  // Scale down for faster analysis
  const QSize originalSize = reader.size();
  const int maxDim = 1500;  // Good resolution for text detection
  if (originalSize.width() > maxDim || originalSize.height() > maxDim) {
    const qreal scale = qMin(static_cast<qreal>(maxDim) / originalSize.width(),
                             static_cast<qreal>(maxDim) / originalSize.height());
    reader.setScaledSize(QSize(qRound(originalSize.width() * scale),
                               qRound(originalSize.height() * scale)));
  }

  const QImage image = reader.read();
  return detectPageSplit(image);
}

QVector<AppleVisionDetector::OcrWordResult> AppleVisionDetector::performOcr(const QImage& image,
                                                                             const OcrConfig& config) {
  QVector<OcrWordResult> results;

  if (!isAvailable() || image.isNull()) {
    return results;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return results;
    }

    if (@available(macOS 10.15, *)) {
      VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];

      // Set recognition level
      request.recognitionLevel = config.useAccurateRecognition
                                     ? VNRequestTextRecognitionLevelAccurate
                                     : VNRequestTextRecognitionLevelFast;

      request.usesLanguageCorrection = config.usesLanguageCorrection;

      // Set language if specified (non-empty means user selected a specific language)
      if (!config.languageCode.isEmpty()) {
        NSString* langCode = config.languageCode.toNSString();
        request.recognitionLanguages = @[ langCode ];
      }
      // If empty, Vision will auto-detect the language

      NSArray* observations = performVisionRequest(request, cgImage);
      CGImageRelease(cgImage);

      qreal imgWidth = image.width();
      qreal imgHeight = image.height();

      for (VNRecognizedTextObservation* observation in observations) {
        NSArray<VNRecognizedText*>* candidates = [observation topCandidates:1];
        if (candidates.count == 0) {
          continue;
        }

        VNRecognizedText* bestCandidate = candidates.firstObject;
        NSString* fullText = bestCandidate.string;

        // Get the bounding box for the entire text line/block
        CGRect bbox = observation.boundingBox;

        OcrWordResult word;
        word.text = QString::fromNSString(fullText);
        word.confidence = observation.confidence;

        // Convert Vision coordinates (normalized, Y-inverted) to image coords
        // Vision: origin at bottom-left, normalized 0-1
        // Image: origin at top-left, in pixels
        word.bounds = QRectF(bbox.origin.x * imgWidth,
                             (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,
                             bbox.size.width * imgWidth,
                             bbox.size.height * imgHeight);

        results.append(word);
      }
    }
  }

  return results;
}

QStringList AppleVisionDetector::supportedOcrLanguages() {
  QStringList languages;

  if (@available(macOS 10.15, *)) {
    // Return the list of supported languages for accurate recognition
    // Based on Apple Vision documentation
    languages << "en-US"
              << "fr-FR"
              << "de-DE"
              << "it-IT"
              << "pt-BR"
              << "es-ES"
              << "zh-Hans"
              << "zh-Hant"
              << "ja-JP"
              << "ko-KR"
              << "ru-RU"
              << "uk-UA"
              << "pl-PL"
              << "cs-CZ"
              << "nl-NL"
              << "da-DK"
              << "fi-FI"
              << "nb-NO"
              << "sv-SE"
              << "tr-TR"
              << "el-GR"
              << "hu-HU"
              << "ro-RO"
              << "th-TH"
              << "vi-VN";
  }

  return languages;
}

#else  // Non-macOS platforms

bool AppleVisionDetector::isAvailable() {
  return false;
}

QVector<AppleVisionDetector::TextRegion> AppleVisionDetector::detectTextRegions(const QImage&) {
  return QVector<TextRegion>();
}

AppleVisionDetector::PageSplitResult AppleVisionDetector::detectPageSplit(const QImage&) {
  PageSplitResult result;
  result.shouldSplit = false;
  result.splitLineX = 0.5;
  result.confidence = 0.0f;
  result.leftTextRegions = 0;
  result.rightTextRegions = 0;
  return result;
}

AppleVisionDetector::PageSplitResult AppleVisionDetector::detectPageSplitFromFile(const QString&) {
  PageSplitResult result;
  result.shouldSplit = false;
  result.splitLineX = 0.5;
  result.confidence = 0.0f;
  result.leftTextRegions = 0;
  result.rightTextRegions = 0;
  return result;
}

QVector<AppleVisionDetector::OcrWordResult> AppleVisionDetector::performOcr(const QImage&, const OcrConfig&) {
  return QVector<OcrWordResult>();
}

QStringList AppleVisionDetector::supportedOcrLanguages() {
  return QStringList();
}

#endif  // Q_OS_MACOS
