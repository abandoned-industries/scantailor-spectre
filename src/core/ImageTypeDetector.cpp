// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ImageTypeDetector.h"

#include <QImage>
#include <QImageReader>
#include <QtGlobal>

bool ImageTypeDetector::isEffectivelyMono(const QImage& image) {
  const int totalPixels = image.width() * image.height();
  const int sampleSize = qMin(10000, totalPixels);
  const int step = qMax(1, totalPixels / sampleSize);

  for (int i = 0; i < totalPixels; i += step) {
    const int x = i % image.width();
    const int y = i / image.width();
    const QRgb pixel = image.pixel(x, y);
    const int gray = qGray(pixel);
    if (gray != 0 && gray != 255) {
      return false;
    }
    if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel)) {
      return false;
    }
  }
  return true;
}

bool ImageTypeDetector::isEffectivelyGrayscale(const QImage& image, int tolerance) {
  const int totalPixels = image.width() * image.height();
  const int sampleSize = qMin(10000, totalPixels);
  const int step = qMax(1, totalPixels / sampleSize);

  for (int i = 0; i < totalPixels; i += step) {
    const int x = i % image.width();
    const int y = i / image.width();
    const QRgb pixel = image.pixel(x, y);
    const int r = qRed(pixel);
    const int g = qGreen(pixel);
    const int b = qBlue(pixel);
    // Check if RGB values are within tolerance of each other
    const int maxDiff = qMax(qMax(qAbs(r - g), qAbs(g - b)), qAbs(r - b));
    if (maxDiff > tolerance) {
      return false;
    }
  }
  return true;
}

ImageTypeDetector::Type ImageTypeDetector::detect(const QImage& image) {
  const QImage::Format fmt = image.format();

  // Check native format first
  if (fmt == QImage::Format_Mono || fmt == QImage::Format_MonoLSB) {
    return Type::Mono;
  }
  if (fmt == QImage::Format_Grayscale8 || fmt == QImage::Format_Grayscale16) {
    return Type::Grayscale;
  }

  // Sample pixels for content-based detection
  if (isEffectivelyMono(image)) {
    return Type::Mono;
  }
  if (isEffectivelyGrayscale(image)) {
    return Type::Grayscale;
  }
  return Type::Color;
}

ImageTypeDetector::Type ImageTypeDetector::detectFromFile(const QString& imagePath) {
  // Use QImageReader to load a downsampled version for efficiency
  QImageReader reader(imagePath);
  if (!reader.canRead()) {
    // Default to grayscale if we can't read the file
    return Type::Grayscale;
  }

  // Scale down large images for faster detection
  const QSize originalSize = reader.size();
  const int maxDim = 800;  // Sample at most 800x800 for detection
  if (originalSize.width() > maxDim || originalSize.height() > maxDim) {
    const qreal scale = qMin(static_cast<qreal>(maxDim) / originalSize.width(),
                             static_cast<qreal>(maxDim) / originalSize.height());
    reader.setScaledSize(QSize(qRound(originalSize.width() * scale), qRound(originalSize.height() * scale)));
  }

  const QImage image = reader.read();
  if (image.isNull()) {
    return Type::Grayscale;
  }

  return detect(image);
}
