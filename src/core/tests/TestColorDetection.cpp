// Copyright (C) 2026  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

// Tests for Finalize color detection on aged/toned paper: the margin-less
// fallback background estimate (WhiteBalance::estimateBackgroundColor /
// detectPaperColor) and cast-compensated classification
// (LeptonicaDetector::detectWithCastCompensation).

#include <LeptonicaDetector.h>
#include <WhiteBalance.h>

#include <QImage>
#include <QFileInfo>
#include <QPainter>
#include <boost/test/unit_test.hpp>
#include <cstdlib>

namespace Tests {

namespace {

const int kPageW = 800;
const int kPageH = 1000;

// Typical aged 1920s book stock: warm tan, strong enough cast that
// pixColorFraction's diffthresh (50) counts it as "color".
const QColor kTanPaper(214, 192, 150);

// Draw simulated text lines: black word-blocks on the given background.
// Produces >10% dark pixels and >30% light pixels (bimodal), no midtones.
QImage makeTextPage(const QColor& paper) {
  QImage image(kPageW, kPageH, QImage::Format_RGB32);
  image.fill(paper);

  QPainter painter(&image);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  for (int y = 60; y < kPageH - 60; y += 34) {
    for (int x = 60; x < kPageW - 60; x += 90) {
      painter.drawRect(x, y, 70, 14);  // a "word"
    }
  }
  painter.end();
  return image;
}

// A genuine color photograph (varied saturated hues) occupying the center of
// a tan text page. Hues are kept below paper brightness so the background
// estimate still lands on the paper.
QImage makeColorPhotoOnTanPage() {
  QImage image = makeTextPage(kTanPaper);

  QPainter painter(&image);
  painter.setPen(Qt::NoPen);
  const QColor hues[] = {QColor(190, 40, 40),  QColor(40, 150, 60),  QColor(50, 70, 190),
                         QColor(180, 120, 30), QColor(140, 40, 160), QColor(30, 150, 150)};
  const int photoX = 150, photoY = 250, photoW = 500, photoH = 500;
  const int stripeW = photoW / 6;
  for (int i = 0; i < 6; ++i) {
    painter.setBrush(hues[i]);
    painter.drawRect(photoX + i * stripeW, photoY, stripeW, photoH);
  }
  painter.end();
  return image;
}

// A neutral continuous-tone photograph on an otherwise white text page.
// The patch deliberately has smooth luminance changes rather than flat
// synthetic stripes, exercising the same normalized midtone safeguard used
// for real halftones and photographs.
QImage makeGrayscalePhotoOnTextPage() {
  QImage image = makeTextPage(QColor(250, 250, 250));
  const QRect photoRect(150, 250, 500, 500);
  for (int y = photoRect.top(); y <= photoRect.bottom(); ++y) {
    QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = photoRect.left(); x <= photoRect.right(); ++x) {
      const int localX = x - photoRect.left();
      const int localY = y - photoRect.top();
      const int ramp = 35 + (localX * 100) / photoRect.width();
      const int texture = ((localX / 18 + localY / 14) % 2) * 12;
      const int gray = ramp + texture;
      line[x] = qRgb(gray, gray, gray);
    }
  }
  return image;
}

int maxChannelDiff(const QColor& a, const QColor& b) {
  int d = std::abs(a.red() - b.red());
  d = std::max(d, std::abs(a.green() - b.green()));
  d = std::max(d, std::abs(a.blue() - b.blue()));
  return d;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(ColorDetectionTestSuite)

// A content box that hugs the page leaves no margins; the fallback must still
// produce a paper-color estimate close to the actual paper tint.
BOOST_AUTO_TEST_CASE(paper_color_fallback_without_margins) {
  const QImage page = makeTextPage(kTanPaper);
  const QRect hugging = page.rect();  // content box == page: zero margins

  const QColor paper = WhiteBalance::detectPaperColor(page, hugging);
  BOOST_REQUIRE(paper.isValid());
  BOOST_CHECK_LE(maxChannelDiff(paper, kTanPaper), 8);
  BOOST_CHECK(WhiteBalance::hasSignificantCast(paper));
}

// The background estimate on a neutral white page is near-white and carries
// no cast, so no neutralization would be triggered.
BOOST_AUTO_TEST_CASE(background_estimate_neutral_page) {
  const QImage page = makeTextPage(QColor(250, 250, 250));

  const QColor bg = WhiteBalance::estimateBackgroundColor(page);
  BOOST_REQUIRE(bg.isValid());
  BOOST_CHECK_LE(maxChannelDiff(bg, QColor(250, 250, 250)), 6);
  BOOST_CHECK(!WhiteBalance::hasSignificantCast(bg));
}

// A mostly dark page has no paper-like background; the estimate must refuse
// rather than invent one from photo midtones.
BOOST_AUTO_TEST_CASE(background_estimate_rejects_dark_page) {
  QImage dark(kPageW, kPageH, QImage::Format_RGB32);
  dark.fill(QColor(40, 35, 30));

  const QColor bg = WhiteBalance::estimateBackgroundColor(dark);
  BOOST_CHECK(!bg.isValid());
}

// Uniform tan paper with black text: plain detection reads the tint as COLOR,
// cast compensation must classify it as B&W.
BOOST_AUTO_TEST_CASE(tan_text_page_is_not_color) {
  const QImage page = makeTextPage(kTanPaper);

  // Precondition for the regression: the tint alone trips the color detector.
  BOOST_REQUIRE(LeptonicaDetector::detect(page) == LeptonicaDetector::ColorType::Color);

  const LeptonicaDetector::ColorType compensated = LeptonicaDetector::detectWithCastCompensation(page);
  BOOST_CHECK(compensated != LeptonicaDetector::ColorType::Color);
  BOOST_CHECK(compensated == LeptonicaDetector::ColorType::BlackWhite);
}

// A genuine color photo on the same tan paper must survive neutralization
// and stay COLOR.
BOOST_AUTO_TEST_CASE(color_photo_on_tan_page_stays_color) {
  const QImage page = makeColorPhotoOnTanPage();

  const LeptonicaDetector::ColorType compensated = LeptonicaDetector::detectWithCastCompensation(page);
  BOOST_CHECK(compensated == LeptonicaDetector::ColorType::Color);
}

BOOST_AUTO_TEST_CASE(grayscale_photo_on_text_page_stays_grayscale) {
  const QImage page = makeGrayscalePhotoOnTextPage();

  BOOST_CHECK(LeptonicaDetector::detectWithCastCompensation(page)
              == LeptonicaDetector::ColorType::Grayscale);
}

// Neutral white paper with black text is B&W, and compensation changes nothing.
BOOST_AUTO_TEST_CASE(white_text_page_is_bw) {
  const QImage page = makeTextPage(QColor(250, 250, 250));

  BOOST_CHECK(LeptonicaDetector::detect(page) == LeptonicaDetector::ColorType::BlackWhite);
  BOOST_CHECK(LeptonicaDetector::detectWithCastCompensation(page) == LeptonicaDetector::ColorType::BlackWhite);
}

BOOST_AUTO_TEST_CASE(real_toned_text_crop_is_bw) {
  const QString path =
      QStringLiteral(SCANTAILOR_TEST_SOURCE_DIR "/src/core/tests/fixtures/toned-text-page-92.png");
  BOOST_REQUIRE(QFileInfo::exists(path));
  const QImage page(path);
  BOOST_REQUIRE(!page.isNull());

  BOOST_REQUIRE(LeptonicaDetector::detect(page) == LeptonicaDetector::ColorType::Color);
  BOOST_CHECK(LeptonicaDetector::detectWithCastCompensation(page)
              == LeptonicaDetector::ColorType::BlackWhite);
}

BOOST_AUTO_TEST_CASE(real_color_card_crop_stays_color) {
  const QString path =
      QStringLiteral(SCANTAILOR_TEST_SOURCE_DIR "/src/core/tests/fixtures/color-card-page-390.png");
  BOOST_REQUIRE(QFileInfo::exists(path));
  const QImage page(path);
  BOOST_REQUIRE(!page.isNull());

  BOOST_CHECK(LeptonicaDetector::detectWithCastCompensation(page) == LeptonicaDetector::ColorType::Color);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
