// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Thumbnail.h"

#include <QImage>
#include <QPainter>
#include <utility>

#include "Settings.h"

namespace finalize {

Thumbnail::Thumbnail(std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                     const QSizeF& maxSize,
                     const ImageId& imageId,
                     const ImageTransformation& xform,
                     std::shared_ptr<Settings> settings,
                     const PageId& pageId)
    : ThumbnailBase(std::move(thumbnailCache), maxSize, imageId, xform),
      m_settings(std::move(settings)),
      m_pageId(pageId) {}

void Thumbnail::paintOverImage(QPainter& painter,
                               const QTransform& imageToDisplay,
                               const QTransform& thumbToDisplay) {
  // Display color mode indicator
  const ColorMode mode = m_settings->getColorMode(m_pageId);

  QString label;
  QColor color;
  switch (mode) {
    case ColorMode::BlackAndWhite:
      label = "B&W";
      color = Qt::black;
      break;
    case ColorMode::Grayscale:
      label = "Gray";
      color = Qt::darkGray;
      break;
    case ColorMode::Color:
      label = "Color";
      color = Qt::blue;
      break;
  }

  // Draw label in corner
  painter.save();
  painter.resetTransform();

  QFont font = painter.font();
  font.setPointSize(8);
  font.setBold(true);
  painter.setFont(font);

  const QRect boundingRect = painter.boundingRect(QRect(), Qt::AlignLeft, label);
  const int padding = 2;
  const QRect bgRect(padding, padding, boundingRect.width() + 2 * padding, boundingRect.height() + padding);

  painter.fillRect(bgRect, QColor(255, 255, 255, 200));
  painter.setPen(color);
  painter.drawText(bgRect, Qt::AlignCenter, label);

  painter.restore();
}

void Thumbnail::transformPixmap(QPixmap& pixmap) const {
  const ColorMode mode = m_settings->getColorMode(m_pageId);
  if (mode == ColorMode::Color) {
    return;  // No transformation needed
  }

  // For both B&W and Grayscale modes, just show grayscale preview.
  // Don't apply B&W binarization to thumbnails - the smooth scaling
  // makes text too light (gray anti-aliasing), and thresholding turns
  // everything white. Actual binarization happens in output generation.
  QImage img = pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);
  pixmap = QPixmap::fromImage(img);
}

}  // namespace finalize
