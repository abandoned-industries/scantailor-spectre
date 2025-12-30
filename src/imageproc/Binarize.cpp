// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Binarize.h"

#include <QDebug>
#include <cassert>
#include <cmath>
#include <deque>
#include <stdexcept>

#include "BinaryImage.h"
#include "GaussBlur.h"
#include "GrayImage.h"
#include "Grayscale.h"
#include "IntegralImage.h"

#ifdef Q_OS_MACOS
#include <dispatch/dispatch.h>
#endif

namespace imageproc {
BinaryImage binarizeOtsu(const QImage& src) {
  return BinaryImage(src, BinaryThreshold::otsuThreshold(src));
}

BinaryImage binarizeMokji(const QImage& src, const unsigned maxEdgeWidth, const unsigned minEdgeMagnitude) {
  const BinaryThreshold threshold(BinaryThreshold::mokjiThreshold(src, maxEdgeWidth, minEdgeMagnitude));
  return BinaryImage(src, threshold);
}

/*
 * sauvola = mean * (1.0 + k * (stderr / 128.0 - 1.0)), k = 0.34
 * modification by zvezdochiot:
 * sauvola = mean * (1.0 + k * ((stderr + delta) / 128.0 - 1.0)), k = 0.34, delta = 0
 */
BinaryImage binarizeSauvola(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeSauvola: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqimage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqimage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqimage.push(pixel * pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  BinaryImage bwImg(w, h);
  uint32_t* bwData = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();
  const uint8_t* grayData = gray.bits();

#ifdef Q_OS_MACOS
  // Parallel row processing using Grand Central Dispatch
  // Capture pointers to integral images (read-only, thread-safe)
  const IntegralImage<uint32_t>* integralPtr = &integralImage;
  const IntegralImage<uint64_t>* integralSqPtr = &integralSqimage;

  dispatch_apply(h, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t y) {
    const int top = std::max(0, (int)y - windowLowerHalf);
    const int bottom = std::min(h, (int)y + windowUpperHalf);
    const uint8_t* grayRow = grayData + y * grayBpl;
    uint32_t* bwRow = bwData + y * bwWpl;

    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralPtr->sum(rect);
      const double windowSqsum = integralSqPtr->sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;

      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));

      const double threshold = mean * (1.0 + k * ((deviation + delta) / 128.0 - 1.0));

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayRow[x]) < threshold) {
        bwRow[x >> 5] |= mask;
      } else {
        bwRow[x >> 5] &= ~mask;
      }
    }
  });
#else
  grayLine = gray.bits();
  uint32_t* bwLine = bwData;
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);  // exclusive
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);  // exclusive
      const int area = (bottom - top) * (right - left);
      assert(area > 0);  // because windowSize > 0 and w > 0 and h > 0
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqsum = integralSqimage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;

      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));

      const double threshold = mean * (1.0 + k * ((deviation + delta) / 128.0 - 1.0));

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        // black
        bwLine[x >> 5] |= mask;
      } else {
        // white
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
#endif
  return bwImg;
}  // binarizeSauvola

/*
 * wolf = mean - k * (mean - min_v) * (1.0 - stderr / stdmax), k = 0.3
 * modification by zvezdochiot:
 * wolf = mean - k * (mean - min_v) * (1.0 - (stderr / stdmax + delta / 128.0), k = 0.3, delta = 0
 */
BinaryImage binarizeWolf(const QImage& src,
                         const QSize windowSize,
                         const unsigned char lowerBound,
                         const unsigned char upperBound,
                         const double k,
                         const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeWolf: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqimage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  uint32_t minGrayLevel = 255;

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqimage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqimage.push(pixel * pixel);
      minGrayLevel = std::min(minGrayLevel, pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  std::vector<float> means(w * h, 0);
  std::vector<float> deviations(w * h, 0);

  double maxDeviation = 0;

  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);  // exclusive
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);  // exclusive
      const int area = (bottom - top) * (right - left);
      assert(area > 0);  // because windowSize > 0 and w > 0 and h > 0
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqsum = integralSqimage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;

      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));
      maxDeviation = std::max(maxDeviation, deviation);
      means[w * y + x] = (float) mean;
      deviations[w * y + x] = (float) deviation;
    }
  }

  // TODO: integral images can be disposed at this point.

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float mean = means[y * w + x];
      const float deviation = deviations[y * w + x];
      const double a = 1.0 - (deviation / maxDeviation + (double) delta / 128.0);
      const double threshold = mean - k * a * (mean - minGrayLevel);

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if ((grayLine[x] < lowerBound) || ((grayLine[x] <= upperBound) && (int(grayLine[x]) < threshold))) {
        // black
        bwLine[x >> 5] |= mask;
      } else {
        // white
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeWolf

BinaryImage binarizeBradley(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeBradley: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);

  uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);  // exclusive
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);  // exclusive
      const int area = (bottom - top) * (right - left);
      assert(area > 0);  // because windowSize > 0 and w > 0 and h > 0
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double threshold = (k < 1.0) ? (mean * (1.0 - k)) : 0;
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < (threshold + delta)) {
        // black
        bwLine[x >> 5] |= mask;
      } else {
        // white
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeBradley

/*
 * grad = mean * k + meanG * (1.0 - k), meanG = mean(I * G) / mean(G), G = |I - mean|, k = 0.75
 * modification by zvezdochiot:
 * mean = mean + delta, delta = 0
 */
BinaryImage binarizeGrad(const QImage& src,
                         const QSize windowSize,
                         const unsigned char lowerBound,
                         const unsigned char upperBound,
                         const double k,
                         const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeGrad: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  QImage gmean(toGrayscale(src));
  if (gmean.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  uint8_t* gmeanLine = gmean.bits();
  const int gmeanBpl = gmean.bytesPerLine();

  IntegralImage<uint32_t> integralImage(w, h);

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);  // exclusive
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);  // exclusive
      const int area = (bottom - top) * (right - left);
      assert(area > 0);  // because windowSize > 0 and w > 0 and h > 0
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea + 0.5 + delta;
      const int imean = (int) ((mean < 0.0) ? 0.0 : (mean < 255.0) ? mean : 255.0);
      gmeanLine[x] = imean;
    }
    gmeanLine += gmeanBpl;
  }

  double gvalue = 127.5;
  double sum_g = 0.0, sum_gi = 0.0;
  grayLine = gray.bits();
  gmeanLine = gmean.bits();
  for (int y = 0; y < h; y++) {
    double sum_gl = 0.0;
    double sum_gil = 0.0;
    for (int x = 0; x < w; x++) {
      double gi = grayLine[x];
      double g = gmeanLine[x];
      g -= gi;
      g = (g < 0.0) ? -g : g;
      gi *= g;
      sum_gl += g;
      sum_gil += gi;
    }
    sum_g += sum_gl;
    sum_gi += sum_gil;
    grayLine += grayBpl;
    gmeanLine += gmeanBpl;
  }
  gvalue = (sum_g > 0.0) ? (sum_gi / sum_g) : gvalue;

  double const meanGrad = gvalue * (1.0 - k);

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  gmeanLine = gmean.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double origin = grayLine[x];
      const double mean = gmeanLine[x];
      const double threshold = meanGrad + mean * k;
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if ((grayLine[x] < lowerBound) || ((grayLine[x] <= upperBound) && (origin < threshold))) {
        // black
        bwLine[x >> 5] |= mask;
      } else {
        // white
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    gmeanLine += gmeanBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeGrad

/*
 * edgediv == EdgeDiv image prefilter before the Otsu threshold
 */
BinaryImage binarizeEdgeDiv(const QImage& src,
                            const QSize windowSize,
                            const double kep,
                            const double kbd,
                            const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeBlurDiv: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);

  uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);  // exclusive
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);  // exclusive
      const int area = (bottom - top) * (right - left);
      assert(area > 0);  // because windowSize > 0 and w > 0 and h > 0
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double origin = grayLine[x];
      double retval = origin;
      if (kep > 0.0) {
        // EdgePlus
        // edge = I / blur (shift = -0.5) {0.0 .. >1.0}, mean value = 0.5
        const double edge = (retval + 1) / (mean + 1) - 0.5;
        // edgeplus = I * edge, mean value = 0.5 * mean(I)
        const double edgeplus = origin * edge;
        // return k * edgeplus + (1 - k) * I
        retval = kep * edgeplus + (1.0 - kep) * origin;
      }
      if (kbd > 0.0) {
        // BlurDiv
        // edge = blur / I (shift = -0.5) {0.0 .. >1.0}, mean value = 0.5
        const double edgeinv = (mean + 1) / (retval + 1) - 0.5;
        // edgenorm = edge * k + max * (1 - k), mean value = {0.5 .. 1.0} * mean(I)
        const double edgenorm = kbd * edgeinv + (1.0 - kbd);
        // return I / edgenorm
        retval = (edgenorm > 0.0) ? (origin / edgenorm) : origin;
      }
      // trim value {0..255}
      retval = (retval < 0.0) ? 0.0 : (retval < 255.0) ? retval : 255.0;
      grayLine[x] = (int) retval;
    }
    grayLine += grayBpl;
  }
  return BinaryImage(gray, (BinaryThreshold::otsuThreshold(gray) + delta));
}  // binarizeBlurDiv

/*
 * niblack = mean - k * stdev, k = 0.2
 * modification: niblack = mean - k * (stdev - delta), delta = 0
 */
BinaryImage binarizeNiblack(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeNiblack: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqimage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqimage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqimage.push(pixel * pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqsum = integralSqimage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;

      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));

      // Niblack: threshold = mean - k * (deviation - delta)
      const double threshold = mean - k * (deviation - delta);

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeNiblack

/*
 * N.I.C.K = mean - k * sqrt(stdev^2 + c*mean^2), k = 0.10
 * c = (50 - delta) * 0.01, where delta adjusts the c parameter
 */
BinaryImage binarizeNick(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeNick: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqimage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqimage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqimage.push(pixel * pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  // c parameter for NICK algorithm, controlled by delta
  // delta=0 gives c=0.5, delta=-50 gives c=1.0, delta=50 gives c=0.0
  const double c = (50.0 - delta) * 0.01;

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqsum = integralSqimage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;

      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));

      // NICK: threshold = mean - k * sqrt(stdev^2 + c*mean^2)
      const double circle = std::sqrt(deviation * deviation + c * mean * mean);
      const double threshold = mean - k * circle;

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeNick

/*
 * Singh: threshold = mean * (1.0 - k * 0.5 * (1.0 - (frac_s + frac_d)))
 * where frac_s = (pixel - mean) / (256 - (pixel - mean))
 * frac_d = delta / 128.0
 */
BinaryImage binarizeSingh(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeSingh: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  const double fracDelta = delta / 128.0;

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double origin = grayLine[x];

      // Singh: uses difference between pixel and local mean
      const double diff = origin - mean;
      const double absDiff = std::fabs(diff);
      // frac_s = dI / (256 - dI) where dI = |origin - mean|
      const double fracS = absDiff / (256.0 - absDiff + 1e-6);  // avoid division by zero
      // threshold = mean * (1.0 - k * 0.5 * (1.0 - (frac_s + frac_d)))
      const double part = k * 0.5 * (1.0 - (fracS + fracDelta));
      const double threshold = mean * (1.0 - part);

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (origin < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeSingh

/*
 * WAN = base * (1.0 - k * (1.0 - (stdev + delta) / 128.0))
 * base = (mean + max) / 2
 * Uses local mean, deviation, and max to compute adaptive threshold.
 */
BinaryImage binarizeWAN(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeWAN: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqimage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqimage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqimage.push(pixel * pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  // First pass: compute means, deviations, and find local max
  std::vector<float> means(w * h);
  std::vector<float> deviations(w * h);
  std::vector<uint8_t> localMax(w * h);

  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqsum = integralSqimage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqsum * rArea;
      const double variance = sqmean - mean * mean;
      const double deviation = std::sqrt(std::fabs(variance));

      means[y * w + x] = (float) mean;
      deviations[y * w + x] = (float) deviation;
    }
  }

  // Compute local max using O(n) 2D sliding window maximum algorithm.
  // First pass: compute row-wise maximums using monotonic deque.
  std::vector<uint8_t> rowMax(w * h);
  const int windowW = windowSize.width();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    std::deque<int> dq;  // indices of potential maximums
    for (int x = 0; x < w; ++x) {
      // Remove elements outside the window
      while (!dq.empty() && dq.front() <= x - windowW) {
        dq.pop_front();
      }
      // Remove smaller elements (they can never be maximum)
      while (!dq.empty() && grayLine[dq.back()] <= grayLine[x]) {
        dq.pop_back();
      }
      dq.push_back(x);
      // Window is centered at x - windowLeftHalf + windowLeftHalf = x - windowLeftHalf + radius
      // Output for position x - windowLeftHalf when we have enough elements
      const int outX = x - windowLeftHalf;
      if (outX >= 0) {
        rowMax[y * w + outX] = grayLine[dq.front()];
      }
    }
    // Handle remaining positions at end of row
    for (int x = w - windowLeftHalf; x < w; ++x) {
      while (!dq.empty() && dq.front() <= x - windowW) {
        dq.pop_front();
      }
      if (!dq.empty()) {
        rowMax[y * w + x] = grayLine[dq.front()];
      }
    }
    grayLine += grayBpl;
  }

  // Second pass: compute column-wise maximums on the row-max results
  const int windowH = windowSize.height();
  for (int x = 0; x < w; ++x) {
    std::deque<int> dq;
    for (int y = 0; y < h; ++y) {
      while (!dq.empty() && dq.front() <= y - windowH) {
        dq.pop_front();
      }
      while (!dq.empty() && rowMax[dq.back() * w + x] <= rowMax[y * w + x]) {
        dq.pop_back();
      }
      dq.push_back(y);
      const int outY = y - windowLowerHalf;
      if (outY >= 0) {
        localMax[outY * w + x] = rowMax[dq.front() * w + x];
      }
    }
    for (int y = h - windowLowerHalf; y < h; ++y) {
      while (!dq.empty() && dq.front() <= y - windowH) {
        dq.pop_front();
      }
      if (!dq.empty()) {
        localMax[y * w + x] = rowMax[dq.front() * w + x];
      }
    }
  }

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float mean = means[y * w + x];
      const float lmax = localMax[y * w + x];
      const float deviation = deviations[y * w + x];

      // WAN formula: base = (mean + max) / 2
      const float base = (mean + lmax) * 0.5f;
      // threshold = base * (1.0 - k * (1.0 - (stdev + delta) / 128.0))
      const float shift = deviation + delta;
      const float part = k * (1.0f - shift / 128.0f);
      const double threshold = base * (1.0f - part);

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeWAN

/*
 * MultiScale binarization using hierarchical block-based thresholding.
 * Uses multiple resolution levels to adapt to varying illumination.
 */
BinaryImage binarizeMultiScale(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeMultiScale: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  // Compute global min/max for initial threshold
  uint8_t globalMin = 255, globalMax = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      globalMin = std::min(globalMin, grayLine[x]);
      globalMax = std::max(globalMax, grayLine[x]);
    }
    grayLine += grayBpl;
  }
  const float globalThreshold = (globalMax + globalMin) * 0.5f;

  // Create threshold map initialized to global threshold
  std::vector<float> thresholdMap(w * h, globalThreshold);

  // Multi-scale processing: start from large blocks, refine to small
  int radius = windowSize.width() >> 1;
  if (radius < 1) radius = 1;

  // Determine number of levels
  int whcp = (h + w) >> 1;
  int blsz = 1;
  int level = 0;
  while (blsz < whcp) {
    level++;
    blsz <<= 1;
  }
  blsz >>= 1;

  // Adjust level based on radius
  int rsz = 1;
  while (rsz < radius && level > 1) {
    level--;
    rsz <<= 1;
  }

  // Sensitivity controls blending between levels
  const float sensitivity = std::fabs(k);
  const float sensdiv = sensitivity + 1.0f;
  const float senspos = (k >= 0) ? (1.0f / sensdiv) : (sensitivity / sensdiv);
  const float sensinv = (k >= 0) ? (sensitivity / sensdiv) : (1.0f / sensdiv);

  const float kover = 1.5f;

  grayLine = gray.bits();
  for (int l = 0; l < level; ++l) {
    const int cnth = (h + blsz - 1) / blsz;
    const int cntw = (w + blsz - 1) / blsz;
    const int maskbl = blsz;
    const int maskover = (int)(kover * maskbl);

    for (int i = 0; i < cnth; ++i) {
      const int y0 = i * maskbl;
      const int y1 = std::min(y0 + maskover, h);

      for (int j = 0; j < cntw; ++j) {
        const int x0 = j * maskbl;
        const int x1 = std::min(x0 + maskover, w);

        // Find local min/max in this block
        uint8_t blockMin = 255, blockMax = 0;
        for (int y = y0; y < y1; ++y) {
          const uint8_t* rowPtr = gray.bits() + y * grayBpl;
          for (int x = x0; x < x1; ++x) {
            blockMin = std::min(blockMin, rowPtr[x]);
            blockMax = std::max(blockMax, rowPtr[x]);
          }
        }

        // Local threshold from min/max
        const float localT = (blockMax + blockMin) * 0.5f * sensinv;

        // Blend with existing threshold
        for (int y = y0; y < y1; ++y) {
          for (int x = x0; x < x1; ++x) {
            const int idx = y * w + x;
            float t = thresholdMap[idx] * senspos + localT + 0.5f;
            thresholdMap[idx] = std::max(0.0f, std::min(255.0f, t));
          }
        }
      }
    }
    blsz >>= 1;
    if (blsz < 1) break;
  }

  // Apply threshold with delta adjustment
  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double threshold = thresholdMap[y * w + x] + delta;

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeMultiScale

/*
 * Robust binarization using surround-based contrast enhancement.
 * Robust = 255.0 - (surround + 255.0) * sc / (surround + sc)
 * where sc = surround - pixel, surround = local mean
 */
BinaryImage binarizeRobust(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeRobust: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  // Create enhanced image - make a copy of gray and modify it
  QImage enhanced = gray.copy();
  if (enhanced.isNull()) {
    return BinaryImage();
  }
  uint8_t* enhLine = enhanced.bits();
  const int enhBpl = enhanced.bytesPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);

      const double rArea = 1.0 / area;
      const double surround = windowSum * rArea;
      const double origin = grayLine[x];

      // Robust formula: 255 - (surround + 255) * sc / (surround + sc)
      // where sc = surround - origin
      const double sc = surround - origin;
      const double denominator = surround + sc;
      double robust;
      if (std::fabs(denominator) > 1e-6) {
        robust = 255.0 - (surround + 255.0) * sc / denominator;
      } else {
        robust = origin;
      }

      // Blend with original based on k
      double result = k * robust + (1.0 - k) * origin + 0.5;
      result = std::max(0.0, std::min(255.0, result));
      enhLine[x] = static_cast<uint8_t>(result);
    }
    grayLine += grayBpl;
    enhLine += enhBpl;
  }

  // Apply Otsu threshold to enhanced image with delta adjustment
  BinaryThreshold threshold = BinaryThreshold::otsuThreshold(enhanced);
  return BinaryImage(enhanced, BinaryThreshold(threshold + static_cast<int>(delta)));
}  // binarizeRobust

/*
 * Gatos binarization using adaptive degraded document image binarization.
 * Gatos, B., Pratikakis, I., Perantonis, S.J. (2006).
 *
 * Steps:
 * 1. Apply Wiener filter to reduce noise
 * 2. Use Niblack for initial foreground/background segmentation
 * 3. Estimate background by interpolating from background pixels
 * 4. Compute adaptive threshold map from background estimate
 * 5. Apply threshold with bounds checking
 */
BinaryImage binarizeGatos(const QImage& src,
                          const QSize windowSize,
                          const double noiseSigma,
                          const double k,
                          const double delta,
                          const double q,
                          const double p) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeGatos: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  GrayImage gray(src);
  if (gray.isNull()) {
    return BinaryImage();
  }

  const int w = gray.width();
  const int h = gray.height();
  const int radius = windowSize.width() >> 1;

  // Step 1: Apply Wiener filter to reduce noise
  // Wiener filter: output = mean + (variance - noise_variance) / variance * (input - mean)
  GrayImage wiener(gray);
  if (radius > 0 && noiseSigma > 0.0) {
    const float noiseVariance = static_cast<float>(noiseSigma * noiseSigma);

    // Compute local mean using gaussian blur
    GrayImage gmean = gaussBlur(gray, static_cast<float>(radius), static_cast<float>(radius));
    if (gmean.isNull()) {
      return BinaryImage();
    }

    // Compute local variance using integral images
    IntegralImage<uint32_t> integralImage(w, h);
    IntegralImage<uint64_t> integralSqImage(w, h);

    const uint8_t* grayLine = gray.data();
    const int grayStride = gray.stride();

    for (int y = 0; y < h; ++y) {
      integralImage.beginRow();
      integralSqImage.beginRow();
      for (int x = 0; x < w; ++x) {
        const uint32_t pixel = grayLine[x];
        integralImage.push(pixel);
        integralSqImage.push(pixel * pixel);
      }
      grayLine += grayStride;
    }

    uint8_t* wienerLine = wiener.data();
    const int wienerStride = wiener.stride();
    const uint8_t* gmeanLine = gmean.data();
    const int gmeanStride = gmean.stride();
    grayLine = gray.data();

    for (int y = 0; y < h; ++y) {
      const int top = std::max(0, y - radius);
      const int bottom = std::min(h, y + radius + 1);
      for (int x = 0; x < w; ++x) {
        const int left = std::max(0, x - radius);
        const int right = std::min(w, x + radius + 1);
        const int area = (bottom - top) * (right - left);
        const QRect rect(left, top, right - left, bottom - top);

        const double sum = integralImage.sum(rect);
        const double sqsum = integralSqImage.sum(rect);
        const double rArea = 1.0 / area;
        const double mean = sum * rArea;
        const double sqmean = sqsum * rArea;
        const double variance = std::max(0.0, sqmean - mean * mean);

        const float srcPixel = static_cast<float>(grayLine[x]);
        const float localMean = static_cast<float>(gmeanLine[x]);
        const float deltaPixel = srcPixel - localMean;
        const float deltaVariance = static_cast<float>(variance) - noiseVariance;

        float dstPixel = localMean;
        if (deltaVariance > 0.0f && variance > 1e-6) {
          dstPixel += deltaPixel * deltaVariance / static_cast<float>(variance);
        }
        int val = static_cast<int>(dstPixel + 0.5f);
        val = std::max(0, std::min(255, val));
        wienerLine[x] = static_cast<uint8_t>(val);
      }
      grayLine += grayStride;
      wienerLine += wienerStride;
      gmeanLine += gmeanStride;
    }
  }

  // Step 2: Apply Niblack for initial segmentation
  BinaryImage niblack = binarizeNiblack(wiener.toQImage(), windowSize, k, 0.0);
  if (niblack.isNull()) {
    return BinaryImage();
  }

  // Step 3: Estimate background by interpolating from background (white) pixels
  // For foreground (black) pixels, find nearest background pixels and interpolate
  GrayImage background(wiener);

  // Build integral images for background pixel interpolation
  IntegralImage<uint32_t> niblackBgII(w, h);
  IntegralImage<uint32_t> grayBgII(w, h);

  const uint32_t* niblackLine = niblack.data();
  const int niblackWpl = niblack.wordsPerLine();
  const uint8_t* wienerLine = wiener.data();
  const int wienerStride = wiener.stride();

  for (int y = 0; y < h; ++y) {
    niblackBgII.beginRow();
    grayBgII.beginRow();
    for (int x = 0; x < w; ++x) {
      // Check if pixel is background (white = 0 in binary image convention)
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      const bool isForeground = (niblackLine[x >> 5] & mask) != 0;

      // bg: 1 if background, 0 if foreground
      const uint32_t isBg = isForeground ? 0 : 1;
      niblackBgII.push(isBg);
      // bg: gray_pixel if background, 0 if foreground
      grayBgII.push(isBg ? wienerLine[x] : 0);
    }
    wienerLine += wienerStride;
    niblackLine += niblackWpl;
  }

  // Interpolate background for foreground pixels
  uint8_t* bgLine = background.data();
  const int bgStride = background.stride();
  niblackLine = niblack.data();
  const int ws = (radius << 1) + 1;
  const QRect imageRect(0, 0, w, h);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      const bool isForeground = (niblackLine[x >> 5] & mask) != 0;

      if (isForeground) {
        // Expand window until we find background pixels
        uint32_t niblackSumBg = 0;
        int wss = 0;
        QRect window;
        while (niblackSumBg == 0 && (wss < w * 2 || wss < h * 2)) {
          wss += ws;
          window = QRect(x - wss / 2, y - wss / 2, wss, wss);
          window = window.intersected(imageRect);
          if (!window.isEmpty()) {
            niblackSumBg = niblackBgII.sum(window);
          }
        }

        // Interpolate from background pixels
        if (niblackSumBg > 0 && !window.isEmpty()) {
          const uint32_t graySumBg = grayBgII.sum(window);
          bgLine[x] = static_cast<uint8_t>((graySumBg + (niblackSumBg >> 1)) / niblackSumBg);
        }
      }
    }
    bgLine += bgStride;
    niblackLine += niblackWpl;
  }

  // Step 4: Compute threshold map from background estimate
  // Calculate statistics for threshold computation
  wienerLine = wiener.data();
  bgLine = background.data();
  uint64_t sumDiff = 0;
  uint64_t sumBg = 0;
  uint64_t sumContour = 0;
  const uint64_t srcSize = static_cast<uint64_t>(w) * h;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int im = wienerLine[x];
      const int bg = bgLine[x];
      if (im < bg) {
        sumDiff += (bg - im);
        sumContour++;
      } else {
        sumBg += bg;
      }
    }
    wienerLine += wienerStride;
    bgLine += bgStride;
  }

  if (sumContour == 0 || sumContour == srcSize) {
    // All pixels are same type, use Otsu fallback
    return BinaryImage(wiener.toQImage(), BinaryThreshold::otsuThreshold(wiener.toQImage()));
  }

  const float d = static_cast<float>(sumDiff) / sumContour;
  const float b = static_cast<float>(sumBg) / (srcSize - sumContour);

  // Adjust q based on delta
  const float qd = static_cast<float>(q) - (1.0f - static_cast<float>(q)) * static_cast<float>(delta) * 0.02f;
  const float qdVal = qd * d;
  const float thresholdScale = qdVal * static_cast<float>(p);
  const float thresholdBias = qdVal * (1.0f - static_cast<float>(p));

  // Build lookup table for threshold adjustment
  uint8_t histogram[256];
  for (int k = 0; k < 256; ++k) {
    const float bgf = (static_cast<float>(k) + 1.0f) / (b + 1.0f);
    const float tk = 1.0f / (1.0f + std::exp(-8.0f * bgf + 6.0f));
    int threshold = static_cast<int>(thresholdScale * tk + thresholdBias + 0.5f);
    int bgn = k - threshold;
    bgn = std::max(0, std::min(255, bgn));
    histogram[k] = static_cast<uint8_t>(bgn);
  }

  // Apply lookup table to background to get threshold map
  bgLine = background.data();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      bgLine[x] = histogram[bgLine[x]];
    }
    bgLine += bgStride;
  }

  // Step 5: Apply threshold map to produce final binary image
  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  wienerLine = wiener.data();
  bgLine = background.data();

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      // Black if pixel < threshold
      if (wienerLine[x] < bgLine[x]) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    wienerLine += wienerStride;
    bgLine += bgStride;
    bwLine += bwWpl;
  }

  return bwImg;
}  // binarizeGatos

/*
 * Window binarization using local mean, deviation, and global statistics.
 * Uses formula: threshold = mean * (1 - k * md / kd)
 * where md = (mean + 1 - delta) / (meanFull + deviation + 1)
 *       kd = 1 + kdm * kds
 *       kdm = (2 * meanFull + 1) / (deviation + 1)
 *       kds = (deviation - deviationMin) / deviationDelta
 */
BinaryImage binarizeWindow(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeWindow: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);
  IntegralImage<uint64_t> integralSqImage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    integralSqImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
      integralSqImage.push(pixel * pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  // Pre-compute means and deviations for global statistics
  std::vector<float> means(w * h);
  std::vector<float> deviations(w * h);

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      assert(area > 0);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double windowSqSum = integralSqImage.sum(rect);

      const double rArea = 1.0 / area;
      const double mean = windowSum * rArea;
      const double sqmean = windowSqSum * rArea;
      const double variance = std::max(0.0, sqmean - mean * mean);
      const double deviation = std::sqrt(variance);

      const int idx = y * w + x;
      means[idx] = static_cast<float>(mean);
      deviations[idx] = static_cast<float>(deviation);
    }
    grayLine += grayBpl;
  }

  // Compute global mean and deviation min/max
  double meanFull = 0.0;
  float deviationMin = 256.0f, deviationMax = 0.0f;
  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      meanFull += grayLine[x];
      const float dev = deviations[y * w + x];
      deviationMin = std::min(deviationMin, dev);
      deviationMax = std::max(deviationMax, dev);
    }
    grayLine += grayBpl;
  }
  meanFull /= (static_cast<double>(w) * h);
  const float deviationDelta = deviationMax - deviationMin;

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int idx = y * w + x;
      const float mean = means[idx];
      const float deviation = deviations[idx];

      // Window formula
      const float md = (mean + 1.0f - static_cast<float>(delta)) / (static_cast<float>(meanFull) + deviation + 1.0f);
      const float kdm = (static_cast<float>(meanFull * 2.0) + 1.0f) / (deviation + 1.0f);
      const float kds = (deviationDelta > 0.0f) ? ((deviation - deviationMin) / deviationDelta) : 1.0f;
      const float kd = 1.0f + kdm * kds;
      const double threshold = mean * (1.0 - k * md / kd);

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeWindow

/*
 * Fox binarization using gradient-based adaptive thresholding.
 * Uses normalized gradient relative to local mean:
 * threshold = base * (1.0 - k * part) + grayMin
 * where base = mean - grayMin, part = 0.5 * (1.0 - (frac + fracDelta))
 *       frac = normalized gradient (origin - mean) / (256 - grad) relative to max
 */
BinaryImage binarizeFox(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeFox: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  const QImage gray(toGrayscale(src));
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  IntegralImage<uint32_t> integralImage(w, h);

  const uint8_t* grayLine = gray.bits();
  const int grayBpl = gray.bytesPerLine();

  for (int y = 0; y < h; ++y) {
    integralImage.beginRow();
    for (int x = 0; x < w; ++x) {
      const uint32_t pixel = grayLine[x];
      integralImage.push(pixel);
    }
    grayLine += grayBpl;
  }

  const int windowLowerHalf = windowSize.height() >> 1;
  const int windowUpperHalf = windowSize.height() - windowLowerHalf;
  const int windowLeftHalf = windowSize.width() >> 1;
  const int windowRightHalf = windowSize.width() - windowLeftHalf;

  // Pre-compute means
  std::vector<float> means(w * h);

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    const int top = std::max(0, y - windowLowerHalf);
    const int bottom = std::min(h, y + windowUpperHalf);
    for (int x = 0; x < w; ++x) {
      const int left = std::max(0, x - windowLeftHalf);
      const int right = std::min(w, x + windowRightHalf);
      const int area = (bottom - top) * (right - left);
      const QRect rect(left, top, right - left, bottom - top);
      const double windowSum = integralImage.sum(rect);
      const double mean = windowSum / area;
      means[y * w + x] = static_cast<float>(mean);
    }
    grayLine += grayBpl;
  }

  // First pass: compute global min gray and max frac
  float grayMin = 255.0f;
  float fracMax = 0.0f;

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float origin = grayLine[x];
      const float mean = means[y * w + x];
      const float grad = origin - mean;
      const float frac = grad / (256.0f - grad);
      const float fracAbs = std::fabs(frac);
      fracMax = std::max(fracMax, fracAbs);
      grayMin = std::min(grayMin, origin);
    }
    grayLine += grayBpl;
  }

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = gray.bits();
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float origin = grayLine[x];
      const float mean = means[y * w + x];

      const float base = mean - grayMin;
      const float grad = origin - mean;
      const float frac = (fracMax > 0.0f) ? (grad / (256.0f - grad) / fracMax) : 1.0f;
      const float fracDelta = static_cast<float>(delta) / 128.0f;
      const float part = 0.5f * (1.0f - (frac + fracDelta));
      double threshold = base * (1.0 - k * part) + grayMin;

      threshold = std::max(0.0, std::min(255.0, threshold));

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (int(grayLine[x]) < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayBpl;
    bwLine += bwWpl;
  }
  return bwImg;
}  // binarizeFox

/*
 * Engraving binarization using Gaussian blur and overlay-style blending.
 * Good for engraved or printed documents with fine patterns.
 * Uses mean absolute difference for threshold scaling.
 */
BinaryImage binarizeEngraving(const QImage& src, const QSize windowSize, const double coef, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeEngraving: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  GrayImage gray(src);
  if (gray.isNull()) {
    return BinaryImage();
  }
  const int w = gray.width();
  const int h = gray.height();

  const int radius = windowSize.width() >> 1;

  // Apply Gaussian blur to get local mean
  GrayImage gmean = gaussBlur(gray, static_cast<float>(radius), static_cast<float>(radius));
  if (gmean.isNull()) {
    return BinaryImage();
  }

  if (radius > 0 && coef != 0.0) {
    const uint8_t* grayLine = gray.data();
    const int grayStride = gray.stride();
    uint8_t* gmeanLine = gmean.data();
    const int gmeanStride = gmean.stride();

    // Compute mean absolute difference
    double meanDelta = 0.0;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const float origin = grayLine[x];
        const float mean = gmeanLine[x];
        const float diff = std::fabs(origin - mean);
        meanDelta += diff;
      }
      grayLine += grayStride;
      gmeanLine += gmeanStride;
    }
    meanDelta /= (static_cast<double>(w) * h);

    if (meanDelta > 0.0) {
      gmeanLine = gmean.data();
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          const float mean = gmeanLine[x];

          const float tline = mean / static_cast<float>(meanDelta);
          int threshInt = static_cast<int>(tline);
          float d = tline - threshInt;
          d = (d < 0.5f) ? (0.5f - d) : (d - 0.5f);
          d *= d;
          d += d;
          d += d;  // d = 4 * d^2

          // Overlay blend
          float retval = mean;
          if (mean > 127.5f) {
            retval = 255.0f - retval;
            d = 1.0f - d;
          }
          retval *= d;
          retval += retval;
          if (mean > 127.5f) {
            retval = 255.0f - retval;
          }
          retval = static_cast<float>(coef) * retval + (1.0f - static_cast<float>(coef)) * mean + 0.5f;
          retval = std::max(0.0f, std::min(255.0f, retval));
          gmeanLine[x] = static_cast<uint8_t>(retval);
        }
        gmeanLine += gmeanStride;
      }
    }
  }

  // Apply threshold with delta adjustment
  BinaryThreshold threshold = BinaryThreshold::otsuThreshold(gmean.toQImage());
  return BinaryImage(gmean.toQImage(), BinaryThreshold(threshold + static_cast<int>(delta)));
}  // binarizeEngraving

/*
 * Helper: Compute bimodal threshold using iterative k-means-like clustering.
 * Finds the threshold that best separates the histogram into two groups.
 */
static unsigned int computeBiModalThreshold(const uint8_t* data, int stride, int w, int h, int delta) {
  const unsigned int histsize = 256;
  uint64_t histogram[histsize] = {0};

  // Build histogram
  const uint8_t* line = data;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      histogram[line[x]]++;
    }
    line += stride;
  }

  // Find min/max non-zero histogram entries
  int Tmin = 255, Tmax = 0;
  for (int i = 0; i < 256; ++i) {
    if (histogram[i] > 0) {
      if (i < Tmin) Tmin = i;
      if (i > Tmax) Tmax = i;
    }
  }
  Tmax++;

  // Scale for precision
  Tmin <<= 7;
  Tmax <<= 7;

  // Compute initial threshold based on delta
  int part = 256 + delta * 2;
  part = std::max(0, std::min(512, part));

  unsigned int threshold = (part * Tmax + (512 - part) * Tmin) >> 8;
  unsigned int Tn = threshold + 1;

  // Iterative refinement
  while (threshold != Tn) {
    Tn = threshold;
    uint64_t Tb = 0, Tw = 0;
    uint64_t ib = 0, iw = 0;

    for (unsigned int k = 0; k < histsize; ++k) {
      uint64_t im = histogram[k];
      if ((k << 8) < threshold) {
        Tb += im * k;
        ib += im;
      } else {
        Tw += im * k;
        iw += im;
      }
    }

    Tb = (ib > 0) ? ((Tb << 7) / ib) : Tmin;
    Tw = (iw > 0) ? ((Tw << 7) / iw) : Tmax;
    threshold = (part * Tw + (512 - part) * Tb + 127) >> 8;
  }

  threshold = (threshold + 127) >> 8;
  return threshold;
}

/*
 * Helper: Find dominant gray value using histogram analysis with binary search.
 */
static unsigned int computeDominantGray(const uint8_t* data, int stride, int w, int h) {
  uint64_t histogram[256] = {0};

  // Build histogram
  const uint8_t* line = data;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      histogram[line[x]]++;
    }
    line += stride;
  }

  // Compute cumulative histogram
  for (int i = 1; i < 256; ++i) {
    histogram[i] += histogram[i - 1];
  }

  // Binary search for dominant value (most populated range)
  unsigned int ilow = 0;
  for (unsigned int i = 128; i > 0; i >>= 1) {
    uint64_t sumMax = 0;
    unsigned int inew = ilow;
    for (unsigned int j = ilow; j < ilow + i && j + i < 256; ++j) {
      uint64_t sumLow = (j > 0) ? histogram[j] : 0;
      uint64_t sumHigh = histogram[j + i];
      uint64_t sum = sumHigh - sumLow;
      if (sum > sumMax) {
        inew = j;
        sumMax = sum;
      }
    }
    ilow = inew;
  }

  return ilow;
}

/*
 * BiModal binarization using histogram clustering.
 * Finds optimal threshold by iteratively separating histogram into two groups.
 */
BinaryImage binarizeBiModal(const QImage& src, const double delta) {
  if (src.isNull()) {
    return BinaryImage();
  }

  GrayImage gray(src);
  if (gray.isNull()) {
    return BinaryImage();
  }

  const int w = gray.width();
  const int h = gray.height();
  const uint8_t* grayData = gray.data();
  const int grayStride = gray.stride();

  unsigned int threshold = computeBiModalThreshold(grayData, grayStride, w, h, static_cast<int>(delta));

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  const uint8_t* grayLine = grayData;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (grayLine[x] < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayStride;
    bwLine += bwWpl;
  }

  return bwImg;
}  // binarizeBiModal

/*
 * Mean binarization using distance from dominant gray value.
 * Computes standard deviation of distances and uses it as threshold.
 */
BinaryImage binarizeMean(const QImage& src, const double delta) {
  if (src.isNull()) {
    return BinaryImage();
  }

  GrayImage gray(src);
  if (gray.isNull()) {
    return BinaryImage();
  }

  const int w = gray.width();
  const int h = gray.height();
  const uint8_t* grayData = gray.data();
  const int grayStride = gray.stride();

  // Find dominant gray value
  double dominant = computeDominantGray(grayData, grayStride, w, h);

  // Compute mean squared distance from dominant value
  double distMean = 0.0;
  uint64_t count = 0;
  const uint8_t* grayLine = grayData;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      double pixel = grayLine[x];
      double dist = std::fabs(pixel - dominant);
      distMean += dist * dist;
      count++;
    }
    grayLine += grayStride;
  }
  distMean = (count > 0) ? (distMean / count) : 64.0 * 64.0;
  double threshold = std::sqrt(distMean);

  // Count pixels within threshold to determine if we need to invert
  uint64_t countWithin = 0;
  grayLine = grayData;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      double pixel = grayLine[x];
      double dist = std::fabs(pixel - dominant);
      if (dist < threshold) {
        countWithin++;
      }
    }
    grayLine += grayStride;
  }
  countWithin *= 2;

  // Adjust threshold based on delta and whether we invert
  bool invert = (count < countWithin);
  threshold *= invert ? (1.0 - delta * 0.02) : (1.0 + delta * 0.02);

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  grayLine = grayData;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      double pixel = grayLine[x];
      double dist = std::fabs(pixel - dominant);
      bool isBlack = (dist < threshold) != invert;

      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (isBlack) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayStride;
    bwLine += bwWpl;
  }

  return bwImg;
}  // binarizeMean

/*
 * Grain binarization using local contrast enhancement and BiModal.
 * Pre-processes image to reduce grain/noise before thresholding.
 */
BinaryImage binarizeGrain(const QImage& src, const QSize windowSize, const double k, const double delta) {
  if (windowSize.isEmpty()) {
    throw std::invalid_argument("binarizeGrain: invalid windowSize");
  }

  if (src.isNull()) {
    return BinaryImage();
  }

  GrayImage gray(src);
  if (gray.isNull()) {
    return BinaryImage();
  }

  const int w = gray.width();
  const int h = gray.height();
  const int radius = windowSize.width() >> 1;

  if (radius > 0 && k != 0.0) {
    // Step 1: Compute local mean using Gaussian blur
    GrayImage gmean = gaussBlur(gray, static_cast<float>(radius), static_cast<float>(radius));
    if (gmean.isNull()) {
      return binarizeBiModal(src, delta);
    }

    // Step 2: Compute local contrast (origin - mean + 127)
    uint8_t* grayLine = gray.data();
    const int grayStride = gray.stride();
    uint8_t* gmeanLine = gmean.data();
    const int gmeanStride = gmean.stride();

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        int origin = grayLine[x];
        int mean = gmeanLine[x];
        int retval = origin - mean + 127;
        retval = std::max(0, std::min(255, retval));
        gmeanLine[x] = static_cast<uint8_t>(retval);
      }
      grayLine += grayStride;
      gmeanLine += gmeanStride;
    }

    // Step 3: Apply second blur to the contrast image
    GrayImage gsigma = gaussBlur(gmean, static_cast<float>(radius), static_cast<float>(radius));
    if (!gsigma.isNull()) {
      gmeanLine = gmean.data();
      uint8_t* gsigmaLine = gsigma.data();
      const int gsigmaStride = gsigma.stride();

      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          int mean = gmeanLine[x];
          int sigma = gsigmaLine[x];
          int retval = mean + mean - sigma;
          retval = std::max(0, std::min(255, retval));
          gmeanLine[x] = static_cast<uint8_t>(retval);
        }
        gmeanLine += gmeanStride;
        gsigmaLine += gsigmaStride;
      }
    }

    // Step 4: Blend processed result back with original
    grayLine = gray.data();
    gmeanLine = gmean.data();
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float origin = grayLine[x];
        float processed = gmeanLine[x];
        float retval = static_cast<float>(k) * processed + (1.0f - static_cast<float>(k)) * origin + 0.5f;
        retval = std::max(0.0f, std::min(255.0f, retval));
        grayLine[x] = static_cast<uint8_t>(retval);
      }
      grayLine += grayStride;
      gmeanLine += gmeanStride;
    }
  }

  // Apply BiModal threshold to processed image
  const uint8_t* grayData = gray.data();
  const int grayStride = gray.stride();

  unsigned int threshold = computeBiModalThreshold(grayData, grayStride, w, h, static_cast<int>(delta));

  BinaryImage bwImg(w, h);
  uint32_t* bwLine = bwImg.data();
  const int bwWpl = bwImg.wordsPerLine();

  const uint8_t* grayLine = grayData;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint32_t msb = uint32_t(1) << 31;
      const uint32_t mask = msb >> (x & 31);
      if (grayLine[x] < threshold) {
        bwLine[x >> 5] |= mask;
      } else {
        bwLine[x >> 5] &= ~mask;
      }
    }
    grayLine += grayStride;
    bwLine += bwWpl;
  }

  return bwImg;
}  // binarizeGrain

BinaryImage peakThreshold(const QImage& image) {
  return BinaryImage(image, BinaryThreshold::peakThreshold(image));
}
}  // namespace imageproc
