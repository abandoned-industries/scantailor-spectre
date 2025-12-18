// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_
#define SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

/**
 * Apple Vision Framework integration for page split detection.
 * Uses macOS Vision APIs for text recognition to detect two-page spreads.
 * Falls back to disabled on non-Apple platforms.
 */
class AppleVisionDetector {
 public:
  /**
   * Detected text region with confidence score.
   * Used internally for page split detection.
   */
  struct TextRegion {
    QRectF bounds;       // Bounding rectangle in image coordinates
    float confidence;    // Detection confidence (0.0 - 1.0)
    QString text;        // Recognized text (if available)
  };

  /**
   * Page split detection result.
   */
  struct PageSplitResult {
    bool shouldSplit;        // True if a two-page layout was detected
    double splitLineX;       // X coordinate of suggested split line (normalized 0.0-1.0)
    float confidence;        // Detection confidence (0.0 - 1.0)
    int leftTextRegions;     // Number of text regions on left side
    int rightTextRegions;    // Number of text regions on right side
  };

  /**
   * Check if Apple Vision is available on this platform.
   */
  static bool isAvailable();

  /**
   * Detect text regions in an image.
   * @param image The image to analyze
   * @return Vector of detected text regions
   */
  static QVector<TextRegion> detectTextRegions(const QImage& image);

  /**
   * Detect if an image contains two pages (like a book spread) and suggest split location.
   * Uses text region clustering to find natural page boundaries.
   * @param image The image to analyze
   * @return Page split detection result
   */
  static PageSplitResult detectPageSplit(const QImage& image);

  /**
   * Detect page split from file path (with automatic downscaling for performance).
   * @param imagePath Path to the image file
   * @return Page split detection result
   */
  static PageSplitResult detectPageSplitFromFile(const QString& imagePath);
};

#endif  // SCANTAILOR_CORE_APPLEVISIONDETECTOR_H_
