// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "WhiteBalance.h"

#include <QDebug>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
// Minimum margin size (pixels) to consider for sampling
const int MIN_MARGIN_SIZE = 20;

// Minimum percentage of image that margins must cover
const double MIN_MARGIN_COVERAGE = 0.05;  // 5%

// Brightness threshold for "paper-like" pixels (0-255)
const int BRIGHT_THRESHOLD = 160;

// Maximum saturation for "neutral" pixels (0-255 scale)
const int MAX_SATURATION = 60;

// Number of pixels to sample for margin/neutral detection
const int SAMPLE_COUNT = 1000;

// Helper: compute a simple local variance proxy (average absolute diff to 3x3 mean).
int localVarianceScore(const QImage& image, const int x, const int y) {
  const int w = image.width();
  const int h = image.height();
  const int x0 = std::max(0, x - 1);
  const int x1 = std::min(w - 1, x + 1);
  const int y0 = std::max(0, y - 1);
  const int y1 = std::min(h - 1, y + 1);

  int sumR = 0, sumG = 0, sumB = 0, count = 0;
  for (int yy = y0; yy <= y1; ++yy) {
    const QRgb* line = reinterpret_cast<const QRgb*>(image.scanLine(yy));
    for (int xx = x0; xx <= x1; ++xx) {
      const QRgb p = line[xx];
      sumR += qRed(p);
      sumG += qGreen(p);
      sumB += qBlue(p);
      ++count;
    }
  }
  const int meanR = sumR / count;
  const int meanG = sumG / count;
  const int meanB = sumB / count;

  int diffSum = 0;
  const QRgb center = image.pixel(x, y);
  diffSum += std::abs(qRed(center) - meanR);
  diffSum += std::abs(qGreen(center) - meanG);
  diffSum += std::abs(qBlue(center) - meanB);
  return diffSum / 3;  // average per channel
}
}  // namespace

bool WhiteBalance::hasSignificantMargins(const QImage& image, const QRect& contentBox) {
  if (contentBox.isEmpty() || !image.rect().contains(contentBox)) {
    return false;
  }

  const int imageArea = image.width() * image.height();
  const int contentArea = contentBox.width() * contentBox.height();
  const int marginArea = imageArea - contentArea;

  // Check if margins are large enough
  if (marginArea < imageArea * MIN_MARGIN_COVERAGE) {
    return false;
  }

  // Check if any margin edge is wide enough
  const int topMargin = contentBox.top();
  const int bottomMargin = image.height() - contentBox.bottom() - 1;
  const int leftMargin = contentBox.left();
  const int rightMargin = image.width() - contentBox.right() - 1;

  return (topMargin >= MIN_MARGIN_SIZE || bottomMargin >= MIN_MARGIN_SIZE || leftMargin >= MIN_MARGIN_SIZE
          || rightMargin >= MIN_MARGIN_SIZE);
}

QColor WhiteBalance::sampleMarginColor(const QImage& image, const QRect& contentBox) {
  std::vector<int> rValues, gValues, bValues;
  rValues.reserve(SAMPLE_COUNT);
  gValues.reserve(SAMPLE_COUNT);
  bValues.reserve(SAMPLE_COUNT);

  // Sample from all four margins
  const int topMargin = contentBox.top();
  const int bottomMargin = image.height() - contentBox.bottom() - 1;
  const int leftMargin = contentBox.left();
  const int rightMargin = image.width() - contentBox.right() - 1;

  auto samplePixel = [&](int x, int y) {
    const QRgb pixel = image.pixel(x, y);
    const int r = qRed(pixel);
    const int g = qGreen(pixel);
    const int b = qBlue(pixel);

    // Only include bright pixels (likely paper, not content that bleeds into margin)
    const int brightness = (r + g + b) / 3;
    if (brightness >= BRIGHT_THRESHOLD - 50) {  // Slightly lower threshold for margins
      rValues.push_back(r);
      gValues.push_back(g);
      bValues.push_back(b);
    }
  };

  // Sample from top margin
  if (topMargin >= MIN_MARGIN_SIZE) {
    for (int i = 0; i < SAMPLE_COUNT / 4 && rValues.size() < SAMPLE_COUNT; ++i) {
      const int x = rand() % image.width();
      const int y = rand() % topMargin;
      samplePixel(x, y);
    }
  }

  // Sample from bottom margin
  if (bottomMargin >= MIN_MARGIN_SIZE) {
    for (int i = 0; i < SAMPLE_COUNT / 4 && rValues.size() < SAMPLE_COUNT; ++i) {
      const int x = rand() % image.width();
      const int y = contentBox.bottom() + 1 + (rand() % bottomMargin);
      samplePixel(x, y);
    }
  }

  // Sample from left margin
  if (leftMargin >= MIN_MARGIN_SIZE) {
    for (int i = 0; i < SAMPLE_COUNT / 4 && rValues.size() < SAMPLE_COUNT; ++i) {
      const int x = rand() % leftMargin;
      const int y = rand() % image.height();
      samplePixel(x, y);
    }
  }

  // Sample from right margin
  if (rightMargin >= MIN_MARGIN_SIZE) {
    for (int i = 0; i < SAMPLE_COUNT / 4 && rValues.size() < SAMPLE_COUNT; ++i) {
      const int x = contentBox.right() + 1 + (rand() % rightMargin);
      const int y = rand() % image.height();
      samplePixel(x, y);
    }
  }

  if (rValues.empty()) {
    return QColor();  // Invalid - no samples collected
  }

  // Use median for robustness against outliers
  std::sort(rValues.begin(), rValues.end());
  std::sort(gValues.begin(), gValues.end());
  std::sort(bValues.begin(), bValues.end());

  const size_t mid = rValues.size() / 2;
  return QColor(rValues[mid], gValues[mid], bValues[mid]);
}

QColor WhiteBalance::findNeutralPixels(const QImage& image) {
  std::vector<int> rValues, gValues, bValues;
  rValues.reserve(SAMPLE_COUNT);
  gValues.reserve(SAMPLE_COUNT);
  bValues.reserve(SAMPLE_COUNT);

  // Sample random pixels looking for bright, low-saturation ones
  for (int attempts = 0; attempts < SAMPLE_COUNT * 10 && rValues.size() < SAMPLE_COUNT; ++attempts) {
    const int x = rand() % image.width();
    const int y = rand() % image.height();
    const QRgb pixel = image.pixel(x, y);
    const int r = qRed(pixel);
    const int g = qGreen(pixel);
    const int b = qBlue(pixel);

    // Check brightness
    const int brightness = (r + g + b) / 3;
    if (brightness < BRIGHT_THRESHOLD) {
      continue;
    }

    // Check saturation (max - min channel difference)
    const int maxC = std::max({r, g, b});
    const int minC = std::min({r, g, b});
    const int saturation = maxC - minC;
    if (saturation > MAX_SATURATION) {
      continue;
    }

    rValues.push_back(r);
    gValues.push_back(g);
    bValues.push_back(b);
  }

  if (rValues.size() < 10) {
    return QColor();  // Not enough neutral pixels found
  }

  // Use median
  std::sort(rValues.begin(), rValues.end());
  std::sort(gValues.begin(), gValues.end());
  std::sort(bValues.begin(), bValues.end());

  const size_t mid = rValues.size() / 2;
  return QColor(rValues[mid], gValues[mid], bValues[mid]);
}

QColor WhiteBalance::detectPaperColor(const QImage& image, const QRect& contentBox) {
  if (image.isNull()) {
    return QColor();
  }

  // Only sample from margins - this is the safest approach.
  // Sampling from the full image risks picking up content colors
  // (e.g., sepia engravings, colored backgrounds) as "paper".
  if (hasSignificantMargins(image, contentBox)) {
    QColor marginColor = sampleMarginColor(image, contentBox);
    if (marginColor.isValid()) {
      qDebug() << "WhiteBalance: detected paper color from margins:" << marginColor;
      return marginColor;
    }
  }

  qDebug() << "WhiteBalance: no margins available, skipping auto white balance";
  return QColor();
}

bool WhiteBalance::hasSignificantCast(const QColor& paperColor, int threshold) {
  if (!paperColor.isValid()) {
    return false;
  }

  const int r = paperColor.red();
  const int g = paperColor.green();
  const int b = paperColor.blue();

  // Check deviation from neutral (where R == G == B)
  const int avg = (r + g + b) / 3;
  const int maxDev = std::max({std::abs(r - avg), std::abs(g - avg), std::abs(b - avg)});

  return maxDev >= threshold;
}

QColor WhiteBalance::findBrightestPixels(const QImage& image) {
  if (image.isNull()) {
    return QColor();
  }

  // Collect bright, low-saturation, low-variance pixels (prefer paper-like areas)
  struct PixelSample {
    int r, g, b;
    int brightness;
  };
  std::vector<PixelSample> samples;
  samples.reserve(SAMPLE_COUNT);

  // Sample random pixels, excluding pure white and very dark, and require low saturation / low variance
  for (int i = 0; i < SAMPLE_COUNT * 20 && samples.size() < SAMPLE_COUNT; ++i) {
    const int x = rand() % image.width();
    const int y = rand() % image.height();
    const QRgb pixel = image.pixel(x, y);
    const int r = qRed(pixel);
    const int g = qGreen(pixel);
    const int b = qBlue(pixel);
    const int brightness = (r + g + b) / 3;

    // Skip very dark pixels (content, not paper)
    if (brightness < 170) {
      continue;
    }

    // Skip near-pure-white pixels (margins, already correct)
    // These would mask the color cast we want to fix
    if (r > 250 && g > 250 && b > 250) {
      continue;
    }

    const int maxC = std::max({r, g, b});
    const int minC = std::min({r, g, b});
    const int saturation = maxC - minC;
    if (saturation > 40) {  // prefer low-sat bright areas
      continue;
    }

    // Require locally uniform area to avoid shiny highlights
    const int varScore = localVarianceScore(image, x, y);
    if (varScore > 20) {
      continue;
    }

    samples.push_back({r, g, b, brightness});
  }

  if (samples.size() < 10) {
    qDebug() << "WhiteBalance: not enough bright content pixels found";
    return QColor();
  }

  // Sort by brightness (descending) and keep top 20%
  std::sort(samples.begin(), samples.end(),
            [](const PixelSample& a, const PixelSample& b) { return a.brightness > b.brightness; });

  const size_t keepCount = std::max(size_t(10), samples.size() / 5);

  // Get median of the brightest content pixels
  std::vector<int> rValues, gValues, bValues;
  rValues.reserve(keepCount);
  gValues.reserve(keepCount);
  bValues.reserve(keepCount);

  for (size_t i = 0; i < keepCount; ++i) {
    rValues.push_back(samples[i].r);
    gValues.push_back(samples[i].g);
    bValues.push_back(samples[i].b);
  }

  std::sort(rValues.begin(), rValues.end());
  std::sort(gValues.begin(), gValues.end());
  std::sort(bValues.begin(), bValues.end());

  const size_t mid = rValues.size() / 2;
  QColor result(rValues[mid], gValues[mid], bValues[mid]);

  // If our "brightest" sample is still too dark, it's likely not paper; skip.
  const int brightness = (result.red() + result.green() + result.blue()) / 3;
  if (brightness < 180) {
    qDebug() << "WhiteBalance: brightest sample too dark for forced WB, skipping";
    return QColor();
  }

  qDebug() << "WhiteBalance: brightest content pixels color:" << result
           << "from" << samples.size() << "samples";
  return result;
}

QImage WhiteBalance::apply(const QImage& image, const QColor& paperColor) {
  if (image.isNull() || !paperColor.isValid()) {
    return image;
  }

  // Don't correct if already neutral
  if (!hasSignificantCast(paperColor)) {
    qDebug() << "WhiteBalance: paper color is already neutral, skipping correction";
    return image;
  }

  const int paperR = paperColor.red();
  const int paperG = paperColor.green();
  const int paperB = paperColor.blue();

  // Avoid division by zero
  if (paperR == 0 || paperG == 0 || paperB == 0) {
    return image;
  }

  // Target a near-white neutral value so paper brightens toward white.
  const float targetNeutral = 245.0f;

  float rMult = targetNeutral / paperR;
  float gMult = targetNeutral / paperG;
  float bMult = targetNeutral / paperB;

  // Clamp multipliers to a reasonable range to prevent extreme shifts.
  auto clampGain = [](float v) { return std::clamp(v, 0.2f, 6.0f); };
  rMult = clampGain(rMult);
  gMult = clampGain(gMult);
  bMult = clampGain(bMult);

  qDebug() << "WhiteBalance: applying correction - R*" << rMult << "G*" << gMult << "B*" << bMult;

  // Apply correction to all pixels
  QImage result = image.convertToFormat(QImage::Format_RGB32);
  const int width = result.width();
  const int height = result.height();

  for (int y = 0; y < height; ++y) {
    QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
    for (int x = 0; x < width; ++x) {
      const QRgb pixel = line[x];
      const int r = std::min(255, static_cast<int>(qRed(pixel) * rMult));
      const int g = std::min(255, static_cast<int>(qGreen(pixel) * gMult));
      const int b = std::min(255, static_cast<int>(qBlue(pixel) * bMult));
      line[x] = qRgb(r, g, b);
    }
  }

  return result;
}
