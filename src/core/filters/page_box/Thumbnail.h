// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_THUMBNAIL_H_
#define SCANTAILOR_PAGE_BOX_THUMBNAIL_H_

#include <QRectF>

#include "ThumbnailBase.h"

class QSizeF;
class ThumbnailPixmapCache;
class ImageId;
class ImageTransformation;

namespace page_box {
class Thumbnail : public ThumbnailBase {
 public:
  Thumbnail(std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
            const QSizeF& maxSize,
            const ImageId& imageId,
            const ImageTransformation& xform,
            const QRectF& pageRect,
            bool deviant);

  void paintOverImage(QPainter& painter, const QTransform& imageToDisplay, const QTransform& thumbToDisplay) override;

 private:
  QRectF m_pageRect;
  bool m_deviant;
};
}  // namespace page_box
#endif
