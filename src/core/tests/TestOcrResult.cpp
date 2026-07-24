// Copyright (C) 2026 ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <boost/test/unit_test.hpp>

#include <QDomDocument>

#include "filters/ocr/OcrResult.h"

BOOST_AUTO_TEST_SUITE(OcrResultTestSuite)

BOOST_AUTO_TEST_CASE(persistsBlankPageDependencyFingerprint) {
  ocr::OcrResult original;
  original.setImageDimensions(1725, 2650);
  original.setLanguage(QStringLiteral("en-US"));
  original.setAccurateRecognition(true);
  original.setLanguageCorrection(false);
  original.setOutputFileIdentity(123456, 987654321);
  original.setRecognitionSchema(1);

  QDomDocument document;
  const QDomElement element = original.toXml(document, QStringLiteral("ocr-result"));
  document.appendChild(element);

  const ocr::OcrResult restored(document.documentElement());
  BOOST_CHECK(restored.isEmpty());
  BOOST_CHECK_EQUAL(restored.imageWidth(), 1725);
  BOOST_CHECK_EQUAL(restored.imageHeight(), 2650);
  BOOST_CHECK_EQUAL(restored.language().toStdString(), "en-US");
  BOOST_CHECK(restored.accurateRecognition());
  BOOST_CHECK(!restored.languageCorrection());
  BOOST_CHECK_EQUAL(restored.outputFileSize(), 123456);
  BOOST_CHECK_EQUAL(restored.outputFileMTime(), 987654321);
  BOOST_CHECK_EQUAL(restored.recognitionSchema(), 1);
}

BOOST_AUTO_TEST_CASE(oldProjectResultIsInvalidatedByMissingSchema) {
  QDomDocument document;
  BOOST_REQUIRE(document.setContent(
      QStringLiteral("<ocr-result imageWidth=\"100\" imageHeight=\"200\" language=\"en-US\"/>")));

  const ocr::OcrResult restored(document.documentElement());
  BOOST_CHECK_EQUAL(restored.recognitionSchema(), 0);
  BOOST_CHECK_EQUAL(restored.outputFileSize(), -1);
  BOOST_CHECK_EQUAL(restored.outputFileMTime(), -1);
}

BOOST_AUTO_TEST_SUITE_END()
