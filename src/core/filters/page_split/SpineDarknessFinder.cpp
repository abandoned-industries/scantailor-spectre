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
// paper. Comfortably above text-on-paper (~25-35), while allowing tinted
// scans and soft gutter shadows where the adjacent paper measures darker.
// Still below image-interior darkness (~100+).
constexpr double kMaxPaperNeighborDarkness = 70.0;

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
                                      DebugImages* const dbg,
                                      bool* const broadGutterRescue) {
  if (broadGutterRescue) {
    *broadGutterRescue = false;
  }
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
                            bool& outBroadGutter,
                            const char* logTag) -> bool {
    outBroadGutter = false;
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
    const double maxNbr = std::max(outLeft, outRight);
    const double xCenter = 0.5 * (xT + xB);
    const double distFromAnchor = std::abs(xCenter - centerXDs);
    const bool visionAnchored = std::isfinite(centerXOverride);
    const bool insideRefineWindow = distFromAnchor <= (centerWindowFraction + 0.025) * virtualWidthDs;
    const bool nearAnchor = distFromAnchor <= 0.06 * virtualWidthDs;
    if (minNbr > kMaxPaperNeighborDarkness) {
      const bool broadDarkBand = meanDark >= 170.0 && outDrf >= 0.70
          && (meanDark - minNbr) >= 80.0 && maxNbr >= 100.0;
      const bool broadPlateau = meanDark >= 150.0 && outDrf >= 0.55
          && minNbr >= kMaxPaperNeighborDarkness && maxNbr >= 90.0
          && (meanDark - minNbr) >= 6.0;
      if (insideRefineWindow && ((visionAnchored && broadDarkBand) || broadPlateau)) {
        outPP = meanDark - maxNbr;
        outBroadGutter = true;
        if (logTag) {
          qDebug() << "SpineDarknessFinder:" << logTag
                   << "accepting broad gutter xT=" << xT
                   << "mean=" << meanDark << "drf=" << outDrf
                   << "left=" << outLeft << "right=" << outRight
                   << "distFromAnchor=" << distFromAnchor;
        }
        return true;
      }
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
    outPP = meanDark - maxNbr;
    const bool plateauEdgeNearAnchor = nearAnchor && meanDark >= 150.0 && outDrf >= 0.55
        && minNbr <= kMaxPaperNeighborDarkness && maxNbr >= 100.0
        && (meanDark - minNbr) >= 80.0;
    if (plateauEdgeNearAnchor) {
      outBroadGutter = true;
      return true;
    }
    if (outPP < kMinPeakProminence) {
      // "Binding fold against text edge" pattern: a moderate-darkness
      // candidate column with bright paper on one neighbor and text on
      // the other. The peak-prominence gate (mean - max(left,right))
      // computes prominence vs the text neighbor and fails by a hair,
      // even though the candidate is dramatically darker than the
      // paper neighbor — exactly the signature of a tight inner
      // margin where the binding fold sits right against the text.
      // Accept when:
      //   - candidate is moderately dark (≥ 80, brightness ≤ ~175)
      //   - drk-row fraction is high (≥ 0.7, consistent vertical line)
      //   - one neighbor is paper-bright (≤ 25 darkness, ≥ 230 bright)
      //   - mean exceeds the paper neighbor by ≥ 60 (clear contrast
      //     vs the bright side; the text-side prominence is not
      //     constrained because text adjacency is the whole point)
      const bool bindingAgainstText = meanDark >= 80.0 && outDrf >= 0.7
          && minNbr <= 25.0 && (meanDark - minNbr) >= 60.0;
      if (bindingAgainstText) {
        if (logTag) {
          qDebug() << "SpineDarknessFinder:" << logTag
                   << "accepting binding-against-text xT=" << xT
                   << "mean=" << meanDark << "drf=" << outDrf
                   << "left=" << outLeft << "right=" << outRight
                   << "peakProm=" << outPP;
        }
        return true;
      }
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
  bool globalBroadGutter = false;
  if (!evaluateColumn(bestXTop, bestXBottom, bestMean, bestDarkRows, bestSampledRows,
                      leftNeighborMean, rightNeighborMean, peakProminence, darkRowFraction,
                      globalBroadGutter,
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
    bool broadGutter = false;
  };
  std::vector<ViableCandidate> viable;
  viable.reserve(nms.size() + 1);

  const int globalMaxXCenterDs =
      static_cast<int>(std::round(0.5 * (bestXTop + bestXBottom)));
  viable.push_back({globalMaxXCenterDs, bestXTop, bestXBottom,
                    bestMean, bestDarkRows, bestSampledRows,
                    leftNeighborMean, rightNeighborMean, peakProminence, darkRowFraction,
                    globalBroadGutter});

  for (const NmsCandidate& c : nms) {
    if (std::abs(c.xCenterDs - globalMaxXCenterDs) < kMinSeparationDs) continue;
    double l = 0.0, r = 0.0, pp = 0.0, df = 0.0;
    bool broad = false;
    if (!evaluateColumn(double(c.xCenterDs), double(c.xCenterDs),
                        c.meanDarkness, c.darkRows, c.sampledRows,
                        l, r, pp, df, broad, /*logTag=*/nullptr)) {
      continue;
    }
    viable.push_back({c.xCenterDs, double(c.xCenterDs), double(c.xCenterDs),
                      c.meanDarkness, c.darkRows, c.sampledRows, l, r, pp, df, broad});
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

    // Broad-gutter rescue is intentionally permissive so we can handle
    // shadowed book folds, but a dark photo interior can also form a broad
    // plateau near the center. If a normal, high-prominence spine candidate
    // is nearly as close to the anchor, prefer it over the broad plateau.
    if (viable[pickIdx].broadGutter) {
      constexpr double kNormalPeakOverrideMinProminence = 30.0;
      constexpr double kNormalPeakOverrideExtraDistFraction = 0.035;
      const double maxNormalDist = bestDist + kNormalPeakOverrideExtraDistFraction * virtualWidthDs;
      size_t normalPickIdx = pickIdx;
      double bestNormalDist = std::numeric_limits<double>::max();
      for (size_t i = 0; i < viable.size(); ++i) {
        const ViableCandidate& c = viable[i];
        if (c.broadGutter || c.peakProm < kNormalPeakOverrideMinProminence) {
          continue;
        }
        const double d = std::abs(double(c.xCenterDs) - centerXDs);
        if ((d <= maxNormalDist) && (d < bestNormalDist)) {
          bestNormalDist = d;
          normalPickIdx = i;
        }
      }
      if (normalPickIdx != pickIdx) {
        qDebug() << "SpineDarknessFinder: [ANCHOR-PICK] preferring normal peak over broad plateau:"
                 << "broad xCenter=" << viable[pickIdx].xCenterDs
                 << "broadDist=" << bestDist
                 << "normal xCenter=" << viable[normalPickIdx].xCenterDs
                 << "normalDist=" << bestNormalDist
                 << "normalPeakProm=" << viable[normalPickIdx].peakProm;
        pickIdx = normalPickIdx;
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

  if (broadGutterRescue) {
    *broadGutterRescue = viable[pickIdx].broadGutter;
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

QLineF SpineDarknessFinder::findSpineByPaperGap(const GrayImage& grayDownscaled,
                                                const QTransform& outToDownscaled,
                                                const QRectF& virtualImageRect,
                                                const double centerWindowFraction,
                                                const double centerXOverride) {
  if (grayDownscaled.isNull() || virtualImageRect.isEmpty()) {
    return QLineF();
  }

  bool invertible = false;
  const QTransform downscaledToOut = outToDownscaled.inverted(&invertible);
  if (!invertible) {
    return QLineF();
  }

  const int dsW = grayDownscaled.width();
  const int dsH = grayDownscaled.height();
  if (dsW < 16 || dsH < 16) {
    return QLineF();
  }

  // Reproduce findSpine()'s window math so the search range matches.
  const double anchorVirtX = std::isfinite(centerXOverride)
      ? centerXOverride
      : virtualImageRect.center().x();
  const QPointF topCenterDs =
      outToDownscaled.map(QPointF(anchorVirtX, virtualImageRect.top()));
  const QPointF bottomCenterDs =
      outToDownscaled.map(QPointF(anchorVirtX, virtualImageRect.bottom()));
  const QPointF leftEdgeDs = outToDownscaled.map(
      QPointF(virtualImageRect.left(), virtualImageRect.center().y()));
  const QPointF rightEdgeDs = outToDownscaled.map(
      QPointF(virtualImageRect.right(), virtualImageRect.center().y()));
  const double virtualWidthDs =
      std::hypot(rightEdgeDs.x() - leftEdgeDs.x(), rightEdgeDs.y() - leftEdgeDs.y());
  const double halfWindowDs = std::max(4.0, centerWindowFraction * virtualWidthDs);
  const double centerXDs = 0.5 * (topCenterDs.x() + bottomCenterDs.x());

  // Full-height row band (5..95% to skip edge scanner noise). We used to
  // use the bottom text band (60..95%) on the theory that photos at the
  // top of the page would pollute the brightness profile — but on
  // asymmetric text/photo spreads (e.g. text-left/photo-right) the photo's
  // darker mean brightness is exactly the "right flank" signal the
  // algorithm needs to bracket the gutter on the photo side. Without it
  // the right flank stays paper-tinted and paper-gap can't find a proper
  // right boundary. Using the full height means photos contribute darker
  // columns on their side of the gutter, which is what we want.
  const int rowLo = std::max(0, static_cast<int>(std::round(dsH * 0.05)));
  const int rowHi = std::min(dsH - 1, static_cast<int>(std::round(dsH * 0.95)));
  if (rowHi - rowLo < 8) {
    return QLineF();
  }

  // Build a column-mean brightness profile across the FULL width (not just
  // the search window) so the smoothing kernel doesn't truncate near edges.
  const uint8_t* data = grayDownscaled.data();
  const int stride = grayDownscaled.stride();
  std::vector<double> colBrightness(dsW, 0.0);
  const int rowCount = rowHi - rowLo + 1;
  for (int x = 0; x < dsW; ++x) {
    long long sum = 0;
    for (int y = rowLo; y <= rowHi; ++y) {
      sum += data[y * stride + x];
    }
    colBrightness[x] = static_cast<double>(sum) / rowCount;
  }

  // 9-tap boxcar smoothing to suppress per-glyph noise. Glyph strokes are
  // 1-3 px wide at 100 dpi; this kernel suppresses them but preserves the
  // wider gutter peak (typically 30-80 px wide).
  std::vector<double> smooth(dsW, 0.0);
  constexpr int kSmoothHalf = 4;
  for (int x = 0; x < dsW; ++x) {
    double s = 0.0;
    int n = 0;
    for (int dx = -kSmoothHalf; dx <= kSmoothHalf; ++dx) {
      const int xi = x + dx;
      if (xi < 0 || xi >= dsW) continue;
      s += colBrightness[xi];
      ++n;
    }
    smooth[x] = n > 0 ? s / n : colBrightness[x];
  }

  // Search bounds. The flank lookups read up to kFlankReach columns
  // beyond the bright run, so leave a margin against the image edges.
  constexpr int kFlankReach = 100;
  const int safeMargin = kFlankReach + 2;
  const int xLo = std::max(safeMargin, static_cast<int>(std::floor(centerXDs - halfWindowDs)));
  const int xHi = std::min(dsW - 1 - safeMargin, static_cast<int>(std::ceil(centerXDs + halfWindowDs)));
  if (xLo >= xHi) {
    return QLineF();
  }

  // Strategy: identify the bright paper run that CONTAINS centerXDs,
  // verify it has darker columns within reach on both sides, and return
  // the midpoint of the two flanking text-edge transitions. The line
  // between transition midpoints sits at the geometric center of the
  // gutter — much closer to the actual binding fold than the center of
  // the (≥235) bright run, which is biased on asymmetric pages where
  // the brightness slope on one side is much sharper than the other.
  //
  // Anchor: the run that contains centerXDs, not the longest run
  // anywhere in the window — this way, page-margin paper next to the
  // window cannot mislead the search.
  //
  // - kMinPaperBrightness 235: separates real white paper from
  //   text-tinted paper. The right-page text body in the bottom row
  //   band smoothes to ~220-230, well below 235.
  // - kMinDrop 18: each flank must contain a column at least this much
  //   darker than the run's peak brightness. Rejects flat-margin runs.
  // - kMinFlankBrightness 80: both flanks may not be photo-deep dark.
  //   One photo-dark flank is acceptable for asymmetric text/photo spreads,
  //   where the correct paper gutter is bounded by text on one side and
  //   a photograph on the other.
  constexpr double kMinPaperBrightness = 235.0;
  constexpr double kMinDrop = 18.0;
  constexpr double kMinFlankBrightness = 80.0;

  // The run containing centerXDs only exists if smooth[centerXDs] is
  // already paper-bright. If not, the binding fold is sitting inside
  // text rather than between texts and this fallback can't help.
  const int centerInt =
      std::clamp(static_cast<int>(std::round(centerXDs)), xLo, xHi);
  if (smooth[centerInt] < kMinPaperBrightness) {
    qDebug() << "SpineDarknessFinder: [PAPER-GAP] center column not paper-bright"
             << "centerXDs=" << centerXDs << "smooth=" << smooth[centerInt];
    return QLineF();
  }

  // Walk left and right from centerXDs while still in the bright run.
  // We allow the run to extend past the search window so the flank
  // lookup below can find darker columns just outside the leash.
  int runLo = centerInt;
  while (runLo - 1 >= safeMargin && smooth[runLo - 1] >= kMinPaperBrightness) {
    --runLo;
  }
  int runHi = centerInt;
  while (runHi + 1 <= dsW - 1 - safeMargin && smooth[runHi + 1] >= kMinPaperBrightness) {
    ++runHi;
  }

  // Peak brightness inside the run (for the drop test below).
  double runPeak = 0.0;
  for (int x = runLo; x <= runHi; ++x) {
    if (smooth[x] > runPeak) runPeak = smooth[x];
  }

  // Look for darker columns within kFlankReach px on each side of the
  // run (i.e., outside the run, in the dark valleys that bracket it).
  double leftFlankMin = std::numeric_limits<double>::max();
  for (int dx = 1; dx <= kFlankReach; ++dx) {
    const int xi = runLo - dx;
    if (xi < 0) break;
    if (smooth[xi] < leftFlankMin) leftFlankMin = smooth[xi];
  }
  double rightFlankMin = std::numeric_limits<double>::max();
  for (int dx = 1; dx <= kFlankReach; ++dx) {
    const int xi = runHi + dx;
    if (xi >= dsW) break;
    if (smooth[xi] < rightFlankMin) rightFlankMin = smooth[xi];
  }

  if (runPeak - leftFlankMin < kMinDrop || runPeak - rightFlankMin < kMinDrop) {
    qDebug() << "SpineDarknessFinder: [PAPER-GAP] run flank drops too small"
             << "runLo=" << runLo << "runHi=" << runHi
             << "peak=" << runPeak
             << "leftMin=" << leftFlankMin << "rightMin=" << rightFlankMin;
    return QLineF();
  }
  if (leftFlankMin < kMinFlankBrightness && rightFlankMin < kMinFlankBrightness) {
    qDebug() << "SpineDarknessFinder: [PAPER-GAP] both flanks look like photos"
             << "leftMin=" << leftFlankMin << "rightMin=" << rightFlankMin;
    return QLineF();
  }

  // Find each text edge as the column where the brightness profile
  // crosses the midway level between the run peak and the respective
  // flank min. This is the geometric center of the brightness ramp,
  // which corresponds to the text margin: less biased than picking
  // either the (≥235) cutoff (biased toward the run) or the deepest
  // dark column (biased toward dense text).
  const double leftCrossThresh = 0.5 * (runPeak + leftFlankMin);
  const double rightCrossThresh = 0.5 * (runPeak + rightFlankMin);
  int leftEdge = runLo;
  for (int dx = 0; dx <= kFlankReach; ++dx) {
    const int xi = runLo - dx;
    if (xi < 0) break;
    if (smooth[xi] <= leftCrossThresh) {
      leftEdge = xi;
      break;
    }
  }
  int rightEdge = runHi;
  for (int dx = 0; dx <= kFlankReach; ++dx) {
    const int xi = runHi + dx;
    if (xi >= dsW) break;
    if (smooth[xi] <= rightCrossThresh) {
      rightEdge = xi;
      break;
    }
  }
  const int pickX = (leftEdge + rightEdge) / 2;

  const QPointF topVirt = downscaledToOut.map(QPointF(pickX, 0.0));
  const QPointF bottomVirt = downscaledToOut.map(QPointF(pickX, dsH - 1));
  QLineF spineVirt(topVirt, bottomVirt);

  // Clip to virtual rect's vertical extent (mirror findSpine()'s endpoint
  // clipping so the returned line spans the full page).
  const double y0 = spineVirt.p1().y();
  const double y1 = spineVirt.p2().y();
  if (std::fabs(y1 - y0) > 1e-9) {
    auto pointAtY = [&](double y) {
      const double t = (y - y0) / (y1 - y0);
      return QPointF(spineVirt.p1().x() + t * (spineVirt.p2().x() - spineVirt.p1().x()), y);
    };
    const QPointF newTop = pointAtY(virtualImageRect.top());
    const QPointF newBottom = pointAtY(virtualImageRect.bottom());
    spineVirt.setP1(newTop);
    spineVirt.setP2(newBottom);
  }

  qDebug() << "SpineDarknessFinder: [PAPER-GAP] picked xCenterDs=" << pickX
           << "leftEdge=" << leftEdge << "rightEdge=" << rightEdge
           << "runLo=" << runLo << "runHi=" << runHi
           << "peak=" << runPeak
           << "leftFlankMin=" << leftFlankMin
           << "rightFlankMin=" << rightFlankMin
           << "spineVirt=" << spineVirt;

  return spineVirt;
}

}  // namespace page_split
