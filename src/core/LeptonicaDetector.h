// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_LEPTONICADETECTOR_H_
#define SCANTAILOR_CORE_LEPTONICADETECTOR_H_

#include <QString>

class QImage;

/**
 * Document color type detection using Leptonica library.
 * Uses connected component analysis and channel variance to properly
 * distinguish between B&W, grayscale, and color content.
 */
class LeptonicaDetector {
 public:
  enum class ColorType { BlackWhite, Grayscale, Color };

  /**
   * Detect the color type of an image.
   * Uses Leptonica's pixColorFraction() and channel analysis.
   *
   * @param image The image to analyze
   * @param midtoneThreshold Minimum midtone % to trigger region-based detection (default 10)
   * @return The detected color type
   */
  static ColorType detect(const QImage& image, int midtoneThreshold = 10);

  /**
   * Detect from file path (loads downsampled for efficiency).
   */
  static ColorType detectFromFile(const QString& imagePath);

  /**
   * Get string representation of color type for logging.
   */
  static const char* colorTypeToString(ColorType type);
};

#endif  // SCANTAILOR_CORE_LEPTONICADETECTOR_H_
