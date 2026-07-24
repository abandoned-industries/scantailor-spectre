// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Thumbnail.h"

#include <QFontMetrics>
#include <QPainter>
#include <utility>

#include "Settings.h"

namespace ocr {

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
  const bool hasOcr = m_settings->hasOcrResult(m_pageId);
  if (!hasOcr) {
    return;  // No indicator needed
  }

  // Draw "OCR" label in top-left corner
  painter.save();

  // Use a font size relative to thumbnail size
  QFont font("Sans");
  font.setWeight(QFont::Bold);
  font.setPixelSize(static_cast<int>(boundingRect().height() / 6));
  painter.setFont(font);

  const QString label = "OCR";
  const QRectF thumbRect = boundingRect();
  const QFontMetrics fm(font);
  const int textWidth = fm.horizontalAdvance(label);
  const int textHeight = fm.height();
  const int padding = 3;

  // Position in top-left corner of the thumbnail
  const QRectF bgRect(thumbRect.left() + padding, thumbRect.top() + padding,
                      textWidth + 2 * padding, textHeight + padding);

  // Green background to indicate success/completion
  painter.fillRect(bgRect, QColor(34, 139, 34, 200));  // Forest green with alpha
  painter.setPen(Qt::white);
  painter.drawText(bgRect, Qt::AlignCenter, label);

  painter.restore();
}

}  // namespace ocr
