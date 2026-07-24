// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "LeptonicaDetector.h"

#include <QImage>
#include <QImageReader>
#include <cstdio>

#include <leptonica/allheaders.h>

#include "WhiteBalance.h"

namespace {

/**
 * Convert QImage to Leptonica PIX format.
 * Caller must call pixDestroy() on the returned PIX.
 */
PIX* qImageToPix(const QImage& qimg) {
  QImage img = qimg;

  // Convert to 32-bit ARGB if needed
  if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32) {
    img = img.convertToFormat(QImage::Format_ARGB32);
  }

  const int w = img.width();
  const int h = img.height();

  PIX* pix = pixCreate(w, h, 32);
  if (!pix) return nullptr;

  l_uint32* pixData = pixGetData(pix);
  const int wpl = pixGetWpl(pix);

  for (int y = 0; y < h; y++) {
    const QRgb* scanline = reinterpret_cast<const QRgb*>(img.constScanLine(y));
    l_uint32* line = pixData + y * wpl;
    for (int x = 0; x < w; x++) {
      QRgb pixel = scanline[x];
      // Leptonica uses RGBA format
      composeRGBAPixel(qRed(pixel), qGreen(pixel), qBlue(pixel), 255, line + x);
    }
  }

  return pix;
}

/**
 * Remove a dominant low-saturation paper cast without desaturating unrelated
 * colors. A single RGB gain neutralizes one sampled paper brightness, but real
 * book scans have shadows and scanner-response gradients whose chroma remains
 * above pixColorFraction's threshold after that correction.
 *
 * Pixels are neutralized only when their chroma points in nearly the same
 * direction as the sampled paper cast and is no more than three times as
 * strong. Saturated inks and photo colors therefore remain available to the
 * detector.
 */
QImage neutralizeDominantPaperCast(const QImage& image, const QColor& paperColor) {
  if (image.isNull() || !paperColor.isValid()) {
    return image;
  }

  const int paperAverage = (paperColor.red() + paperColor.green() + paperColor.blue()) / 3;
  const int paperCastR = paperColor.red() - paperAverage;
  const int paperCastG = paperColor.green() - paperAverage;
  const int paperCastB = paperColor.blue() - paperAverage;
  const qint64 paperNorm2 = static_cast<qint64>(paperCastR) * paperCastR
                           + static_cast<qint64>(paperCastG) * paperCastG
                           + static_cast<qint64>(paperCastB) * paperCastB;
  if (paperNorm2 < 25) {
    return image;
  }

  QImage result = image.convertToFormat(QImage::Format_RGB32);
  for (int y = 0; y < result.height(); ++y) {
    QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
    for (int x = 0; x < result.width(); ++x) {
      const int red = qRed(line[x]);
      const int green = qGreen(line[x]);
      const int blue = qBlue(line[x]);
      const int average = (red + green + blue) / 3;
      const int castR = red - average;
      const int castG = green - average;
      const int castB = blue - average;
      const qint64 norm2 = static_cast<qint64>(castR) * castR
                           + static_cast<qint64>(castG) * castG
                           + static_cast<qint64>(castB) * castB;
      if (norm2 == 0 || norm2 > paperNorm2 * 9) {
        continue;
      }

      const qint64 dot = static_cast<qint64>(castR) * paperCastR
                         + static_cast<qint64>(castG) * paperCastG
                         + static_cast<qint64>(castB) * paperCastB;
      // cos(angle) >= 0.90, expressed without a square root.
      if (dot > 0 && dot * dot * 100 >= norm2 * paperNorm2 * 81) {
        line[x] = qRgb(average, average, average);
      }
    }
  }
  return result;
}

/**
 * Check if a region of a brightness-normalized grayscale image has high
 * midtone concentration.
 * Used to detect embedded images/illustrations in otherwise B&W pages.
 * Returns true if any interior grid cell has >50% midtones.
 * Skips edge cells which often contain page gutters, binding shadows, or margins.
 */
bool hasHighMidtoneRegion(PIX* gray) {
  const int w = pixGetWidth(gray);
  const int h = pixGetHeight(gray);

  // Use 6x6 grid (36 cells) for region analysis
  const int gridSize = 6;
  const int cellW = w / gridSize;
  const int cellH = h / gridSize;

  // Skip if cells would be too small
  if (cellW < 50 || cellH < 50) {
    return false;
  }

  l_uint32* pixData = pixGetData(gray);
  const int wpl = pixGetWpl(gray);

  // Check only interior grid cells (skip edges which often have margins/gutters)
  // With 6x6 grid, check cells [1,1] through [4,4] (16 interior cells)
  for (int gy = 1; gy < gridSize - 1; gy++) {
    for (int gx = 1; gx < gridSize - 1; gx++) {
      int startX = gx * cellW;
      int startY = gy * cellH;
      int endX = (gx == gridSize - 1) ? w : startX + cellW;
      int endY = (gy == gridSize - 1) ? h : startY + cellH;

      int cellMidtones = 0;
      int cellTotal = 0;

      // Sample every 4th pixel for speed
      for (int y = startY; y < endY; y += 4) {
        l_uint32* line = pixData + y * wpl;
        for (int x = startX; x < endX; x += 4) {
          // For 8-bit grayscale, get pixel value
          l_uint32 val = GET_DATA_BYTE(line, x);
          cellTotal++;
          // Midtone range: 60-195
          if (val > 60 && val < 195) {
            cellMidtones++;
          }
        }
      }

      if (cellTotal > 0) {
        float cellMidtoneRatio = (float)cellMidtones / cellTotal;
        // The 150-DPI census of all 392 pages in the toned-paper regression
        // book topped out at 41.8% for a dense text cell after brightness
        // normalization. The continuous-tone fixture exceeds 50%, leaving
        // an evidence-backed gap instead of counting a page-wide paper slope.
        if (cellMidtoneRatio > 0.50f) {
          fprintf(stderr, "  Region [%d,%d]: %.1f%% midtones -> embedded image detected\n",
                  gx, gy, cellMidtoneRatio * 100);
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Check if image is effectively B&W (bimodal histogram with pixels at extremes).
 * This detects text documents where pixels cluster near black and white,
 * allowing for anti-aliasing and scan noise.
 *
 * @param pix The Leptonica PIX image
 * @param midtoneRatio Output: the fraction of midtone pixels
 * @param midtoneThreshold Minimum midtone % to trigger region-based detection
 */
bool isPureBW(PIX* pix, float* midtoneRatio, int midtoneThreshold = 10) {
  (void)midtoneThreshold;
  // Normalize low-frequency brightness before measuring tone. This flattens
  // shadows and paper gradients while retaining local photograph detail.
  PIX* gray = pixConvertRGBToGray(pix, 0.0, 0.0, 0.0);
  if (!gray) return false;
  PIX* normalized = pixBackgroundNormSimple(gray, nullptr, nullptr);
  if (normalized) {
    pixDestroy(&gray);
    gray = normalized;
  }

  // Get histogram
  NUMA* histo = pixGetGrayHistogram(gray, 1);
  if (!histo) {
    pixDestroy(&gray);
    return false;
  }

  l_int32 n = numaGetCount(histo);
  l_float32 total = 0;
  l_float32 darks = 0;      // 0-79: black zone (text + dark edges)
  l_float32 midtones = 0;   // 80-129: true midtone zone (photos, illustrations)
  l_float32 lights = 0;     // 130-255: white zone (paper, including yellowed/grayish)

  for (int i = 0; i < n; i++) {
    l_float32 val;
    numaGetFValue(histo, i, &val);
    total += val;
    if (i <= 79) {
      darks += val;
    } else if (i >= 130) {
      lights += val;  // Lowered from 150 to include grayish/yellowed paper
    } else {
      midtones += val;
    }
  }

  numaDestroy(&histo);

  if (total == 0) return false;

  float midRatio = midtones / total;
  if (midtoneRatio) *midtoneRatio = midRatio;

  // B&W if:
  // 1. Less than 45% of pixels in true midtone zone (80-149)
  // 2. At least 10% in darks AND at least 30% in lights (bimodal)
  //    OR at least 70% in one extreme (mostly white or mostly black page)
  float darkRatio = darks / total;
  float lightRatio = lights / total;

  // 45% threshold - tolerant of anti-aliasing, paper texture, yellowing
  bool lowMidtones = midRatio < 0.45f;
  bool bimodal = (darkRatio > 0.10f && lightRatio > 0.30f);  // Text on white
  bool extremelyLight = lightRatio > 0.70f;  // Nearly blank page

  // Before declaring B&W, ALWAYS check for embedded images using region-based detection.
  // This catches photographs embedded in pages with lots of white space, where overall
  // midtone % is low but a localized region has significant midtone content.
  // Only skip if midtones are truly negligible (<1%) - pure text pages.
  if (lowMidtones && (bimodal || extremelyLight)) {
    if (midRatio >= 0.01f) {  // At least 1% midtones - worth checking regions
      fprintf(stderr, "  Candidate B&W (%.1f%% midtones), checking regions for embedded images...\n",
              midRatio * 100);
      if (hasHighMidtoneRegion(gray)) {
        // Found a region with concentrated midtones - likely an embedded image
        fprintf(stderr, "  -> Embedded image detected, treating as grayscale\n");
        pixDestroy(&gray);
        return false;  // Not B&W, treat as grayscale
      }
    }
    pixDestroy(&gray);
    return true;  // B&W
  }

  pixDestroy(&gray);
  return false;  // Not B&W (high midtones overall)
}

/**
 * Check if image is grayscale (R≈G≈B for all pixels).
 */
bool isGrayscale(PIX* pix, float* colorFraction) {
  l_float32 pixfract = 0.0f;
  l_float32 colorfract = 0.0f;

  // pixColorFraction analyzes what fraction of pixels have color
  // darkthresh: ignore pixels darker than this (avoid noise in shadows)
  // lightthresh: ignore pixels lighter than this (avoid noise in highlights)
  // diffthresh: minimum R-G, R-B, G-B difference to count as "color"
  // factor: subsampling factor for speed
  l_int32 result = pixColorFraction(pix,
                                     10,    // darkthresh - lowered from 20 to include dark photos
                                     240,   // lightthresh - raised from 235 to include more highlights
                                     50,    // diffthresh - raised from 35 to tolerate heavily yellowed old paper
                                     4,     // factor (subsample for speed)
                                     &pixfract,   // fraction of pixels analyzed
                                     &colorfract); // fraction of those that are color

  if (result != 0) {
    // Error - assume grayscale
    if (colorFraction) *colorFraction = 0.0f;
    return true;
  }

  if (colorFraction) *colorFraction = colorfract;

  // If less than 3% of analyzed pixels have significant color, it's grayscale
  // Lowered from 10% to catch photos with muted colors and dark backgrounds
  return colorfract < 0.03f;
}

}  // namespace

LeptonicaDetector::ColorType LeptonicaDetector::detect(const QImage& image, int midtoneThreshold) {
  if (image.isNull()) {
    return ColorType::Grayscale;
  }

  PIX* pix = qImageToPix(image);
  if (!pix) {
    return ColorType::Grayscale;
  }

  float colorFraction = 0.0f;
  bool grayscale = isGrayscale(pix, &colorFraction);

  ColorType result;
  float midtoneRatio = 0.0f;
  const bool pureBW = grayscale && isPureBW(pix, &midtoneRatio, midtoneThreshold);
  if (!grayscale) {
    result = ColorType::Color;
    fprintf(stderr, "LeptonicaDetector: COLOR (%.1f%% color pixels)\n", colorFraction * 100);
  } else if (pureBW) {
    result = ColorType::BlackWhite;
    fprintf(stderr, "LeptonicaDetector: B&W (%.1f%% midtones)\n", midtoneRatio * 100);
  } else {
    result = ColorType::Grayscale;
    fprintf(stderr, "LeptonicaDetector: GRAYSCALE (%.1f%% color, %.1f%% midtones)\n",
            colorFraction * 100, midtoneRatio * 100);
  }

  pixDestroy(&pix);
  return result;
}

LeptonicaDetector::ColorType LeptonicaDetector::detectWithCastCompensation(const QImage& image,
                                                                           const int midtoneThreshold) {
  ColorType result = detect(image, midtoneThreshold);
  if (result != ColorType::Color) {
    return result;
  }

  // Classified Color - but a uniform paper tint (aged/toned stock) can push
  // every paper pixel over pixColorFraction's diff threshold. Estimate the
  // background; if it carries a significant cast, neutralize and re-detect.
  const QColor background = WhiteBalance::estimateBackgroundColor(image);
  if (!background.isValid() || !WhiteBalance::hasSignificantCast(background)) {
    return result;  // Neutral background - the color is genuine content
  }

  fprintf(stderr, "LeptonicaDetector: COLOR verdict with tinted background (%d,%d,%d), re-detecting neutralized\n",
          background.red(), background.green(), background.blue());
  const QImage neutralized = WhiteBalance::apply(image, background);
  ColorType recheck = detect(neutralized, midtoneThreshold);
  if (recheck == ColorType::Color) {
    float residualColorFraction = 1.0f;
    PIX* neutralizedPix = qImageToPix(neutralized);
    if (neutralizedPix) {
      isGrayscale(neutralizedPix, &residualColorFraction);
      pixDestroy(&neutralizedPix);
    }

    // Only a modest residual is plausibly a brightness-varying paper cast.
    // A large surviving fraction is real page-level color even when its hues
    // are related to the paper (for example, a yellow library card).
    if (residualColorFraction < 0.20f) {
      const QImage selectivelyNeutralized = neutralizeDominantPaperCast(image, background);
      recheck = detect(selectivelyNeutralized, midtoneThreshold);
    }
  }
  if (recheck != ColorType::Color) {
    fprintf(stderr, "LeptonicaDetector: color did not survive tint neutralization -> %s\n",
            colorTypeToString(recheck));
    return recheck;
  }
  return result;
}

LeptonicaDetector::ColorType LeptonicaDetector::detectFromFile(const QString& imagePath) {
  QImageReader reader(imagePath);
  if (!reader.canRead()) {
    return ColorType::Grayscale;
  }

  // Downsample for speed
  const QSize originalSize = reader.size();
  const int maxDim = 1200;
  if (originalSize.width() > maxDim || originalSize.height() > maxDim) {
    const qreal scale = qMin(static_cast<qreal>(maxDim) / originalSize.width(),
                             static_cast<qreal>(maxDim) / originalSize.height());
    reader.setScaledSize(QSize(qRound(originalSize.width() * scale),
                               qRound(originalSize.height() * scale)));
  }

  const QImage image = reader.read();
  return detect(image);
}

const char* LeptonicaDetector::colorTypeToString(ColorType type) {
  switch (type) {
    case ColorType::BlackWhite: return "bw";
    case ColorType::Grayscale: return "grayscale";
    case ColorType::Color: return "color";
  }
  return "unknown";
}
