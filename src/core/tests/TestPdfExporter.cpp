// Copyright (C) 2026  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <boost/test/unit_test.hpp>

#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "PdfExporter.h"

#ifdef Q_OS_MACOS
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace {

QString savePage(const QString& path, const QSize& size, const QColor& color) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(color);
  image.setDotsPerMeterX(3937);
  image.setDotsPerMeterY(3937);
  BOOST_REQUIRE(image.save(path, "PNG"));
  return path;
}

#ifdef Q_OS_MACOS
CGPDFDocumentRef openPdf(const QString& path) {
  const QByteArray encodedPath = QFile::encodeName(path);
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(encodedPath.constData()),
      encodedPath.size(),
      false);
  CGPDFDocumentRef document = CGPDFDocumentCreateWithURL(url);
  CFRelease(url);
  return document;
}
#endif

}  // namespace

BOOST_AUTO_TEST_SUITE(PdfExporterTestSuite)

BOOST_AUTO_TEST_CASE(exports_pages_in_input_order) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());
  const QString first = savePage(dir.filePath("first.png"), QSize(100, 200), Qt::red);
  const QString second = savePage(dir.filePath("second.png"), QSize(300, 100), Qt::blue);
  const QString pdfPath = dir.filePath("ordered.pdf");

  BOOST_REQUIRE(PdfExporter::exportToPdf(
      {first, second}, pdfPath, "Order test", "ScanTailor", PdfExporter::Quality::High,
      false, 0));

#ifdef Q_OS_MACOS
  CGPDFDocumentRef document = openPdf(pdfPath);
  BOOST_REQUIRE(document != nullptr);
  BOOST_CHECK_EQUAL(CGPDFDocumentGetNumberOfPages(document), 2);
  const CGRect firstBox = CGPDFPageGetBoxRect(CGPDFDocumentGetPage(document, 1), kCGPDFMediaBox);
  const CGRect secondBox = CGPDFPageGetBoxRect(CGPDFDocumentGetPage(document, 2), kCGPDFMediaBox);
  BOOST_CHECK(firstBox.size.height > firstBox.size.width);
  BOOST_CHECK(secondBox.size.width > secondBox.size.height);
  CGPDFDocumentRelease(document);
#else
  BOOST_CHECK(QFileInfo::exists(pdfPath));
#endif
}

BOOST_AUTO_TEST_CASE(cancellation_preserves_existing_destination) {
  QTemporaryDir dir;
  BOOST_REQUIRE(dir.isValid());
  const QString first = savePage(dir.filePath("first.png"), QSize(100, 200), Qt::red);
  const QString second = savePage(dir.filePath("second.png"), QSize(300, 100), Qt::blue);
  const QString pdfPath = dir.filePath("existing.pdf");
  QFile existing(pdfPath);
  BOOST_REQUIRE(existing.open(QIODevice::WriteOnly));
  BOOST_REQUIRE_EQUAL(existing.write("existing"), 8);
  existing.close();

  const bool exported = PdfExporter::exportToPdf(
      {first, second}, pdfPath, {}, {}, PdfExporter::Quality::High, false, 0, {},
      [](int current, int) { return current < 2; });
  BOOST_CHECK(!exported);

  BOOST_REQUIRE(existing.open(QIODevice::ReadOnly));
  BOOST_CHECK_EQUAL(existing.readAll().toStdString(), std::string("existing"));
}

BOOST_AUTO_TEST_SUITE_END()
