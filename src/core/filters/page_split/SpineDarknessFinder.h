// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_SPLIT_SPINEDARKNESSFINDER_H_
#define SCANTAILOR_PAGE_SPLIT_SPINEDARKNESSFINDER_H_

#include <QLineF>
#include <QRectF>
#include <QTransform>
#include <limits>

class DebugImages;

namespace imageproc {
class GrayImage;
}

namespace page_split {

/**
 * \brief Locates a book spine as a near-vertical line of consistently dark
 *        pixels close to the horizontal center of the image.
 *
 * This is a fallback used when the gradient/Hough-based VertLineFinder
 * fails to produce a usable candidate. It targets the common case of a
 * dark gutter shadow running top-to-bottom on a two-page spread.
 *
 * The search is restricted to a window around the horizontal center
 * (±centerWindowFraction of the virtual image width). A small tilt
 * search (±maxTiltDegrees) is performed to tolerate slightly skewed
 * scans. The candidate must be both consistently dark along its length
 * and significantly darker than its surroundings to be accepted.
 */
class SpineDarknessFinder {
 public:
  /**
   * \param grayDownscaled    The downscaled (typically 100 dpi) grayscale
   *                          image, as produced by VertLineFinder::findLines
   *                          or VertLineFinder::buildGrayDownscaled.
   * \param outToDownscaled   Transform mapping virtual output coordinates
   *                          to coordinates of \p grayDownscaled. Must be
   *                          invertible.
   * \param virtualImageRect  Bounding rect of the page in virtual output
   *                          coordinates.
   * \param centerWindowFraction
   *                          Half-width of the search window, expressed as
   *                          a fraction of the virtual image width. The
   *                          spine must lie within this window.
   * \param maxTiltDegrees    Maximum tilt of the candidate spine line in
   *                          degrees from vertical.
   * \param centerXOverride   If finite, the search window centers on this
   *                          virtual-X coordinate instead of the geometric
   *                          center of \p virtualImageRect. Used by the
   *                          Vision refinement pass to anchor the search
   *                          on Vision's claimed split position.
   * \param dbg               Optional debug image sink.
   *
   * \return The detected spine in virtual output coordinates, or a default-
   *         constructed (null) QLineF if no acceptable candidate was found.
   */
  static QLineF findSpine(const imageproc::GrayImage& grayDownscaled,
                          const QTransform& outToDownscaled,
                          const QRectF& virtualImageRect,
                          double centerWindowFraction = 0.10,
                          double maxTiltDegrees = 2.0,
                          double centerXOverride = std::numeric_limits<double>::quiet_NaN(),
                          DebugImages* dbg = nullptr);
};

}  // namespace page_split
#endif  // ifndef SCANTAILOR_PAGE_SPLIT_SPINEDARKNESSFINDER_H_
