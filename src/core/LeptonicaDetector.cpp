// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "LeptonicaDetector.h"

#include <QImage>
#include <QImageReader>
#include <cstdio>

#include <leptonica/allheaders.h>

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
 * Check if a region of the image has high midtone concentration.
 * Used to detect embedded images/illustrations in otherwise B&W pages.
 * Returns true if any interior grid cell has >30% midtones.
 * Skips edge cells which often contain page gutters, binding shadows, or margins.
 */
bool hasHighMidtoneRegion(PIX* pix) {
  // Convert to grayscale for analysis
  PIX* gray = pixConvertRGBToGray(pix, 0.0, 0.0, 0.0);
  if (!gray) return false;

  const int w = pixGetWidth(gray);
  const int h = pixGetHeight(gray);

  // Use 6x6 grid (36 cells) for region analysis
  const int gridSize = 6;
  const int cellW = w / gridSize;
  const int cellH = h / gridSize;

  // Skip if cells would be too small
  if (cellW < 50 || cellH < 50) {
    pixDestroy(&gray);
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
        // If any cell has >30% midtones, there's likely an embedded image
        if (cellMidtoneRatio > 0.30f) {
          fprintf(stderr, "  Region [%d,%d]: %.1f%% midtones -> embedded image detected\n",
                  gx, gy, cellMidtoneRatio * 100);
          pixDestroy(&gray);
          return true;
        }
      }
    }
  }

  pixDestroy(&gray);
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
  // Convert to grayscale for analysis
  PIX* gray = pixConvertRGBToGray(pix, 0.0, 0.0, 0.0);
  if (!gray) return false;

  // Get histogram
  NUMA* histo = pixGetGrayHistogram(gray, 1);
  pixDestroy(&gray);
  if (!histo) return false;

  l_int32 n = numaGetCount(histo);
  l_float32 total = 0;
  l_float32 darks = 0;      // 0-70: black zone (text)
  l_float32 midtones = 0;   // 70-170: true midtone zone (indicates tonal content like photos)
  l_float32 lights = 0;     // 170-255: white zone (paper, even if slightly yellowed)

  for (int i = 0; i < n; i++) {
    l_float32 val;
    numaGetFValue(histo, i, &val);
    total += val;
    if (i <= 70) {
      darks += val;
    } else if (i >= 170) {
      lights += val;
    } else {
      midtones += val;
    }
  }

  numaDestroy(&histo);

  if (total == 0) return false;

  float midRatio = midtones / total;
  if (midtoneRatio) *midtoneRatio = midRatio;

  // B&W if:
  // 1. Less than 15% of pixels in true midtone zone (60-195)
  // 2. At least 30% in darks AND at least 30% in lights (bimodal)
  //    OR at least 70% in one extreme (mostly white or mostly black page)
  float darkRatio = darks / total;
  float lightRatio = lights / total;

  // Increased from 15% to 40% to be more lenient with scanned text pages
  // that have anti-aliasing, paper texture, and residual yellowing
  bool lowMidtones = midRatio < 0.40f;
  bool bimodal = (darkRatio > 0.10f && lightRatio > 0.30f);  // Text on white
  bool extremelyLight = lightRatio > 0.70f;  // Nearly blank page

  // For borderline cases (threshold-40% midtones), check for embedded images
  // using region-based detection
  float thresholdFloat = midtoneThreshold / 100.0f;
  if (midRatio >= thresholdFloat && midRatio < 0.40f) {
    fprintf(stderr, "  Borderline case (%.1f%% midtones, threshold=%d%%), checking regions...\n",
            midRatio * 100, midtoneThreshold);
    if (hasHighMidtoneRegion(pix)) {
      // Found a region with concentrated midtones - likely an embedded image
      return false;  // Not B&W, treat as grayscale
    }
  }

  return lowMidtones && (bimodal || extremelyLight);
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
                                     18,    // diffthresh - lowered from 25 to detect muted colors (browns, tans)
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
  if (!grayscale) {
    result = ColorType::Color;
    fprintf(stderr, "LeptonicaDetector: COLOR (%.1f%% color pixels)\n", colorFraction * 100);
  } else if (isPureBW(pix, &midtoneRatio, midtoneThreshold)) {
    result = ColorType::BlackWhite;
    fprintf(stderr, "LeptonicaDetector: B&W (%.1f%% midtones)\n", midtoneRatio * 100);
  } else {
    // Check midtones even if not B&W for logging
    isPureBW(pix, &midtoneRatio, midtoneThreshold);
    result = ColorType::Grayscale;
    fprintf(stderr, "LeptonicaDetector: GRAYSCALE (%.1f%% color, %.1f%% midtones)\n",
            colorFraction * 100, midtoneRatio * 100);
  }

  pixDestroy(&pix);
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
