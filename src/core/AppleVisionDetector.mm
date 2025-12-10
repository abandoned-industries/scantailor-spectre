// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "AppleVisionDetector.h"

#include <QtGlobal>  // Must include before checking Q_OS_MACOS

#ifdef Q_OS_MACOS

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <AppKit/AppKit.h>
#import <Accelerate/Accelerate.h>
#include <QImageReader>

namespace {

// =============================================================================
// ACCELERATE FRAMEWORK OPTIMIZED FUNCTIONS
// These use Apple's SIMD-optimized vImage for histogram and pixel analysis
// =============================================================================

// Structure to hold histogram analysis results
struct HistogramAnalysis {
  int uniqueBins;           // Number of occupied histogram bins (out of 256)
  float midtoneRatio;       // Ratio of pixels in midtone range (30-225)
  float nonWhiteRatio;      // Ratio of non-white pixels (< 245)
  double mean;              // Mean gray value
  double stdDev;            // Standard deviation
  int occupiedCoarseBins;   // Number of occupied bins when quantized to 16
};

// Compute histogram using Accelerate framework (SIMD optimized)
// This is MUCH faster than manual pixel loops on Apple Silicon
static HistogramAnalysis computeHistogramAccelerated(const QImage& image, const QRectF& region) {
  HistogramAnalysis result = {0, 0.0f, 0.0f, 0.0, 0.0, 0};

  if (image.isNull() || region.isEmpty()) {
    return result;
  }

  // Clamp region to image bounds
  int x1 = qMax(0, static_cast<int>(region.left()));
  int y1 = qMax(0, static_cast<int>(region.top()));
  int x2 = qMin(image.width(), static_cast<int>(region.right()));
  int y2 = qMin(image.height(), static_cast<int>(region.bottom()));

  int width = x2 - x1;
  int height = y2 - y1;

  if (width <= 0 || height <= 0) {
    return result;
  }

  // Convert region to grayscale buffer for vImage
  // We'll process it in one go rather than sampling
  std::vector<uint8_t> grayBuffer(width * height);

  // Fill grayscale buffer - this is still a loop but just for data extraction
  // The heavy lifting (histogram) will be done by Accelerate
  int idx = 0;
  for (int y = y1; y < y2; y++) {
    for (int x = x1; x < x2; x++) {
      grayBuffer[idx++] = static_cast<uint8_t>(qGray(image.pixel(x, y)));
    }
  }

  // Set up vImage buffer
  vImage_Buffer srcBuffer;
  srcBuffer.data = grayBuffer.data();
  srcBuffer.width = width;
  srcBuffer.height = height;
  srcBuffer.rowBytes = width;

  // Compute histogram using vImage (SIMD optimized)
  vImagePixelCount histogram[256] = {0};
  vImage_Error err = vImageHistogramCalculation_Planar8(&srcBuffer, histogram, kvImageNoFlags);

  if (err != kvImageNoError) {
    // Fallback to manual calculation if vImage fails
    qDebug() << "vImage histogram failed, falling back to manual";
    return result;
  }

  // Analyze histogram
  vImagePixelCount totalPixels = width * height;
  vImagePixelCount midtonePixels = 0;
  vImagePixelCount nonWhitePixels = 0;
  double sum = 0;
  double sumSq = 0;

  for (int i = 0; i < 256; i++) {
    if (histogram[i] > 0) {
      result.uniqueBins++;

      // Count midtones (30-225)
      if (i > 30 && i < 225) {
        midtonePixels += histogram[i];
      }

      // Count non-white (< 245)
      if (i < 245) {
        nonWhitePixels += histogram[i];
      }

      // For mean and stddev
      sum += static_cast<double>(i) * histogram[i];
      sumSq += static_cast<double>(i) * i * histogram[i];
    }
  }

  // Coarse bins (16 bins for compatibility with existing logic)
  for (int bin = 0; bin < 16; bin++) {
    vImagePixelCount binCount = 0;
    for (int i = bin * 16; i < (bin + 1) * 16 && i < 256; i++) {
      binCount += histogram[i];
    }
    if (binCount > totalPixels / 50) {  // At least 2% of pixels
      result.occupiedCoarseBins++;
    }
  }

  result.midtoneRatio = static_cast<float>(midtonePixels) / totalPixels;
  result.nonWhiteRatio = static_cast<float>(nonWhitePixels) / totalPixels;
  result.mean = sum / totalPixels;
  result.stdDev = sqrt((sumSq / totalPixels) - (result.mean * result.mean));

  return result;
}

// Fast colorfulness calculation using Accelerate
// Computes the RG/YB color opponent model
static float computeColorfulnessAccelerated(const QImage& image, const QRectF& region) {
  if (image.isNull() || region.isEmpty()) {
    return 0.0f;
  }

  int x1 = qMax(0, static_cast<int>(region.left()));
  int y1 = qMax(0, static_cast<int>(region.top()));
  int x2 = qMin(image.width(), static_cast<int>(region.right()));
  int y2 = qMin(image.height(), static_cast<int>(region.bottom()));

  int width = x2 - x1;
  int height = y2 - y1;

  if (width <= 0 || height <= 0) {
    return 0.0f;
  }

  // For colorfulness, we need RGB channels
  // Use vDSP for fast statistics
  size_t pixelCount = width * height;
  std::vector<float> rgDiff(pixelCount);
  std::vector<float> ybDiff(pixelCount);

  size_t idx = 0;
  for (int y = y1; y < y2; y++) {
    for (int x = x1; x < x2; x++) {
      QRgb pixel = image.pixel(x, y);
      float r = qRed(pixel);
      float g = qGreen(pixel);
      float b = qBlue(pixel);

      rgDiff[idx] = r - g;
      ybDiff[idx] = 0.5f * (r + g) - b;
      idx++;
    }
  }

  // Use vDSP to compute mean and standard deviation
  float rgMean = 0, rgStdDev = 0;
  float ybMean = 0, ybStdDev = 0;

  // Mean
  vDSP_meanv(rgDiff.data(), 1, &rgMean, pixelCount);
  vDSP_meanv(ybDiff.data(), 1, &ybMean, pixelCount);

  // Compute variance using vDSP
  // variance = mean(x^2) - mean(x)^2
  float rgMeanSq = 0, ybMeanSq = 0;
  vDSP_measqv(rgDiff.data(), 1, &rgMeanSq, pixelCount);
  vDSP_measqv(ybDiff.data(), 1, &ybMeanSq, pixelCount);

  float rgVar = rgMeanSq - rgMean * rgMean;
  float ybVar = ybMeanSq - ybMean * ybMean;

  rgStdDev = sqrtf(qMax(0.0f, rgVar));
  ybStdDev = sqrtf(qMax(0.0f, ybVar));

  // Colorfulness metric
  float colorfulness = sqrtf(rgStdDev * rgStdDev + ybStdDev * ybStdDev) +
                       0.3f * sqrtf(rgMean * rgMean + ybMean * ybMean);

  return qMin(colorfulness / 100.0f, 1.0f);
}

// =============================================================================
// END ACCELERATE FRAMEWORK OPTIMIZED FUNCTIONS
// =============================================================================

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

QVector<QRectF> AppleVisionDetector::detectFaces(const QImage& image) {
  QVector<QRectF> faces;

  if (!isAvailable() || image.isNull()) {
    return faces;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return faces;
    }

    if (@available(macOS 10.15, *)) {
      VNDetectFaceRectanglesRequest* request = [[VNDetectFaceRectanglesRequest alloc] init];

      NSArray* results = performVisionRequest(request, cgImage);
      CGImageRelease(cgImage);

      qreal imgWidth = image.width();
      qreal imgHeight = image.height();

      for (VNFaceObservation* observation in results) {
        CGRect bbox = observation.boundingBox;
        QRectF faceRect(
            bbox.origin.x * imgWidth,
            (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,
            bbox.size.width * imgWidth,
            bbox.size.height * imgHeight);
        faces.append(faceRect);
      }
    }
  }

  return faces;
}

// Helper function to calculate colorfulness of a region
// NOW USES ACCELERATE FRAMEWORK for faster computation via vDSP
static float calculateColorfulness(const QImage& image, const QRectF& region) {
  // Use the Accelerate-optimized version
  return computeColorfulnessAccelerated(image, region);
}

// Helper function to detect if a region is a continuous-tone image (photo/grayscale photo)
// This works even for grayscale images where colorfulness would be near zero
// NOW USES ACCELERATE FRAMEWORK for faster histogram computation
static bool isContinuousToneRegion(const QImage& image, const QRectF& region) {
  if (image.isNull() || region.isEmpty()) {
    return false;
  }

  // Use Accelerate-optimized histogram computation
  HistogramAnalysis hist = computeHistogramAccelerated(image, region);

  if (hist.uniqueBins < 5) {
    // Too few unique values - definitely not a photo
    return false;
  }

  // Map the accelerated results to our detection criteria
  // uniqueBins is now out of 256 (full histogram), so scale accordingly
  // A photo typically has 50+ unique gray levels
  int numUniqueLevels = hist.uniqueBins / 8;  // Scale to match old 32-bin logic

  qDebug() << "isContinuousTone (accelerated): uniqueLevels=" << numUniqueLevels
           << "midtoneRatio=" << hist.midtoneRatio
           << "stdDev=" << hist.stdDev
           << "occupiedBins=" << hist.occupiedCoarseBins;

  // A continuous-tone image typically has:
  // - Many unique gray levels (photos have 10+ quantized levels out of 32)
  // - Histogram spread across multiple bins
  // - May have low midtones if high-contrast (like silhouettes)

  bool hasRichTones = numUniqueLevels >= 12;  // Relaxed from 15 to catch more photos
  bool hasMidtones = hist.midtoneRatio > 0.15;     // Relaxed from 0.25 for high-contrast photos
  bool spreadHistogram = hist.occupiedCoarseBins >= 6;   // At least 6 of 16 bins occupied
  bool hasVariance = hist.stdDev > 20.0;  // Significant tonal variation

  qDebug() << "  -> hasRichTones=" << hasRichTones << "hasMidtones=" << hasMidtones
           << "spreadHistogram=" << spreadHistogram << "hasVariance=" << hasVariance;

  // DETECTION CRITERIA:
  // Method 1: Traditional - rich tones AND midtones (works for most photos)
  // Method 2: Variance-based - high stddev AND spread histogram AND midtones
  //           Removed hasRichTones-only path because text on white paper has
  //           high variance and rich tones but no midtones - that's NOT a photo!

  bool traditionalPhoto = hasRichTones && hasMidtones;
  // Variance-based now requires SOME midtones to avoid false positives on text pages
  // Text pages have high variance (dark text on white) but very low midtones
  bool variancePhoto = hasVariance && spreadHistogram && hasRichTones && (hasMidtones || hist.midtoneRatio > 0.08);

  qDebug() << "  -> traditionalPhoto=" << traditionalPhoto << "variancePhoto=" << variancePhoto;

  return traditionalPhoto || variancePhoto;
}

// Helper: Check if a rectangle contains a photo (has tonal variety, not just text/solid)
// NOW USES ACCELERATE FRAMEWORK for faster analysis
static bool rectangleContainsPhoto(const QImage& image, const QRectF& rect) {
  if (image.isNull() || rect.isEmpty()) return false;

  // Use accelerated histogram analysis
  HistogramAnalysis hist = computeHistogramAccelerated(image, rect);

  if (hist.uniqueBins < 10) return false;  // Need at least some tonal variety

  // Scale uniqueBins to match old 16-bin logic
  int numBins = hist.uniqueBins / 16;

  qDebug() << "Rectangle check (accelerated): bins=" << numBins << "midtoneRatio=" << hist.midtoneRatio;

  // A photo region should have at least 4 gray level bins and 10% midtones
  return numBins >= 4 && hist.midtoneRatio > 0.10f;
}

// NEW APPROACH: Find regions with visual content but NO text
// This is how OCR scanners identify embedded images - by exclusion
// NOW USES ACCELERATE FRAMEWORK for faster cell analysis
static QVector<QRectF> findNonTextRegionsWithContent(const QImage& image, const QVector<AppleVisionDetector::TextRegion>& textRegions) {
  QVector<QRectF> nonTextRegions;

  const int imgWidth = image.width();
  const int imgHeight = image.height();

  // Divide the page into a grid and check each cell
  // Use 8x8 grid for reasonable granularity
  const int gridCols = 8;
  const int gridRows = 8;
  const int cellWidth = imgWidth / gridCols;
  const int cellHeight = imgHeight / gridRows;

  // For each cell, calculate:
  // 1. How much text coverage it has
  // 2. How much visual content it has (non-white pixels with tonal variation)

  for (int row = 0; row < gridRows; row++) {
    for (int col = 0; col < gridCols; col++) {
      QRectF cell(col * cellWidth, row * cellHeight, cellWidth, cellHeight);

      // Calculate text coverage in this cell
      qreal textArea = 0;
      for (const AppleVisionDetector::TextRegion& textReg : textRegions) {
        QRectF intersection = cell.intersected(textReg.bounds);
        textArea += intersection.width() * intersection.height();
      }
      qreal textCoverage = textArea / (cellWidth * cellHeight);

      // If cell has significant text (>30%), skip it
      if (textCoverage > 0.30) {
        continue;
      }

      // Use Accelerate-optimized histogram for cell analysis
      HistogramAnalysis hist = computeHistogramAccelerated(image, cell);

      if (hist.uniqueBins < 5) continue;  // Too few values

      // Scale uniqueBins to match old 8-bin logic
      int numBins = hist.uniqueBins / 32;

      // A photo cell has:
      // - Significant non-white content (>20%)
      // - Multiple gray levels (>2 bins) OR midtones (not just black text on white)
      // - Little or no text coverage
      if (hist.nonWhiteRatio > 0.20 && (numBins >= 3 || hist.midtoneRatio > 0.15)) {
        nonTextRegions.append(cell);
      }
    }
  }

  // Merge adjacent cells into larger regions
  // (Simple approach: return individual cells and let caller merge or use as-is)
  return nonTextRegions;
}

QVector<AppleVisionDetector::ImageRegion> AppleVisionDetector::detectImageRegions(
    const QImage& image, const QVector<TextRegion>& textRegions) {
  QVector<ImageRegion> imageRegions;

  if (!isAvailable() || image.isNull()) {
    return imageRegions;
  }

  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (!cgImage) {
      return imageRegions;
    }

    qreal imgWidth = image.width();
    qreal imgHeight = image.height();
    qreal imageArea = imgWidth * imgHeight;

    // Strategy 1: Detect faces - faces indicate photos
    QVector<QRectF> faces = detectFaces(image);

    // NEW Strategy 2: Find regions with visual content but no text (OCR-style)
    // This is the most reliable way to find embedded images!
    QVector<QRectF> nonTextRegions = findNonTextRegionsWithContent(image, textRegions);
    if (!nonTextRegions.isEmpty()) {
      qDebug() << "Found" << nonTextRegions.size() << "non-text regions with visual content";

      // If we have multiple adjacent cells, merge them into photo regions
      // For now, check if the cells cluster together (>= 2 adjacent cells = likely photo)
      QRectF mergedBounds;
      for (const QRectF& cell : nonTextRegions) {
        if (mergedBounds.isEmpty()) {
          mergedBounds = cell;
        } else {
          mergedBounds = mergedBounds.united(cell);
        }
      }

      // If merged area is significant (>5% of page) but NOT the whole page, it's likely a photo
      // Regions covering >80% of the page are NOT embedded photos - that's just the page itself
      // with gaps between text regions. Also require some colorfulness to be a real photo.
      qreal mergedArea = mergedBounds.width() * mergedBounds.height();
      float regionColorfulness = calculateColorfulness(image, mergedBounds);
      qreal coverageRatio = mergedArea / imageArea;
      if (coverageRatio > 0.05 && coverageRatio < 0.80 && nonTextRegions.size() >= 2 && regionColorfulness > 0.02f) {
        qDebug() << "Detected photo region from non-text cells:" << mergedBounds
                 << "area%:" << (100.0 * coverageRatio)
                 << "colorfulness:" << regionColorfulness;

        ImageRegion imgRegion;
        imgRegion.bounds = mergedBounds;
        imgRegion.confidence = 0.8f;
        imgRegion.hasFace = false;
        imgRegion.colorfulness = regionColorfulness;
        imageRegions.append(imgRegion);
      } else if (coverageRatio >= 0.80) {
        qDebug() << "Skipping non-text region detection: covers" << (100.0 * coverageRatio)
                 << "% of page (likely just text gaps, not a photo)";
      }
    }

    // Strategy 3: Use RECTANGLE DETECTION to find potential photo frames
    // This catches photos with clear borders
    if (@available(macOS 10.15, *)) {
      VNDetectRectanglesRequest* rectRequest = [[VNDetectRectanglesRequest alloc] init];
      rectRequest.minimumSize = 0.05f;  // At least 5% of image dimension
      rectRequest.maximumObservations = 10;  // Find up to 10 rectangles
      rectRequest.minimumConfidence = 0.5f;
      rectRequest.minimumAspectRatio = 0.3f;
      rectRequest.maximumAspectRatio = 3.0f;

      NSArray* rectResults = performVisionRequest(rectRequest, cgImage);

      for (VNRectangleObservation* rectObs in rectResults) {
        CGRect bbox = rectObs.boundingBox;
        QRectF regionRect(
            bbox.origin.x * imgWidth,
            (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,
            bbox.size.width * imgWidth,
            bbox.size.height * imgHeight);

        // Skip rectangles that are too small (< 3% of page) or too large (> 80%)
        qreal regionArea = regionRect.width() * regionRect.height();
        if (regionArea / imageArea < 0.03 || regionArea / imageArea > 0.80) {
          continue;
        }

        qDebug() << "Found rectangle:" << regionRect << "confidence:" << rectObs.confidence
                 << "area%:" << (100.0 * regionArea / imageArea);

        // Check if rectangle overlaps too much with text (it's probably a text block)
        qreal textOverlap = 0;
        for (const TextRegion& textReg : textRegions) {
          QRectF intersection = regionRect.intersected(textReg.bounds);
          textOverlap += intersection.width() * intersection.height();
        }
        if (textOverlap / regionArea > 0.5) {
          qDebug() << "  -> Skipping: too much text overlap";
          continue;
        }

        // Check if this rectangle contains photo content (tonal variety)
        if (rectangleContainsPhoto(image, regionRect)) {
          ImageRegion imgRegion;
          imgRegion.bounds = regionRect;
          imgRegion.confidence = rectObs.confidence;
          imgRegion.hasFace = false;
          imgRegion.colorfulness = calculateColorfulness(image, regionRect);
          imageRegions.append(imgRegion);
          qDebug() << "  -> DETECTED as photo region!";
        }
      }
    }

    // Strategy 3: Use saliency detection to find visually interesting regions
    if (@available(macOS 10.15, *)) {
      VNGenerateAttentionBasedSaliencyImageRequest* saliencyRequest =
          [[VNGenerateAttentionBasedSaliencyImageRequest alloc] init];

      NSArray* saliencyResults = performVisionRequest(saliencyRequest, cgImage);

      for (VNSaliencyImageObservation* observation in saliencyResults) {
        // Get salient regions
        NSArray<VNRectangleObservation*>* salientObjects = observation.salientObjects;

        for (VNRectangleObservation* salientObj in salientObjects) {
          CGRect bbox = salientObj.boundingBox;
          QRectF regionRect(
              bbox.origin.x * imgWidth,
              (1.0 - bbox.origin.y - bbox.size.height) * imgHeight,
              bbox.size.width * imgWidth,
              bbox.size.height * imgHeight);

          // Skip regions that are too small (less than 2% of image)
          if ((regionRect.width() * regionRect.height()) / imageArea < 0.02) {
            continue;
          }

          // Check if this region overlaps significantly with text regions
          bool isTextRegion = false;
          for (const TextRegion& textReg : textRegions) {
            QRectF intersection = regionRect.intersected(textReg.bounds);
            if (!intersection.isEmpty()) {
              qreal overlapRatio = (intersection.width() * intersection.height()) /
                                   (regionRect.width() * regionRect.height());
              if (overlapRatio > 0.5) {
                isTextRegion = true;
                break;
              }
            }
          }

          if (isTextRegion) {
            continue;
          }

          // Calculate colorfulness of this region
          float colorfulness = calculateColorfulness(image, regionRect);

          // Check if any faces are in this region
          bool hasFace = false;
          for (const QRectF& face : faces) {
            if (regionRect.intersects(face)) {
              hasFace = true;
              break;
            }
          }

          // For large salient regions, also check continuous-tone (catches grayscale photos)
          // Only do continuous-tone check for regions covering >10% of image to avoid
          // false positives from paper texture in small regions
          bool isContinuousTone = false;
          qreal regionArea = regionRect.width() * regionRect.height();
          if (regionArea / imageArea > 0.10) {
            isContinuousTone = isContinuousToneRegion(image, regionRect);
          }

          // Check if region has visual content (not mostly blank/white)
          // Simple check: if the region has significant non-white pixels, it has content
          bool hasVisualContent = false;
          if (regionArea / imageArea > 0.05) {  // Only check for regions > 5% of page
            int nonWhitePixels = 0;
            int sampledPixels = 0;
            int rx1 = qMax(0, static_cast<int>(regionRect.left()));
            int ry1 = qMax(0, static_cast<int>(regionRect.top()));
            int rx2 = qMin(image.width() - 1, static_cast<int>(regionRect.right()));
            int ry2 = qMin(image.height() - 1, static_cast<int>(regionRect.bottom()));
            int step = qMax(1, static_cast<int>((rx2 - rx1) * (ry2 - ry1) / 500));

            for (int y = ry1; y <= ry2; y += qMax(1, step / (rx2 - rx1 + 1))) {
              for (int x = rx1; x <= rx2; x += step) {
                int gray = qGray(image.pixel(x, y));
                if (gray < 240) {  // Not white
                  nonWhitePixels++;
                }
                sampledPixels++;
              }
            }

            // If more than 20% of pixels are non-white, it has visual content
            if (sampledPixels > 0 && static_cast<float>(nonWhitePixels) / sampledPixels > 0.20f) {
              hasVisualContent = true;
              qDebug() << "Region has visual content: " << (100.0f * nonWhitePixels / sampledPixels) << "% non-white";
            }
          }

          // Consider it an image region if:
          // - it's colorful (>15%) OR has a face OR
          // - it's continuous-tone WITH some color variation (>2%) - pure zero colorfulness
          //   is likely paper texture, not a grayscale photo
          // Note: Removed hasVisualContent check - text IS visual content and was causing
          // false positives on text documents (99% non-white = text, not photos!)
          // Note: Removed standalone isContinuousTone check because text pages with
          // high variance were triggering false positives
          bool isContinuousTonePhoto = isContinuousTone && colorfulness > 0.02f;
          if (colorfulness > 0.15f || hasFace || isContinuousTonePhoto) {
            ImageRegion imgRegion;
            imgRegion.bounds = regionRect;
            imgRegion.confidence = salientObj.confidence;
            imgRegion.hasFace = hasFace;
            imgRegion.colorfulness = colorfulness;
            imageRegions.append(imgRegion);

            qDebug() << "Detected image region:" << regionRect
                     << "colorfulness:" << colorfulness
                     << "hasFace:" << hasFace
                     << "isContinuousTone:" << isContinuousTone;
          }
        }
      }
    }

    // Also add face regions that weren't captured by saliency
    for (const QRectF& face : faces) {
      bool alreadyIncluded = false;
      for (const ImageRegion& existing : imageRegions) {
        if (existing.bounds.intersects(face)) {
          alreadyIncluded = true;
          break;
        }
      }
      if (!alreadyIncluded) {
        // Expand face region to likely photo bounds (faces are typically 20-40% of photo)
        qreal expandFactor = 2.5;
        qreal cx = face.center().x();
        qreal cy = face.center().y();
        qreal newWidth = face.width() * expandFactor;
        qreal newHeight = face.height() * expandFactor;
        QRectF expandedRegion(
            qMax(0.0, cx - newWidth / 2),
            qMax(0.0, cy - newHeight / 2),
            qMin(newWidth, imgWidth - (cx - newWidth / 2)),
            qMin(newHeight, imgHeight - (cy - newHeight / 2)));

        ImageRegion imgRegion;
        imgRegion.bounds = expandedRegion;
        imgRegion.confidence = 0.9f;  // High confidence for face-based detection
        imgRegion.hasFace = true;
        imgRegion.colorfulness = calculateColorfulness(image, expandedRegion);
        imageRegions.append(imgRegion);

        qDebug() << "Detected face-based image region:" << expandedRegion;
      }
    }

    CGImageRelease(cgImage);
  }

  return imageRegions;
}

// Use Apple's ML-based image classifier to detect if image is a photo
// Strategy: Photos get confident visual classifications (objects, scenes, etc.)
// Text documents don't get confident visual classifications
// So we check if ANY visual category has high confidence
static bool isPhotoUsingMLClassifier(CGImageRef cgImage) {
  if (@available(macOS 10.15, *)) {
    VNClassifyImageRequest* classifyRequest = [[VNClassifyImageRequest alloc] init];

    NSArray* results = performVisionRequest(classifyRequest, cgImage);

    // Track if we found any confident visual classification
    float maxConfidence = 0.0f;
    NSString* bestCategory = nil;

    // Categories that indicate text/document (NOT a photo)
    // These are the only things we explicitly exclude
    NSArray* textIndicators = @[@"text", @"document", @"writing", @"letter",
                                 @"book", @"page", @"paper", @"print"];

    // Categories that STRONGLY indicate photos even at lower confidence
    // If we detect people/faces/animals, there's almost certainly a photo
    NSArray* strongPhotoIndicators = @[@"people", @"adult", @"child", @"person",
                                        @"face", @"portrait", @"crowd", @"animal",
                                        @"dog", @"cat", @"bird", @"horse"];

    bool foundStrongPhotoIndicator = false;
    float strongIndicatorConfidence = 0.0f;
    NSString* strongIndicatorCategory = nil;

    for (VNClassificationObservation* observation in results) {
      NSString* identifier = observation.identifier;
      float confidence = observation.confidence;

      // Log top classifications for debugging
      if (confidence > 0.1) {
        qDebug() << "ML Classification:" << QString::fromNSString(identifier)
                 << "confidence:" << confidence;
      }

      // Check for strong photo indicators with LOWER threshold (0.25)
      // Finding people/faces/animals at even moderate confidence means photo content
      for (NSString* photoWord in strongPhotoIndicators) {
        if ([identifier.lowercaseString containsString:photoWord] && confidence > 0.25f) {
          if (confidence > strongIndicatorConfidence) {
            foundStrongPhotoIndicator = true;
            strongIndicatorConfidence = confidence;
            strongIndicatorCategory = identifier;
          }
          break;
        }
      }

      // Skip text/document indicators - these suggest it's NOT a photo
      bool isTextIndicator = false;
      for (NSString* textWord in textIndicators) {
        if ([identifier.lowercaseString containsString:textWord]) {
          isTextIndicator = true;
          break;
        }
      }

      if (!isTextIndicator && confidence > maxConfidence) {
        maxConfidence = confidence;
        bestCategory = identifier;
      }
    }

    // FIRST: Check for strong photo indicators (people, faces, animals) at lower threshold
    // This catches photos embedded in documents where overall confidence is lower
    // BUT: If document indicators are also strong, the strong indicator must significantly beat them
    // to be considered a photo. A document with small illustrations isn't a photo.
    float documentConfidence = 0.0f;
    for (VNClassificationObservation* observation in results) {
      for (NSString* textWord in textIndicators) {
        if ([observation.identifier.lowercaseString containsString:textWord]) {
          documentConfidence = qMax(documentConfidence, observation.confidence);
        }
      }
    }

    // Strong indicator must beat document confidence by 0.2 (20%) to be considered a photo
    // This prevents "document 63% + people 60%" from being classified as photo
    if (foundStrongPhotoIndicator && (strongIndicatorConfidence > documentConfidence + 0.2f)) {
      qDebug() << "ML Classifier: Detected as PHOTO based on STRONG indicator:"
               << QString::fromNSString(strongIndicatorCategory) << "(" << strongIndicatorConfidence << ")"
               << "beats document confidence:" << documentConfidence;
      return true;
    } else if (foundStrongPhotoIndicator) {
      qDebug() << "ML Classifier: Strong indicator" << QString::fromNSString(strongIndicatorCategory)
               << "(" << strongIndicatorConfidence << ") NOT strong enough vs document ("
               << documentConfidence << ") - treating as document";
    }

    // SECOND: If we found ANY visual category with confidence > 0.4, it's likely a photo
    // Photos of whales, turnips, architecture - all will trigger some visual category
    // Text pages typically don't trigger confident visual classifications
    // BUT: Skip this check if we already evaluated a strong indicator and rejected it -
    // we don't want to double-count the same detection through a different path
    if (!foundStrongPhotoIndicator && maxConfidence > 0.4f && bestCategory != nil) {
      qDebug() << "ML Classifier: Detected as PHOTO based on:"
               << QString::fromNSString(bestCategory) << "(" << maxConfidence << ")";
      return true;
    }

    qDebug() << "ML Classifier: No confident visual classification (max:" << maxConfidence << ")";
  }
  return false;
}

/**
 * Detect if image has meaningful tonal content (gradients, shading)
 * that should be preserved even if the image is "high contrast" overall.
 *
 * Key distinction:
 * - Line art/text: pixel values cluster around 0 and 255 with nothing in between
 * - Tonal illustration: values spread across the range (meaningful midtones)
 */
static bool hasTonalContent(const QImage& image) {
  // Sample ~10,000 pixels for performance
  const int totalPixels = image.width() * image.height();
  const int step = qMax(1, totalPixels / 10000);

  // Build a coarse histogram (32 bins) for the midtone range
  int midtoneBins[32] = {0};
  int midtoneCount = 0;
  int totalSampled = 0;

  // Also count gradual transitions (gradient detection)
  int gradualTransitions = 0;
  int edgeTransitions = 0;

  for (int i = 0; i < totalPixels; i += step) {
    int x = i % image.width();
    int y = i / image.width();
    int gray = qGray(image.pixel(x, y));
    totalSampled++;

    // Count midtones (50-200, excluding near-black/white)
    if (gray >= 50 && gray <= 200) {
      midtoneCount++;
      int bin = (gray - 50) * 32 / 151;  // Map 50-200 to 0-31
      midtoneBins[bin]++;
    }

    // Gradient detection: compare with neighbor to the right
    if (x + step < image.width()) {
      int neighborGray = qGray(image.pixel(x + step, y));
      int diff = qAbs(gray - neighborGray);
      if (diff >= 5 && diff <= 30) {
        gradualTransitions++;  // Smooth gradient
      } else if (diff > 80) {
        edgeTransitions++;  // Sharp edge
      }
    }
  }

  if (totalSampled == 0) {
    return false;
  }

  // Check 1: Minimum midtone presence
  float midtoneRatio = static_cast<float>(midtoneCount) / totalSampled;
  if (midtoneRatio < 0.03f) {
    return false;  // Too few midtones to matter
  }

  // Check 2: Count distinct gray levels in midtone range
  int distinctLevels = 0;
  for (int i = 0; i < 32; i++) {
    if (midtoneBins[i] > totalSampled / 500) {  // At least 0.2% of pixels
      distinctLevels++;
    }
  }

  // Check 3: Gradient ratio (gradual vs sharp transitions)
  int totalTransitions = gradualTransitions + edgeTransitions;
  float gradientRatio = (totalTransitions > 0)
      ? static_cast<float>(gradualTransitions) / totalTransitions
      : 0.0f;

  qDebug() << "hasTonalContent: midtoneRatio=" << midtoneRatio
           << "distinctLevels=" << distinctLevels
           << "gradientRatio=" << gradientRatio;

  // Decision: tonal content if:
  // - Significant midtones (>5%) AND multiple gray levels (6+)
  // - OR moderate midtones (>3%) AND high gradient ratio (>30%)
  if (midtoneRatio > 0.05f && distinctLevels >= 6) {
    qDebug() << "hasTonalContent: TRUE (midtones + distinct levels)";
    return true;
  }
  if (midtoneRatio > 0.03f && gradientRatio > 0.30f) {
    qDebug() << "hasTonalContent: TRUE (midtones + gradients)";
    return true;
  }

  qDebug() << "hasTonalContent: FALSE";
  return false;
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyze(const QImage& image) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.imageCoverage = 0.0f;
  result.overallColorfulness = 0.0f;
  result.isHighContrast = false;
  result.hasEmbeddedImages = false;
  result.hasTonalContent = false;
  result.documentBounds.detected = false;

  if (image.isNull()) {
    return result;
  }

  // FIRST: Use Apple's ML classifier to detect if this is a photo
  // This is the most reliable method - trained on millions of images
  @autoreleasepool {
    CGImageRef cgImage = createCGImageFromQImage(image);
    if (cgImage) {
      bool mlDetectedPhoto = isPhotoUsingMLClassifier(cgImage);
      if (mlDetectedPhoto) {
        qDebug() << "ML CLASSIFIER DETECTED PHOTO - marking hasEmbeddedImages=true";
        result.hasEmbeddedImages = true;
        result.imageCoverage = 1.0f;
        result.contentType = ContentType::Photo;
      }
      CGImageRelease(cgImage);
    }
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

  // Detect image/photo regions within the document
  result.imageRegions = detectImageRegions(image, result.textRegions);

  // Calculate image coverage from detected regions (but preserve ML result if higher)
  qreal totalImageArea = 0;
  for (const ImageRegion& region : result.imageRegions) {
    totalImageArea += region.bounds.width() * region.bounds.height();
  }
  float regionBasedCoverage = static_cast<float>(totalImageArea / imageArea);
  // Keep the higher of ML-based coverage (1.0 if photo detected) and region-based
  result.imageCoverage = qMax(result.imageCoverage, regionBasedCoverage);
  // Preserve ML classifier result - only set to true if imageRegions found, never reset to false
  if (!result.imageRegions.isEmpty()) {
    result.hasEmbeddedImages = true;
  }
  // Note: hasEmbeddedImages may already be true from ML classifier above

  // Check if the entire image is a photograph (colorful or continuous-tone grayscale)
  // Run this check even with moderate text coverage - photos can have captions, signs, etc.
  bool isOverallContinuousTone = false;

  // Always check the full image for continuous-tone characteristics
  QRectF wholeImage(0, 0, image.width(), image.height());
  result.overallColorfulness = calculateColorfulness(image, wholeImage);
  isOverallContinuousTone = isContinuousToneRegion(image, wholeImage);

  qDebug() << "Full-page analysis: colorfulness=" << result.overallColorfulness
           << "isContinuousTone=" << isOverallContinuousTone
           << "textCoverage=" << result.textCoverage
           << "hasEmbeddedImages(before)=" << result.hasEmbeddedImages;

  // If we haven't already detected embedded images, check if the full page is a photo
  if (!result.hasEmbeddedImages && result.textCoverage < 0.20f) {
    // For colorful images, use a moderate colorfulness threshold
    // For grayscale photos, rely on continuous-tone detection
    // Allow continuous-tone photos even with up to 10% text (captions, signs in photos)
    if (result.overallColorfulness > 0.10f || (isOverallContinuousTone && result.textCoverage < 0.10f)) {
      result.hasEmbeddedImages = true;  // Treat the whole page as an image
      result.imageCoverage = 1.0f;
      qDebug() << "Full-page photo detected! colorful=" << (result.overallColorfulness > 0.10f)
               << "continuousTone=" << isOverallContinuousTone;
    }
  }

  qDebug() << "Vision analysis: textCoverage=" << result.textCoverage
           << "imageCoverage=" << result.imageCoverage
           << "imageRegions=" << result.imageRegions.size()
           << "hasEmbeddedImages=" << result.hasEmbeddedImages;

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

  // Detect tonal content (gradients/shading that should be preserved even if high contrast)
  result.hasTonalContent = hasTonalContent(image);
  qDebug() << "analyze: hasTonalContent=" << result.hasTonalContent;

  // Determine content type
  // Priority: High text coverage = Document (regardless of hasEmbeddedImages false positives)
  // This is critical: hasEmbeddedImages often has false positives from ML classifier
  if (result.textCoverage > 0.30f) {
    // High text coverage = Document, regardless of hasEmbeddedImages
    result.contentType = ContentType::Document;
  } else if (result.hasEmbeddedImages && result.imageCoverage > 0.15f && result.textCoverage > 0.1f) {
    // Document with SIGNIFICANT embedded images (imageCoverage > 15%) -> Mixed
    result.contentType = ContentType::Mixed;
  } else if (result.textCoverage > 0.15f) {
    // Moderate text, low/no images -> Mixed (but could still be B&W if high contrast)
    result.contentType = ContentType::Mixed;
  } else if (result.isHighContrast && result.textCoverage < 0.05f && !result.hasEmbeddedImages) {
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
  // Find the maximum colorfulness among all detected image regions
  float maxRegionColorfulness = 0.0f;
  for (const ImageRegion& region : result.imageRegions) {
    if (region.colorfulness > maxRegionColorfulness) {
      maxRegionColorfulness = region.colorfulness;
    }
  }

  // Effective colorfulness: the maximum of overall and any region
  const float effectiveColorfulness = qMax(result.overallColorfulness, maxRegionColorfulness);

  qDebug() << "suggestColorMode: contentType=" << static_cast<int>(result.contentType)
           << "textCoverage=" << result.textCoverage
           << "imageCoverage=" << result.imageCoverage
           << "hasEmbeddedImages=" << result.hasEmbeddedImages
           << "isHighContrast=" << result.isHighContrast
           << "hasTonalContent=" << result.hasTonalContent
           << "overallColorfulness=" << result.overallColorfulness
           << "maxRegionColorfulness=" << maxRegionColorfulness
           << "effectiveColorfulness=" << effectiveColorfulness;

  // ============================================================================
  // DECISION LOGIC - Based on original simple logic that worked:
  // Use contentType and isHighContrast as primary signals (reliable)
  // Only use colorfulness for grayscale vs color photo distinction
  // ============================================================================

  // TONAL CONTENT OVERRIDE: Preserve grayscale for images with meaningful shading
  // This catches grayscale illustrations that would otherwise be binarized
  // (e.g., illustrations with captions that get classified as Document/Mixed)
  if (result.hasTonalContent && effectiveColorfulness < 0.08f) {
    qDebug() << "suggestColorMode: TONAL CONTENT detected, preserving grayscale";
    return QStringLiteral("grayscale");
  }

  // High-contrast documents → B&W (original working rule)
  // Only applies if no tonal content was detected above
  if (result.contentType == ContentType::Document && result.isHighContrast) {
    qDebug() << "suggestColorMode: HIGH-CONTRAST DOCUMENT, suggesting bw";
    return QStringLiteral("bw");
  }

  // Illustrations - preserve tones (user feedback: illustrations should never be B&W)
  if (result.contentType == ContentType::Illustration) {
    if (effectiveColorfulness < 0.08f) {
      qDebug() << "suggestColorMode: GRAYSCALE ILLUSTRATION, suggesting grayscale";
      return QStringLiteral("grayscale");
    } else {
      qDebug() << "suggestColorMode: COLOR ILLUSTRATION, suggesting color";
      return QStringLiteral("color");
    }
  }

  // Photos - distinguish grayscale vs color based on colorfulness
  if (result.contentType == ContentType::Photo) {
    if (effectiveColorfulness < 0.08f) {
      qDebug() << "suggestColorMode: GRAYSCALE PHOTO (colorfulness < 8%), suggesting grayscale";
      return QStringLiteral("grayscale");
    } else {
      qDebug() << "suggestColorMode: COLOR PHOTO, suggesting color";
      return QStringLiteral("color");
    }
  }

  // Mixed content - check if high contrast (likely text document with moderate coverage)
  if (result.contentType == ContentType::Mixed) {
    // If high contrast and low colorfulness, treat like a document -> B&W
    if (result.isHighContrast && effectiveColorfulness < 0.08f) {
      qDebug() << "suggestColorMode: HIGH-CONTRAST MIXED (likely text), suggesting bw";
      return QStringLiteral("bw");
    } else if (effectiveColorfulness < 0.08f) {
      qDebug() << "suggestColorMode: MIXED CONTENT (low color), suggesting grayscale";
      return QStringLiteral("grayscale");
    } else {
      qDebug() << "suggestColorMode: MIXED CONTENT (colorful), suggesting color";
      return QStringLiteral("color");
    }
  }

  // Documents without high contrast - check if mostly text
  if (result.contentType == ContentType::Document) {
    // If it's a document, default to grayscale to preserve subtle details
    // unless there's significant embedded image content
    if (result.hasEmbeddedImages && result.imageCoverage > 0.10f) {
      if (effectiveColorfulness < 0.08f) {
        qDebug() << "suggestColorMode: DOCUMENT with grayscale images, suggesting grayscale";
        return QStringLiteral("grayscale");
      } else {
        qDebug() << "suggestColorMode: DOCUMENT with color images, suggesting color";
        return QStringLiteral("color");
      }
    }
    qDebug() << "suggestColorMode: DOCUMENT (non high-contrast), suggesting grayscale";
    return QStringLiteral("grayscale");
  }

  // DEFAULT: grayscale (safe fallback)
  qDebug() << "suggestColorMode: UNCERTAIN, defaulting to color";
  return QStringLiteral("color");
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
  const qreal imageCenter = imageWidth / 2.0;

  // SMART FEATURE 1: Page number detection
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
  const qreal leftZoneStart = 0;
  const qreal leftZoneEnd = imageWidth * 0.48;
  const qreal rightZoneStart = imageWidth * 0.52;
  const qreal rightZoneEnd = imageWidth;
  const qreal gutterStart = imageWidth * 0.48;
  const qreal gutterEnd = imageWidth * 0.52;

  // Count regions in each zone and track their positions
  int leftCount = 0;
  int rightCount = 0;
  int gutterCount = 0;
  qreal leftTotalX = 0;  // For calculating average position
  qreal rightTotalX = 0;

  for (const TextRegion& region : regions) {
    const qreal regionCenterX = region.bounds.center().x();

    if (regionCenterX >= leftZoneStart && regionCenterX < leftZoneEnd) {
      leftCount++;
      leftTotalX += regionCenterX;
    } else if (regionCenterX >= rightZoneStart && regionCenterX <= rightZoneEnd) {
      rightCount++;
      rightTotalX += regionCenterX;
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

QVector<QRectF> AppleVisionDetector::detectFaces(const QImage&) {
  return QVector<QRectF>();
}

QVector<AppleVisionDetector::ImageRegion> AppleVisionDetector::detectImageRegions(
    const QImage&, const QVector<TextRegion>&) {
  return QVector<ImageRegion>();
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyze(const QImage&) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.imageCoverage = 0.0f;
  result.overallColorfulness = 0.0f;
  result.isHighContrast = false;
  result.hasEmbeddedImages = false;
  result.documentBounds.detected = false;
  return result;
}

AppleVisionDetector::AnalysisResult AppleVisionDetector::analyzeFromFile(const QString&) {
  AnalysisResult result;
  result.contentType = ContentType::Unknown;
  result.textCoverage = 0.0f;
  result.imageCoverage = 0.0f;
  result.overallColorfulness = 0.0f;
  result.isHighContrast = false;
  result.hasEmbeddedImages = false;
  result.documentBounds.detected = false;
  return result;
}

QString AppleVisionDetector::suggestColorMode(const AnalysisResult&) {
  return QStringLiteral("grayscale");
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

#endif  // Q_OS_MACOS
