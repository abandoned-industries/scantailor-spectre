// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_IMAGETYPEDETECTOR_H_
#define SCANTAILOR_CORE_IMAGETYPEDETECTOR_H_

#include <QString>

class QImage;

/**
 * Utility class for detecting image color characteristics.
 * Used by Output filter for auto-detecting appropriate color mode,
 * and by PDF exporter for optimal compression.
 */
class ImageTypeDetector {
 public:
  enum class Type { Mono, Grayscale, Color };

  /**
   * Detect the effective color type of an image.
   * Uses sampling for efficiency on large images.
   *
   * @param image The image to analyze
   * @return The detected image type
   */
  static Type detect(const QImage& image);

  /**
   * Detect the effective color type from an image file path.
   * Loads a downsampled version for efficiency.
   *
   * @param imagePath Path to the image file
   * @return The detected image type (defaults to Grayscale if loading fails)
   */
  static Type detectFromFile(const QString& imagePath);

  /**
   * Check if an image is effectively pure black and white.
   * @param image The image to check
   * @return true if only pure black (0) and white (255) pixels
   */
  static bool isEffectivelyMono(const QImage& image);

  /**
   * Check if an image is effectively grayscale.
   * Tolerates slight color variations (e.g., yellowed paper).
   *
   * @param image The image to check
   * @param tolerance Maximum allowed RGB channel difference (default 8)
   * @return true if RGB channels are within tolerance of each other
   */
  static bool isEffectivelyGrayscale(const QImage& image, int tolerance = 8);
};

#endif  // SCANTAILOR_CORE_IMAGETYPEDETECTOR_H_
