// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <Binarize.h>
#include <BinaryImage.h>

#include <QImage>
#include <QSize>
#include <boost/test/unit_test.hpp>
#include <cstring>

#include "Utils.h"

namespace imageproc {
namespace tests {
using namespace utils;

BOOST_AUTO_TEST_SUITE(BinarizeTestSuite)
BOOST_AUTO_TEST_CASE(sauvolaSerialAndParallelAreBitIdentical) {
  QImage image(257, 193, QImage::Format_Grayscale8);
  for (int y = 0; y < image.height(); ++y) {
    uchar* line = image.scanLine(y);
    for (int x = 0; x < image.width(); ++x) {
      line[x] = static_cast<uchar>((x * 37 + y * 61 + ((x * y) >> 3)) & 0xff);
    }
  }

  const QByteArray previousOverride = qgetenv("SCANTAILOR_SAUVOLA_PARALLEL");
  const bool hadOverride = qEnvironmentVariableIsSet("SCANTAILOR_SAUVOLA_PARALLEL");

  qputenv("SCANTAILOR_SAUVOLA_PARALLEL", "0");
  const BinaryImage serial = binarizeSauvola(image, QSize(31, 41), 0.34, 2.0);
  qputenv("SCANTAILOR_SAUVOLA_PARALLEL", "1");
  const BinaryImage parallel = binarizeSauvola(image, QSize(31, 41), 0.34, 2.0);

  if (hadOverride) {
    qputenv("SCANTAILOR_SAUVOLA_PARALLEL", previousOverride);
  } else {
    qunsetenv("SCANTAILOR_SAUVOLA_PARALLEL");
  }

  BOOST_REQUIRE(serial.size() == parallel.size());
  const size_t bytes = static_cast<size_t>(serial.wordsPerLine()) * serial.height() * sizeof(uint32_t);
  BOOST_CHECK_EQUAL(std::memcmp(serial.data(), parallel.data(), bytes), 0);
}

#if 0
            BOOST_AUTO_TEST_CASE(test) {
                QImage img("test.png");
                binarizeWolf(img).toQImage().save("out.png");
            }
#endif
BOOST_AUTO_TEST_SUITE_END()
}  // namespace tests
}  // namespace imageproc
