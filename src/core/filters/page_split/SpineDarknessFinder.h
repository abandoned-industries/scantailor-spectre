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
   * \param broadGutterRescue If non-null, set to true when the returned
   *                          line was accepted by the broad-gutter rescue
   *                          path rather than the normal thin-spine gates.
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
                          DebugImages* dbg = nullptr,
                          bool* broadGutterRescue = nullptr);

  /**
   * \brief Brightness-based fallback that locates the spine as a bright
   *        column of paper between the two text blocks.
   *
   * Used when findSpine() returns null because no column in the search
   * window has the darkness signature of a real binding shadow. This
   * happens on flatbed scans where the actual binding fold lies in
   * white paper between the bottom text columns and has no shadow at
   * all, while the only nearby dark features are photo edges that the
   * gates correctly reject.
   *
   * Builds a column-brightness profile over the bottom text band, then
   * picks the column inside the search window that is (a) paper-bright,
   * (b) flanked by text-darkened stripes on both sides, and (c) the
   * closest gate-survivor to centerXOverride (or geometric center).
   * Returns a vertical line in virtual coordinates, or a null QLineF
   * if no acceptable candidate exists.
   *
   * Designed to be conservative: on full-bleed photo spreads or pages
   * without a paper gutter, all gates fail and the function returns
   * null so the caller can fall back to Vision's exact split.
   */
  static QLineF findSpineByPaperGap(const imageproc::GrayImage& grayDownscaled,
                                    const QTransform& outToDownscaled,
                                    const QRectF& virtualImageRect,
                                    double centerWindowFraction = 0.10,
                                    double centerXOverride = std::numeric_limits<double>::quiet_NaN());
};

}  // namespace page_split
#endif  // ifndef SCANTAILOR_PAGE_SPLIT_SPINEDARKNESSFINDER_H_
