// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "SpineDarknessFinder.h"

#include <GrayImage.h>

#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "DebugImages.h"

namespace page_split {
using namespace imageproc;

namespace {

// Width (in downscaled px) of the stripe averaged for each candidate column.
// A small stripe suppresses single-pixel streaks (scratches, dust) while
// still being narrow enough to localize a real spine shadow.
constexpr int kStripeHalfWidth = 1;  // -> 3-pixel-wide stripe

// Minimum mean darkness (255 - pixel) along a column for it to be even
// considered a spine candidate. Filters out fully white-margin scans.
constexpr double kMinMeanDarkness = 25.0;

// Required excess of the chosen column's mean darkness over the median
// column darkness across the search window. This is the contrast/prominence
// check that prevents picking arbitrary columns on uniformly dark pages
// (e.g. full-page photographs).
constexpr double kMinDarknessProminence = 18.0;

// Fraction of rows that must be "dark enough" along the candidate column
// for it to count as a continuous spine. Guards against bright gaps where
// the gutter shadow is broken (e.g. across captions or page numbers in
// the gutter), and against image edges that only span part of the page.
constexpr double kMinDarkRowFraction = 0.55;

// A row is "dark enough" if its sampled value is at least this much above
// the row's local background brightness. Lowered from 30 so soft gutter
// shadows on bright pages still register.
constexpr int kPerRowDarknessThreshold = 18;

// Required excess of the chosen column's mean darkness over its *immediate*
// horizontal neighbors. This is the local-contrast / "valley" check that
// distinguishes a real gutter shadow (which has bright page content right
// next to it on at least one side) from a column inside a uniformly dark
// photograph (where the columns on both sides are equally dark). Without
// this gate, ±15% search windows on dark image-heavy spreads (e.g. dark
// jewelry plates, full-page interiors) latch onto interior columns of
// the photo and split through it.
constexpr double kMinNeighborContrast = 14.0;

// Maximum mean darkness, in 0–255 darkness units, that the *lighter* of the
// two neighbor stripes is allowed to have. The relative neighbor-contrast
// gate above is trivially satisfied inside a dark photograph (a column at
// darkness 200 with neighbors at 180 still passes 200−180 ≥ 14), so we
// additionally require at least one neighbor stripe to look like actual
// paper. Comfortably above text-on-paper (~25–35) and well below
// image-interior (~100+).
constexpr double kMaxPaperNeighborDarkness = 55.0;

// "Local maximum" / peak-prominence gate: how much the candidate column's
// mean darkness must exceed the *darker* of its two neighbor stripes. A
// real spine shadow is a thin dark line surrounded by paper on BOTH sides,
// so the candidate is much darker than both neighbors. The left/right
// edge of a wide dark photograph is a *transition*: the candidate sits in
// the middle of a step gradient, with paper on one side and continuing
// dark on the other side, so the candidate is *brighter* than the dark
// side and the peak prominence is negative or near zero. Without this
// gate, photo edges adjacent to a text page are accepted as gutters
// because they pass the paper-side gate (one side is paper).
constexpr double kMinPeakProminence = 14.0;

// Pixel offsets at which the neighbor stripes are sampled, in the
// downscaled (100 dpi) image. The inner edge of the neighbor stripe sits
// kNeighborInnerOffset px from the candidate column center, and the
// stripe is kNeighborStripeWidth px wide. Far enough out to clear the
// gutter shadow's soft edge, close enough to still be on the same physical
// page surface and not into adjacent unrelated content.
constexpr int kNeighborInnerOffset = 4;
constexpr int kNeighborStripeWidth = 5;

// Tilt sweep step in degrees.
constexpr double kTiltStepDegrees = 0.5;

inline double clampd(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Sample a 3-pixel-wide vertical stripe and return the mean darkness
// (255 - mean pixel) along its length, plus the count of "dark" rows.
struct ColumnStats {
  double meanDarkness = 0.0;
  int darkRows = 0;
  int sampledRows = 0;
};

ColumnStats sampleColumn(const GrayImage& gray,
                         double xTopDs,
                         double xBottomDs,
                         int rowBackground) {
  ColumnStats stats;
  const int w = gray.width();
  const int h = gray.height();
  if (w <= 0 || h <= 0) {
    return stats;
  }
  const uint8_t* data = gray.data();
  const int stride = gray.stride();

  long long darknessSum = 0;
  for (int y = 0; y < h; ++y) {
    const double t = (h <= 1) ? 0.0 : static_cast<double>(y) / (h - 1);
    const double xCenter = xTopDs + t * (xBottomDs - xTopDs);
    const int x0 = static_cast<int>(std::floor(xCenter)) - kStripeHalfWidth;
    const int x1 = x0 + 2 * kStripeHalfWidth;
    if (x0 < 0 || x1 >= w) {
      continue;
    }

    int stripeMin = 255;
    for (int x = x0; x <= x1; ++x) {
      const int v = data[y * stride + x];
      if (v < stripeMin) {
        stripeMin = v;
      }
    }
    // Use the darkest pixel in the stripe — a spine shadow is typically
    // 1-2 px wide and we don't want it diluted by adjacent paper.
    const int darkness = 255 - stripeMin;
    darknessSum += darkness;
    stats.sampledRows += 1;
    if (darkness >= rowBackground + kPerRowDarknessThreshold) {
      stats.darkRows += 1;
    }
  }

  if (stats.sampledRows > 0) {
    stats.meanDarkness = static_cast<double>(darknessSum) / stats.sampledRows;
  }
  return stats;
}

// Mean darkness of a vertical stripe [stripeXLo, stripeXHi] sampled along
// the candidate spine line (which may be tilted). Mirrors the geometry of
// sampleColumn so the comparison is apples-to-apples.
double meanDarknessOfStripe(const GrayImage& gray,
                            double xTopDs,
                            double xBottomDs,
                            int stripeXLo,
                            int stripeXHi) {
  const int w = gray.width();
  const int h = gray.height();
  if (w <= 0 || h <= 0 || stripeXLo > stripeXHi) {
    return 0.0;
  }
  // NOTE: stripeXLo and stripeXHi are *relative offsets* from xCenter, not
  // absolute coordinates — the left neighbor's offsets are negative by
  // design. The earlier early-exit `if (stripeXLo < 0 || stripeXHi >= w)`
  // check was a bug that always made the LEFT neighbor return 0 (forcing
  // the peak-prominence gate to be one-sided). The correct bounds check
  // is per-row inside the loop below.
  const uint8_t* data = gray.data();
  const int stride = gray.stride();

  long long darknessSum = 0;
  int sampledRows = 0;
  for (int y = 0; y < h; ++y) {
    const double t = (h <= 1) ? 0.0 : static_cast<double>(y) / (h - 1);
    const double xCenter = xTopDs + t * (xBottomDs - xTopDs);
    const int x0 = static_cast<int>(std::floor(xCenter)) + stripeXLo;
    const int x1 = static_cast<int>(std::floor(xCenter)) + stripeXHi;
    if (x0 < 0 || x1 >= w) {
      continue;
    }
    long long rowSum = 0;
    int rowCount = 0;
    for (int x = x0; x <= x1; ++x) {
      rowSum += data[y * stride + x];
      ++rowCount;
    }
    if (rowCount > 0) {
      const int meanPixel = static_cast<int>(rowSum / rowCount);
      darknessSum += (255 - meanPixel);
      ++sampledRows;
    }
  }
  if (sampledRows == 0) {
    return 0.0;
  }
  return static_cast<double>(darknessSum) / sampledRows;
}

// Estimate per-row "background" darkness across the search window so the
// per-row threshold adapts to overall page brightness.
int estimateRowBackground(const GrayImage& gray, int xLo, int xHi) {
  const int w = gray.width();
  const int h = gray.height();
  if (w <= 0 || h <= 0 || xLo >= xHi) {
    return 0;
  }
  xLo = std::max(0, xLo);
  xHi = std::min(w - 1, xHi);
  const uint8_t* data = gray.data();
  const int stride = gray.stride();

  // Sample row means at a coarse stride to keep this cheap.
  const int rowStep = std::max(1, h / 64);
  long long sum = 0;
  long long count = 0;
  for (int y = 0; y < h; y += rowStep) {
    long long rowSum = 0;
    int rowCount = 0;
    for (int x = xLo; x <= xHi; ++x) {
      rowSum += data[y * stride + x];
      ++rowCount;
    }
    if (rowCount > 0) {
      sum += rowSum / rowCount;
      ++count;
    }
  }
  if (count == 0) {
    return 0;
  }
  // Convert mean pixel to mean darkness.
  return 255 - static_cast<int>(sum / count);
}

}  // namespace

QLineF SpineDarknessFinder::findSpine(const GrayImage& grayDownscaled,
                                      const QTransform& outToDownscaled,
                                      const QRectF& virtualImageRect,
                                      const double centerWindowFraction,
                                      const double maxTiltDegrees,
                                      const double centerXOverride,
                                      DebugImages* const dbg) {
  if (grayDownscaled.isNull()) {
    return QLineF();
  }
  if (virtualImageRect.isEmpty()) {
    return QLineF();
  }

  bool invertible = false;
  const QTransform downscaledToOut = outToDownscaled.inverted(&invertible);
  if (!invertible) {
    return QLineF();
  }

  const int dsW = grayDownscaled.width();
  const int dsH = grayDownscaled.height();
  if (dsW < 8 || dsH < 8) {
    return QLineF();
  }

  // Decide which virtual-X to center the search window on. Default is the
  // geometric center of virtualImageRect; the caller can pass an override
  // to anchor the search on a different position (used by the Vision
  // refinement pass).
  const double anchorVirtX = std::isfinite(centerXOverride)
      ? centerXOverride
      : virtualImageRect.center().x();

  // Map the virtual top/bottom of the anchor column to downscaled coords.
  // Using both top and bottom (rather than just one point) keeps things
  // correct if outToDownscaled has any rotation/shear component.
  const QPointF topCenterVirt(anchorVirtX, virtualImageRect.top());
  const QPointF bottomCenterVirt(anchorVirtX, virtualImageRect.bottom());
  const QPointF topCenterDs = outToDownscaled.map(topCenterVirt);
  const QPointF bottomCenterDs = outToDownscaled.map(bottomCenterVirt);

  // Half-width of the search window, in downscaled px.
  const QPointF leftEdgeDs = outToDownscaled.map(
      QPointF(virtualImageRect.left(), virtualImageRect.center().y()));
  const QPointF rightEdgeDs = outToDownscaled.map(
      QPointF(virtualImageRect.right(), virtualImageRect.center().y()));
  const double virtualWidthDs =
      std::hypot(rightEdgeDs.x() - leftEdgeDs.x(), rightEdgeDs.y() - leftEdgeDs.y());
  const double halfWindowDs = std::max(4.0, centerWindowFraction * virtualWidthDs);

  const double centerXDs = 0.5 * (topCenterDs.x() + bottomCenterDs.x());
  const int xLoSearch = static_cast<int>(std::floor(centerXDs - halfWindowDs));
  const int xHiSearch = static_cast<int>(std::ceil(centerXDs + halfWindowDs));

  // Stay clear of the absolute image edges.
  const int margin = kStripeHalfWidth + 1;
  const int xLo = std::max(margin, xLoSearch);
  const int xHi = std::min(dsW - 1 - margin, xHiSearch);
  if (xLo >= xHi) {
    return QLineF();
  }

  const int rowBackground = estimateRowBackground(grayDownscaled, xLo, xHi);

  // Sweep tilt angles. For each tilt, the line passes through (x, h/2)
  // with slope tan(theta), so its top x is x - (h/2)*tan(theta) and its
  // bottom x is x + (h/2)*tan(theta).
  const double tiltStep = kTiltStepDegrees;
  const int tiltSteps = static_cast<int>(std::floor(maxTiltDegrees / tiltStep));
  const double halfH = 0.5 * (dsH - 1);

  double bestMean = 0.0;
  double bestXTop = -1.0;
  double bestXBottom = -1.0;
  int bestDarkRows = 0;
  int bestSampledRows = 0;

  // Collect column means at tilt 0 for the prominence check AND for the
  // post-loop spatial-NMS diagnostic pass below.
  std::vector<double> zeroTiltMeans;
  zeroTiltMeans.reserve(static_cast<size_t>(xHi - xLo + 1));

  for (int t = -tiltSteps; t <= tiltSteps; ++t) {
    const double tiltDeg = t * tiltStep;
    const double dx = std::tan(tiltDeg * M_PI / 180.0) * halfH;
    for (int xCenter = xLo; xCenter <= xHi; ++xCenter) {
      const double xTop = xCenter - dx;
      const double xBottom = xCenter + dx;
      // Skip if the tilted line escapes the safe region at either end.
      if (xTop < margin || xTop > dsW - 1 - margin) {
        continue;
      }
      if (xBottom < margin || xBottom > dsW - 1 - margin) {
        continue;
      }

      const ColumnStats stats = sampleColumn(grayDownscaled, xTop, xBottom, rowBackground);
      if (t == 0) {
        zeroTiltMeans.push_back(stats.meanDarkness);
      }
      if (stats.meanDarkness > bestMean) {
        bestMean = stats.meanDarkness;
        bestXTop = xTop;
        bestXBottom = xBottom;
        bestDarkRows = stats.darkRows;
        bestSampledRows = stats.sampledRows;
      }
    }
  }

  if (bestXTop < 0.0 || bestSampledRows == 0) {
    return QLineF();
  }

  // Compute the median column mean once; needed by the global-prominence
  // gate inside evaluateColumn() below.
  double medianMean = 0.0;
  if (!zeroTiltMeans.empty()) {
    std::vector<double> sortedMeans = zeroTiltMeans;
    std::sort(sortedMeans.begin(), sortedMeans.end());
    medianMean = sortedMeans[sortedMeans.size() / 2];
  }

  // evaluateColumn — runs the full gate sequence on a single candidate
  // column and, on success, fills the per-column statistics that the
  // existing line-construction and logging code depend on. Used for both
  // the global-max winner from the tilt sweep AND each spatially-distinct
  // NMS candidate considered for the anchor re-pick below. The gates are
  // identical to the ones the function used to apply only to the global
  // max — moving them into a lambda lets us evaluate other candidates
  // against the same criteria without code duplication.
  auto evaluateColumn = [&](double xT, double xB, double meanDark, int darkRows, int sampledRows,
                            double& outLeft, double& outRight, double& outPP, double& outDrf,
                            const char* logTag) -> bool {
    if (meanDark < kMinMeanDarkness) return false;
    outDrf = sampledRows > 0
        ? static_cast<double>(darkRows) / static_cast<double>(sampledRows)
        : 0.0;
    if (outDrf < kMinDarkRowFraction) return false;
    if (meanDark - medianMean < kMinDarknessProminence) return false;
    outLeft = meanDarknessOfStripe(grayDownscaled, xT, xB,
        -kNeighborInnerOffset - kNeighborStripeWidth + 1, -kNeighborInnerOffset);
    outRight = meanDarknessOfStripe(grayDownscaled, xT, xB,
        kNeighborInnerOffset, kNeighborInnerOffset + kNeighborStripeWidth - 1);
    const double minNbr = std::min(outLeft, outRight);
    if (minNbr > kMaxPaperNeighborDarkness) {
      if (logTag) {
        qDebug() << "SpineDarknessFinder:" << logTag << "reject xT=" << xT
                 << "mean=" << meanDark << "left=" << outLeft << "right=" << outRight
                 << "(no paper-like side; lighter neighbor" << minNbr
                 << ">" << kMaxPaperNeighborDarkness << ")";
      }
      return false;
    }
    if (meanDark - minNbr < kMinNeighborContrast) {
      if (logTag) {
        qDebug() << "SpineDarknessFinder:" << logTag << "reject xT=" << xT
                 << "mean=" << meanDark << "left=" << outLeft << "right=" << outRight
                 << "contrast=" << (meanDark - minNbr) << "<" << kMinNeighborContrast;
      }
      return false;
    }
    outPP = meanDark - std::max(outLeft, outRight);
    if (outPP < kMinPeakProminence) {
      if (logTag) {
        qDebug() << "SpineDarknessFinder:" << logTag << "reject xT=" << xT
                 << "mean=" << meanDark << "left=" << outLeft << "right=" << outRight
                 << "peakProm=" << outPP << "<" << kMinPeakProminence;
      }
      return false;
    }
    return true;
  };

  // Validate the global max from the tilt sweep against all gates. If it
  // fails, no spine — exactly the previous behavior.
  double leftNeighborMean = 0.0;
  double rightNeighborMean = 0.0;
  double peakProminence = 0.0;
  double darkRowFraction = 0.0;
  if (!evaluateColumn(bestXTop, bestXBottom, bestMean, bestDarkRows, bestSampledRows,
                      leftNeighborMean, rightNeighborMean, peakProminence, darkRowFraction,
                      "global-max")) {
    return QLineF();
  }

  // ===== NMS pass + anchor re-pick =====
  //
  // The previous "darkest column wins" rule fails on asymmetric image-on-one-
  // side / text-on-the-other spreads where there's a strong dark feature in
  // the search window that ISN'T the binding fold (e.g. the right edge of a
  // dark relief sculpture). On those spreads the NMS dump shows TWO viable
  // gate-passing candidates: the strong artifact and the actual gutter, with
  // the gutter typically much closer to Vision's anchor (centerXDs) than the
  // artifact. Picking the closest-to-anchor viable candidate flips the choice
  // from the artifact to the gutter without regressing pages where only one
  // viable candidate exists in the window (every clean spread, plus the
  // already-broken-by-other-causes pages 92/95).
  //
  // The diagnostic dump that the previous build added is folded into this
  // pass so we can see, in one place, the runners-up *and* which candidate
  // the new rule selected.
  constexpr int kMinSeparationDs = 25;
  constexpr int kMaxNmsCandidates = 8;

  struct NmsCandidate {
    int xCenterDs = 0;
    double meanDarkness = 0.0;
    int darkRows = 0;
    int sampledRows = 0;
  };
  std::vector<NmsCandidate> nms;
  if (zeroTiltMeans.size() > 1) {
    std::vector<int> idxByMean(zeroTiltMeans.size());
    std::iota(idxByMean.begin(), idxByMean.end(), 0);
    std::sort(idxByMean.begin(), idxByMean.end(),
              [&](int a, int b) { return zeroTiltMeans[a] > zeroTiltMeans[b]; });

    nms.reserve(kMaxNmsCandidates);
    for (int idx : idxByMean) {
      if (nms.size() == static_cast<size_t>(kMaxNmsCandidates)) break;
      const int xCenterCand = xLo + idx;
      bool tooClose = false;
      for (const auto& existing : nms) {
        if (std::abs(existing.xCenterDs - xCenterCand) < kMinSeparationDs) {
          tooClose = true;
          break;
        }
      }
      if (tooClose) continue;
      // Re-sample untilted at this column for accurate darkRows / sampledRows.
      const ColumnStats stats =
          sampleColumn(grayDownscaled, xCenterCand, xCenterCand, rowBackground);
      nms.push_back({xCenterCand, stats.meanDarkness, stats.darkRows, stats.sampledRows});
    }
  }

  // Build the viable-set: candidates that pass *all* gates. The global max
  // is always first (it already passed gates above). Each NMS candidate that
  // doesn't spatially overlap the global max gets re-evaluated against the
  // same gates; survivors are added.
  struct ViableCandidate {
    int xCenterDs = 0;
    double xTopDs = 0.0;
    double xBottomDs = 0.0;
    double meanDarkness = 0.0;
    int darkRows = 0;
    int sampledRows = 0;
    double leftNbr = 0.0;
    double rightNbr = 0.0;
    double peakProm = 0.0;
    double drf = 0.0;
  };
  std::vector<ViableCandidate> viable;
  viable.reserve(nms.size() + 1);

  const int globalMaxXCenterDs =
      static_cast<int>(std::round(0.5 * (bestXTop + bestXBottom)));
  viable.push_back({globalMaxXCenterDs, bestXTop, bestXBottom,
                    bestMean, bestDarkRows, bestSampledRows,
                    leftNeighborMean, rightNeighborMean, peakProminence, darkRowFraction});

  for (const NmsCandidate& c : nms) {
    if (std::abs(c.xCenterDs - globalMaxXCenterDs) < kMinSeparationDs) continue;
    double l = 0.0, r = 0.0, pp = 0.0, df = 0.0;
    if (!evaluateColumn(double(c.xCenterDs), double(c.xCenterDs),
                        c.meanDarkness, c.darkRows, c.sampledRows,
                        l, r, pp, df, /*logTag=*/nullptr)) {
      continue;
    }
    viable.push_back({c.xCenterDs, double(c.xCenterDs), double(c.xCenterDs),
                      c.meanDarkness, c.darkRows, c.sampledRows, l, r, pp, df});
  }

  // Anchor re-pick: among the viable candidates, pick the one closest to
  // centerXDs (the search-window anchor — Vision's centerXOverride if
  // supplied, geometric center of virtualImageRect otherwise).
  size_t pickIdx = 0;
  {
    double bestDist = std::abs(double(viable[0].xCenterDs) - centerXDs);
    for (size_t i = 1; i < viable.size(); ++i) {
      const double d = std::abs(double(viable[i].xCenterDs) - centerXDs);
      if (d < bestDist) {
        bestDist = d;
        pickIdx = i;
      }
    }
  }

  // (No "clean isolated peak" override guard. An earlier version restricted
  // overrides to candidates whose global-max had paper-like neighbors on
  // BOTH sides, on the theory that strongly-asymmetric neighbor profiles
  // were genuine photo-edge gutters that shouldn't be re-picked. That
  // theory turned out to be wrong on the user's project: page 95's
  // skeleton photo has its global max at xVirt=1449 with left=139 right=4
  // — strongly asymmetric — but the *correct* gutter is at xVirt=1608
  // (closer to Vision's anchor), and the unrestricted override correctly
  // flipped to it. The guard regressed page 95. Without the guard the
  // override fires on both page 67 (relief, clean peak) and page 95
  // (skeleton, asymmetric peak), and both end up at the right spot.)

  // If the anchor pick is not the global max, override the chosen winner
  // and recover its best tilt by re-running the tilt sub-search at the
  // picked column. The viable candidate built above is untilted; the tilt
  // sub-search lets the picked column tilt slightly so the spine line
  // matches a not-quite-vertical scan.
  if (pickIdx != 0) {
    const ViableCandidate orig = viable[0];
    const ViableCandidate& picked = viable[pickIdx];

    double tBestMean = picked.meanDarkness;
    double tBestXTop = picked.xTopDs;
    double tBestXBottom = picked.xBottomDs;
    int tBestDarkRows = picked.darkRows;
    int tBestSampledRows = picked.sampledRows;
    for (int t = -tiltSteps; t <= tiltSteps; ++t) {
      const double tiltDeg = t * tiltStep;
      const double dx = std::tan(tiltDeg * M_PI / 180.0) * halfH;
      const double xT = picked.xCenterDs - dx;
      const double xB = picked.xCenterDs + dx;
      if (xT < margin || xT > dsW - 1 - margin) continue;
      if (xB < margin || xB > dsW - 1 - margin) continue;
      const ColumnStats stats =
          sampleColumn(grayDownscaled, xT, xB, rowBackground);
      if (stats.meanDarkness > tBestMean) {
        tBestMean = stats.meanDarkness;
        tBestXTop = xT;
        tBestXBottom = xB;
        tBestDarkRows = stats.darkRows;
        tBestSampledRows = stats.sampledRows;
      }
    }

    // Re-evaluate neighbors at the (now tilted) picked column. The picked
    // candidate already passed evaluateColumn() at tilt=0; the tilt
    // recovery only nudges the line a few pixels, so it's extremely
    // unlikely to push it back across a gate. We don't re-gate.
    const double newLeft = meanDarknessOfStripe(grayDownscaled, tBestXTop, tBestXBottom,
        -kNeighborInnerOffset - kNeighborStripeWidth + 1, -kNeighborInnerOffset);
    const double newRight = meanDarknessOfStripe(grayDownscaled, tBestXTop, tBestXBottom,
        kNeighborInnerOffset, kNeighborInnerOffset + kNeighborStripeWidth - 1);

    qDebug() << "SpineDarknessFinder: [ANCHOR-PICK] overriding global max:"
             << "was xCenter=" << orig.xCenterDs
             << "mean=" << orig.meanDarkness
             << "(distFromAnchor=" << std::abs(double(orig.xCenterDs) - centerXDs) << ")"
             << "→ now xCenter=" << picked.xCenterDs
             << "mean=" << tBestMean
             << "(distFromAnchor=" << std::abs(double(picked.xCenterDs) - centerXDs) << ")"
             << "anchor=" << centerXDs;

    bestMean = tBestMean;
    bestXTop = tBestXTop;
    bestXBottom = tBestXBottom;
    bestDarkRows = tBestDarkRows;
    bestSampledRows = tBestSampledRows;
    leftNeighborMean = newLeft;
    rightNeighborMean = newRight;
    peakProminence = tBestMean - std::max(newLeft, newRight);
    darkRowFraction = tBestSampledRows > 0
        ? static_cast<double>(tBestDarkRows) / static_cast<double>(tBestSampledRows)
        : 0.0;
  }

  // Build the line in downscaled coordinates and map back to virtual.
  const QPointF topDs(bestXTop, 0.0);
  const QPointF bottomDs(bestXBottom, dsH - 1);
  QLineF spineVirt(downscaledToOut.map(topDs), downscaledToOut.map(bottomDs));

  // Clip to the virtual rect's vertical extent so the line spans the page.
  // Snapshot the original endpoints before mutating spineVirt — the lambda
  // below must use the *original* line, not the partially-updated one.
  const QPointF origP1 = spineVirt.p1();
  const QPointF origP2 = spineVirt.p2();
  const double y0 = origP1.y();
  const double y1 = origP2.y();
  if (std::fabs(y1 - y0) > 1e-9) {
    auto pointAtY = [&](double y) {
      const double t = (y - y0) / (y1 - y0);
      return QPointF(origP1.x() + t * (origP2.x() - origP1.x()), y);
    };
    spineVirt.setP1(pointAtY(virtualImageRect.top()));
    spineVirt.setP2(pointAtY(virtualImageRect.bottom()));
  }

  qDebug() << "SpineDarknessFinder: spine at" << spineVirt
           << "meanDarkness=" << bestMean << "darkRowFrac=" << darkRowFraction
           << "leftNbr=" << leftNeighborMean << "rightNbr=" << rightNeighborMean
           << "peakProm=" << peakProminence;

  // Diagnostic dump of the spatially-separated NMS candidates that the
  // anchor re-pick logic above considered. The chosen winner is marked
  // [CHOSEN] (the anchor pick — possibly different from the global max
  // when an [ANCHOR-PICK] override fired). Runners-up that don't pass
  // every gate get a "[gate-fail]" annotation so we can tell from the log
  // which alternatives the rule rejected vs which it found viable.
  if (!nms.empty()) {
    qDebug() << "SpineDarknessFinder: top" << nms.size()
             << "spatially-separated candidates:";
    const double finalChosenCenterDs = 0.5 * (bestXTop + bestXBottom);
    for (size_t i = 0; i < nms.size(); ++i) {
      const NmsCandidate& c = nms[i];
      const double leftNbr = meanDarknessOfStripe(
          grayDownscaled, c.xCenterDs, c.xCenterDs,
          -kNeighborInnerOffset - kNeighborStripeWidth + 1, -kNeighborInnerOffset);
      const double rightNbr = meanDarknessOfStripe(
          grayDownscaled, c.xCenterDs, c.xCenterDs,
          kNeighborInnerOffset, kNeighborInnerOffset + kNeighborStripeWidth - 1);
      const double peakProm = c.meanDarkness - std::max(leftNbr, rightNbr);
      const double drf = c.sampledRows > 0
          ? static_cast<double>(c.darkRows) / static_cast<double>(c.sampledRows)
          : 0.0;
      const QPointF midVirt =
          downscaledToOut.map(QPointF(c.xCenterDs, 0.5 * (dsH - 1)));
      // Was this NMS candidate found viable by the gate evaluation above?
      bool isViable = false;
      for (const auto& v : viable) {
        if (std::abs(v.xCenterDs - c.xCenterDs) < kMinSeparationDs) {
          isViable = true;
          break;
        }
      }
      const bool isChosen =
          std::fabs(static_cast<double>(c.xCenterDs) - finalChosenCenterDs)
          < static_cast<double>(kMinSeparationDs);
      qDebug() << "  #" << i << "xVirt=" << midVirt.x()
               << "mean=" << c.meanDarkness
               << "drf=" << drf
               << "left=" << leftNbr << "right=" << rightNbr
               << "peakProm=" << peakProm
               << (isChosen ? "[CHOSEN]" : (isViable ? "[viable]" : "[gate-fail]"));
    }
  }

  if (dbg) {
    QImage visual(grayDownscaled.toQImage().convertToFormat(QImage::Format_ARGB32_Premultiplied));
    QPainter painter(&visual);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0x00, 0xff, 0x00, 0xc0));
    pen.setWidthF(2.0);
    painter.setPen(pen);
    painter.drawLine(QLineF(QPointF(bestXTop, 0.0), QPointF(bestXBottom, dsH - 1)));
    // Visualize the search window.
    pen.setColor(QColor(0xff, 0xff, 0x00, 0x60));
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.drawLine(QLineF(xLo, 0, xLo, dsH - 1));
    painter.drawLine(QLineF(xHi, 0, xHi, dsH - 1));
    painter.end();
    dbg->add(visual, "spine_darkness");
  }

  // Suppress unused-parameter warning when DebugImages is null.
  (void) clampd;
  return spineVirt;
}

}  // namespace page_split
