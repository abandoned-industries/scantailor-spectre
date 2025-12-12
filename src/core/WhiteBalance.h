// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_WHITEBALANCE_H_
#define SCANTAILOR_CORE_WHITEBALANCE_H_

#include <QColor>
#include <QImage>
#include <QRect>

/**
 * \brief White balance correction utilities for removing color casts from scanned pages.
 *
 * This is used to correct yellowed paper, warm lighting, or scanner color casts.
 * The correction is applied before color mode detection to ensure B&W pages
 * aren't misidentified as color due to paper discoloration.
 */
class WhiteBalance {
 public:
  /**
   * \brief Detect the paper/background color from an image.
   *
   * Uses an adaptive strategy:
   * 1. If significant margins exist outside contentBox, sample from those
   * 2. Otherwise, find bright low-saturation pixels (likely paper)
   *
   * \param image The source image
   * \param contentBox The content area (from Select Content stage)
   * \return The detected paper color, or invalid QColor if detection failed
   */
  static QColor detectPaperColor(const QImage& image, const QRect& contentBox);

  /**
   * \brief Apply white balance correction to make paperColor neutral.
   *
   * Calculates per-channel multipliers to shift paperColor toward neutral gray,
   * then applies those multipliers to all pixels.
   *
   * \param image The source image (must be RGB32 or ARGB32)
   * \param paperColor The color that should become neutral
   * \return The corrected image
   */
  static QImage apply(const QImage& image, const QColor& paperColor);

  /**
   * \brief Check if a color represents a significant cast worth correcting.
   *
   * Returns false if the color is already close to neutral gray,
   * meaning correction would have minimal effect.
   *
   * \param paperColor The detected paper color
   * \param threshold Minimum channel deviation to consider significant (default: 10)
   * \return true if the color has a significant cast
   */
  static bool hasSignificantCast(const QColor& paperColor, int threshold = 5);

  /**
   * \brief Find the brightest pixels and assume they should be white.
   *
   * This is a more aggressive approach for "Force White Balance" mode.
   * Finds the brightest pixels in the image and uses their color
   * as the paper color to correct.
   *
   * \param image The source image
   * \return The color of the brightest pixels
   */
  static QColor findBrightestPixels(const QImage& image);

 private:
  /**
   * \brief Sample color from margin areas outside the content box.
   */
  static QColor sampleMarginColor(const QImage& image, const QRect& contentBox);

  /**
   * \brief Find bright, low-saturation pixels that likely represent paper.
   */
  static QColor findNeutralPixels(const QImage& image);

  /**
   * \brief Check if there are significant margins to sample from.
   */
  static bool hasSignificantMargins(const QImage& image, const QRect& contentBox);
};

#endif  // SCANTAILOR_CORE_WHITEBALANCE_H_
