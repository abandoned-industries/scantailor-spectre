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
   * Detect the color type, compensating for an overall paper tint.
   *
   * If plain detection classifies the image as Color but the background
   * (brightest-quartile estimate) carries a significant cast - aged/toned
   * paper - the image is neutralized against that background color and
   * detection re-runs. The page is only classified Color if the color
   * fraction survives neutralization (a genuine color photo does; uniform
   * tan paper with black text does not).
   *
   * The extra pass only runs for Color-classified images whose background
   * has a cast, so already-neutralized or truly neutral pages pay nothing.
   *
   * @param image The image to analyze
   * @param midtoneThreshold Minimum midtone % to trigger region-based detection (default 10)
   * @return The detected color type
   */
  static ColorType detectWithCastCompensation(const QImage& image, int midtoneThreshold = 10);

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
