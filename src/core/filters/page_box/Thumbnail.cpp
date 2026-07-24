// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Thumbnail.h"

#include <QPainter>
#include <utility>

namespace page_box {
Thumbnail::Thumbnail(std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
                     const QSizeF& maxSize,
                     const ImageId& imageId,
                     const ImageTransformation& xform,
                     const QRectF& pageRect,
                     bool deviant)
    : ThumbnailBase(std::move(thumbnailCache), maxSize, imageId, xform),
      m_pageRect(pageRect),
      m_deviant(deviant) {}

void Thumbnail::paintOverImage(QPainter& painter, const QTransform& imageToDisplay, const QTransform& thumbToDisplay) {
  if (!m_pageRect.isNull()) {
    QRectF pageRect(virtToThumb().mapRect(m_pageRect));

    painter.setRenderHint(QPainter::Antialiasing, false);

    QPen pen(QColor(0xff, 0x7f, 0x00));
    pen.setWidth(1);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(pageRect);
  }

  if (m_deviant) {
    paintDeviant(painter);
  }
}
}  // namespace page_box
