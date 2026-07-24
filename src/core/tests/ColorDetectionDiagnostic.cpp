// Throwaway real-scan diagnostic for Finalize color detection.

#include <LeptonicaDetector.h>
#include <PdfReader.h>
#include <WhiteBalance.h>

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QPolygonF>
#include <QStringList>

#include <leptonica/allheaders.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct DetectorStats {
  float analyzedFraction = 0.0f;
  float colorFraction = 0.0f;
  float midtoneRatio = 0.0f;
  float darkRatio = 0.0f;
  float lightRatio = 0.0f;
  float maxInteriorMidtoneRatio = 0.0f;
};

PIX* qImageToPix(const QImage& source) {
  const QImage image = source.convertToFormat(QImage::Format_RGB32);
  PIX* pix = pixCreate(image.width(), image.height(), 32);
  if (!pix) {
    return nullptr;
  }

  l_uint32* data = pixGetData(pix);
  const int wpl = pixGetWpl(pix);
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* sourceLine = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    l_uint32* destLine = data + y * wpl;
    for (int x = 0; x < image.width(); ++x) {
      composeRGBAPixel(qRed(sourceLine[x]), qGreen(sourceLine[x]), qBlue(sourceLine[x]), 255, destLine + x);
    }
  }
  return pix;
}

DetectorStats measureDetectorStats(const QImage& image, const bool normalizeBrightness = false) {
  DetectorStats stats;
  PIX* pix = qImageToPix(image);
  if (!pix) {
    return stats;
  }

  pixColorFraction(pix, 10, 240, 50, 4, &stats.analyzedFraction, &stats.colorFraction);
  PIX* gray = pixConvertRGBToGray(pix, 0.0, 0.0, 0.0);
  if (normalizeBrightness && gray) {
    PIX* normalized = pixBackgroundNormSimple(gray, nullptr, nullptr);
    if (normalized) {
      pixDestroy(&gray);
      gray = normalized;
    }
  }
  NUMA* histogram = gray ? pixGetGrayHistogram(gray, 1) : nullptr;
  if (histogram) {
    float total = 0.0f;
    float dark = 0.0f;
    float midtone = 0.0f;
    float light = 0.0f;
    for (int i = 0; i < numaGetCount(histogram); ++i) {
      float count = 0.0f;
      numaGetFValue(histogram, i, &count);
      total += count;
      if (i <= 79) {
        dark += count;
      } else if (i >= 130) {
        light += count;
      } else {
        midtone += count;
      }
    }
    if (total > 0.0f) {
      stats.darkRatio = dark / total;
      stats.midtoneRatio = midtone / total;
      stats.lightRatio = light / total;
    }
  }

  if (gray) {
    const int gridSize = 6;
    const int cellW = pixGetWidth(gray) / gridSize;
    const int cellH = pixGetHeight(gray) / gridSize;
    l_uint32* data = pixGetData(gray);
    const int wpl = pixGetWpl(gray);
    if (cellW >= 50 && cellH >= 50) {
      for (int gy = 1; gy < gridSize - 1; ++gy) {
        for (int gx = 1; gx < gridSize - 1; ++gx) {
          int midtones = 0;
          int total = 0;
          for (int y = gy * cellH; y < (gy + 1) * cellH; y += 4) {
            l_uint32* line = data + y * wpl;
            for (int x = gx * cellW; x < (gx + 1) * cellW; x += 4) {
              const l_uint32 value = GET_DATA_BYTE(line, x);
              ++total;
              if (value > 60 && value < 195) {
                ++midtones;
              }
            }
          }
          if (total > 0) {
            stats.maxInteriorMidtoneRatio =
                std::max(stats.maxInteriorMidtoneRatio, static_cast<float>(midtones) / total);
          }
        }
      }
    }
  }

  numaDestroy(&histogram);
  pixDestroy(&gray);
  pixDestroy(&pix);
  return stats;
}

int castDeviation(const QColor& color) {
  if (!color.isValid()) {
    return -1;
  }
  const int average = (color.red() + color.green() + color.blue()) / 3;
  return std::max({std::abs(color.red() - average),
                   std::abs(color.green() - average),
                   std::abs(color.blue() - average)});
}

QString rgb(const QColor& color) {
  if (!color.isValid()) {
    return QStringLiteral("invalid");
  }
  return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

// Approximate the upstream auto-selected content rectangle with the envelope
// of pixels appreciably darker than the estimated paper. Finalize itself then
// performs its exact conversion: contentRectPhys.boundingRect().toRect().
QRect deriveContentRectPhys(const QImage& image) {
  const QColor background = WhiteBalance::estimateBackgroundColor(image);
  const int backgroundLuma = background.isValid()
                                 ? (background.red() + background.green() + background.blue()) / 3
                                 : 220;
  const int inkThreshold = std::clamp(backgroundLuma - 35, 70, 190);
  const int guardX = std::max(1, image.width() * 3 / 100);
  const int guardY = std::max(1, image.height() * 2 / 100);
  const int step = std::max(1, std::min(image.width(), image.height()) / 1200);

  int left = image.width();
  int top = image.height();
  int right = -1;
  int bottom = -1;
  const QImage rgbImage = image.convertToFormat(QImage::Format_RGB32);
  for (int y = guardY; y < image.height() - guardY; y += step) {
    const QRgb* line = reinterpret_cast<const QRgb*>(rgbImage.constScanLine(y));
    for (int x = guardX; x < image.width() - guardX; x += step) {
      const int luma = (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x])) / 3;
      if (luma < inkThreshold) {
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, x);
        bottom = std::max(bottom, y);
      }
    }
  }

  if (right < left || bottom < top) {
    return image.rect().adjusted(image.width() / 20, image.height() / 20,
                                 -image.width() / 20, -image.height() / 20);
  }
  const int padX = image.width() * 2 / 100;
  const int padY = image.height() * 2 / 100;
  return QRect(QPoint(left - padX, top - padY), QPoint(right + padX, bottom + padY)).intersected(image.rect());
}

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
      const int average = (qRed(line[x]) + qGreen(line[x]) + qBlue(line[x])) / 3;
      const int castR = qRed(line[x]) - average;
      const int castG = qGreen(line[x]) - average;
      const int castB = qBlue(line[x]) - average;
      const qint64 norm2 = static_cast<qint64>(castR) * castR
                           + static_cast<qint64>(castG) * castG
                           + static_cast<qint64>(castB) * castB;
      if (norm2 == 0 || norm2 > paperNorm2 * 9) {
        continue;
      }
      const qint64 dot = static_cast<qint64>(castR) * paperCastR
                         + static_cast<qint64>(castG) * paperCastG
                         + static_cast<qint64>(castB) * paperCastB;
      if (dot > 0 && dot * dot * 100 >= norm2 * paperNorm2 * 81) {
        line[x] = qRgb(average, average, average);
      }
    }
  }
  return result;
}

bool hasFinalizeMargins(const QImage& image, const QRect& contentBox) {
  if (contentBox.isEmpty() || !image.rect().contains(contentBox)) {
    return false;
  }
  const int imageArea = image.width() * image.height();
  const int marginArea = imageArea - contentBox.width() * contentBox.height();
  if (marginArea < imageArea * 0.05) {
    return false;
  }
  const int top = contentBox.top();
  const int bottom = image.height() - contentBox.bottom() - 1;
  const int left = contentBox.left();
  const int right = image.width() - contentBox.right() - 1;
  return top >= 20 || bottom >= 20 || left >= 20 || right >= 20;
}

void printStats(const char* label, const DetectorStats& stats) {
  std::printf("%s analyzed=%.6f color=%.6f midtone=%.6f dark=%.6f light=%.6f maxCellMidtone=%.6f",
              label,
              stats.analyzedFraction,
              stats.colorFraction,
              stats.midtoneRatio,
              stats.darkRatio,
              stats.lightRatio,
              stats.maxInteriorMidtoneRatio);
}

void diagnosePage(const QString& pdfPath,
                  const int pageIndex,
                  const int dpi,
                  const QString& fixtureDir,
                  const bool fullContent) {
  const QImage image = PdfReader::readImage(pdfPath, pageIndex, dpi);
  if (image.isNull()) {
    std::printf("PAGE index=%d ERROR=rasterization_failed\n", pageIndex);
    return;
  }

  const QRect contentRectPhys = fullContent ? image.rect() : deriveContentRectPhys(image);
  const QPolygonF upstreamContentRectPhys{QRectF(contentRectPhys)};
  const QRect contentBox = upstreamContentRectPhys.boundingRect().toRect().intersected(image.rect());

  const bool marginsExist = hasFinalizeMargins(image, contentBox);
  const QColor fallbackPaper = WhiteBalance::estimateBackgroundColor(image, contentBox);
  const QColor selectedPaper = WhiteBalance::detectPaperColor(image, contentBox);
  // detectPaperColor prefers the margin sampler whenever margins exist. On
  // these bright-paper scans it succeeds, so this is the exact sampled value
  // used by Finalize without advancing the sampler's deterministic RNG twice.
  const QColor marginPaper = marginsExist ? selectedPaper : QColor();
  const bool outerNeutralize = selectedPaper.isValid() && WhiteBalance::hasSignificantCast(selectedPaper);

  QImage outerImage = outerNeutralize ? WhiteBalance::apply(image, selectedPaper) : image;
  const QImage rawCrop = image.copy(contentBox);
  const QImage outerCrop = outerImage.copy(contentBox);
  const QColor rawHelperPaper = WhiteBalance::estimateBackgroundColor(rawCrop);
  const QImage rawHelperCrop =
      WhiteBalance::hasSignificantCast(rawHelperPaper) ? WhiteBalance::apply(rawCrop, rawHelperPaper) : rawCrop;
  const QImage selectiveCrop = neutralizeDominantPaperCast(rawCrop, rawHelperPaper);
  const LeptonicaDetector::ColorType rawCompensatedVerdict =
      LeptonicaDetector::detectWithCastCompensation(rawCrop);
  const LeptonicaDetector::ColorType plainVerdict = LeptonicaDetector::detect(outerCrop);

  const QColor helperPaper = WhiteBalance::estimateBackgroundColor(outerCrop);
  const bool helperNeutralize = plainVerdict == LeptonicaDetector::ColorType::Color
                                && helperPaper.isValid()
                                && WhiteBalance::hasSignificantCast(helperPaper);
  const QImage helperCrop = helperNeutralize ? WhiteBalance::apply(outerCrop, helperPaper) : outerCrop;
  const LeptonicaDetector::ColorType legacyFinalVerdict = LeptonicaDetector::detect(helperCrop);
  const LeptonicaDetector::ColorType outerCompensatedVerdict =
      LeptonicaDetector::detectWithCastCompensation(outerCrop);
  // Fixed Finalize path defers automatic paper correction and gives the raw
  // crop to detectWithCastCompensation, preserving the cast direction.
  const LeptonicaDetector::ColorType finalVerdict = rawCompensatedVerdict;

  std::printf("PAGE index=%d dpi=%d size=%dx%d content=%d,%d,%d,%d margins=%s "
              "marginRGB=%s marginDev=%d fallbackRGB=%s fallbackDev=%d selectedRGB=%s selectedDev=%d "
              "outerNeutralize=%s ",
              pageIndex,
              dpi,
              image.width(),
              image.height(),
              contentBox.x(),
              contentBox.y(),
              contentBox.width(),
              contentBox.height(),
              marginsExist ? "yes" : "no",
              qPrintable(rgb(marginPaper)),
              castDeviation(marginPaper),
              qPrintable(rgb(fallbackPaper)),
              castDeviation(fallbackPaper),
              qPrintable(rgb(selectedPaper)),
              castDeviation(selectedPaper),
              outerNeutralize ? "yes" : "no");
  printStats("raw", measureDetectorStats(rawCrop));
  std::printf(" rawHelperRGB=%s rawHelperDev=%d ",
              qPrintable(rgb(rawHelperPaper)),
              castDeviation(rawHelperPaper));
  printStats("afterRawHelper", measureDetectorStats(rawHelperCrop));
  std::printf(" ");
  printStats("afterSelective", measureDetectorStats(selectiveCrop));
  std::printf(" ");
  printStats("afterSelectiveNormalized", measureDetectorStats(selectiveCrop, true));
  std::printf(" rawCompensated=%s ",
              LeptonicaDetector::colorTypeToString(rawCompensatedVerdict));
  std::printf(" ");
  printStats("afterOuter", measureDetectorStats(outerCrop));
  std::printf(" plain=%s helperRGB=%s helperDev=%d helperNeutralize=%s ",
              LeptonicaDetector::colorTypeToString(plainVerdict),
              qPrintable(rgb(helperPaper)),
              castDeviation(helperPaper),
              helperNeutralize ? "yes" : "no");
  printStats("afterHelper", measureDetectorStats(helperCrop));
  std::printf(" legacyFinal=%s outerCompensated=%s final=%s\n",
              LeptonicaDetector::colorTypeToString(legacyFinalVerdict),
              LeptonicaDetector::colorTypeToString(outerCompensatedVerdict),
              LeptonicaDetector::colorTypeToString(finalVerdict));
  std::fflush(stdout);

  if (!fixtureDir.isEmpty() && (pageIndex == 92 || pageIndex == 390)) {
    const QRect crop = pageIndex == 92
                           ? contentBox
                           : QRect(image.width() * 4 / 100,
                                   image.height() * 3 / 100,
                                   image.width() * 92 / 100,
                                   image.height() * 88 / 100);
    const QImage fixture = image.copy(crop).scaledToWidth(480, Qt::SmoothTransformation);
    const QString name = pageIndex == 92 ? QStringLiteral("toned-text-page-92.png")
                                         : QStringLiteral("color-card-page-390.png");
    fixture.save(QDir(fixtureDir).filePath(name), "PNG");
  }
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  const QStringList args = app.arguments();
  if (args.size() < 3) {
    std::fprintf(stderr, "usage: %s PDF PAGE_INDEX... [--dpi=N]\n", argv[0]);
    return 2;
  }

  int dpi = 300;
  QString fixtureDir;
  bool fullContent = false;
  for (int i = 2; i < args.size(); ++i) {
    if (args[i].startsWith(QStringLiteral("--dpi="))) {
      dpi = args[i].mid(6).toInt();
    } else if (args[i].startsWith(QStringLiteral("--save-fixtures="))) {
      fixtureDir = args[i].mid(16);
    } else if (args[i] == QStringLiteral("--full-content")) {
      fullContent = true;
    }
  }
  for (int i = 2; i < args.size(); ++i) {
    if (!args[i].startsWith(QStringLiteral("--"))) {
      diagnosePage(args[1], args[i].toInt(), dpi, fixtureDir, fullContent);
    }
  }
  return 0;
}
