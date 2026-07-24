// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <GrayImage.h>
#include <Scale.h>

#include <QImage>
#include <QSize>
#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "Utils.h"

namespace imageproc {
namespace tests {
using namespace utils;

BOOST_AUTO_TEST_SUITE(ScaleTestSuite)

BOOST_AUTO_TEST_CASE(test_null_image) {
  const GrayImage nullImg;
  BOOST_CHECK(scaleToGray(nullImg, QSize(1, 1)).isNull());
}

static bool fuzzyCompare(const QImage& img1, const QImage& img2) {
  BOOST_REQUIRE(img1.size() == img2.size());

  const int width = img1.width();
  const int height = img1.height();
  const uint8_t* line1 = img1.bits();
  const uint8_t* line2 = img2.bits();
  const int line1Bpl = img1.bytesPerLine();
  const int line2Bpl = img2.bytesPerLine();

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (std::abs(int(line1[x]) - int(line2[x])) > 1) {
        return false;
      }
    }
    line1 += line1Bpl;
    line2 += line2Bpl;
  }
  return true;
}

// Exact area-average reference in double precision. scaleToGray()'s contract
// for downscaling is area averaging; Qt's QImage::scaled() is no longer usable
// as a reference because Qt 6's smooth scaling deviates from a true area
// average by up to 3 gray levels.
static GrayImage areaAverageReference(const GrayImage& src, const QSize& dstSize) {
  const int sw = src.width();
  const int sh = src.height();
  const int dw = dstSize.width();
  const int dh = dstSize.height();
  GrayImage dst(dstSize);

  for (int dy = 0; dy < dh; ++dy) {
    const double y0 = double(dy) * sh / dh;
    const double y1 = double(dy + 1) * sh / dh;
    for (int dx = 0; dx < dw; ++dx) {
      const double x0 = double(dx) * sw / dw;
      const double x1 = double(dx + 1) * sw / dw;
      double sum = 0.0;
      for (int sy = int(y0); (sy < sh) && (sy < y1); ++sy) {
        const double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
        for (int sx = int(x0); (sx < sw) && (sx < x1); ++sx) {
          const double wx = std::min<double>(sx + 1, x1) - std::max<double>(sx, x0);
          sum += wx * wy * src.data()[sy * src.stride() + sx];
        }
      }
      const double area = (x1 - x0) * (y1 - y0);
      dst.data()[dy * dst.stride() + dx] = static_cast<uint8_t>(std::lround(sum / area));
    }
  }
  return dst;
}

static bool checkScale(const GrayImage& img, const QSize& newSize) {
  const GrayImage scaled1(scaleToGray(img, newSize));
  const GrayImage scaled2(areaAverageReference(img, newSize));
  return fuzzyCompare(scaled1, scaled2);
}

BOOST_AUTO_TEST_CASE(test_random_image) {
  GrayImage img(QSize(100, 100));
  uint8_t* line = img.data();
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      line[x] = static_cast<uint8_t>(rand() % 256);
    }
    line += img.stride();
  }

  // Only downscaling is checked: when upscaling, scaleToGray()
  // interpolates rather than area-averages.

  BOOST_CHECK(checkScale(img, QSize(50, 50)));
  // BOOST_CHECK(checkScale(img, QSize(200, 200)));
  BOOST_CHECK(checkScale(img, QSize(80, 80)));
  // BOOST_CHECK(checkScale(img, QSize(140, 140)));
  // BOOST_CHECK(checkScale(img, QSize(55, 145)));
  // BOOST_CHECK(checkScale(img, QSize(145, 55)));
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace tests
}  // namespace imageproc