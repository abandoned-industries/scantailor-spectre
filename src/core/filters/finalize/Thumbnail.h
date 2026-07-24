// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_THUMBNAIL_H_
#define SCANTAILOR_FINALIZE_THUMBNAIL_H_

#include <memory>

#include "PageId.h"
#include "ThumbnailBase.h"

class ThumbnailPixmapCache;
class QSizeF;

namespace finalize {
class Settings;

class Thumbnail : public ThumbnailBase {
 public:
  Thumbnail(std::shared_ptr<ThumbnailPixmapCache> thumbnailCache,
            const QSizeF& maxSize,
            const ImageId& imageId,
            const ImageTransformation& xform,
            std::shared_ptr<Settings> settings,
            const PageId& pageId);

  void paintOverImage(QPainter& painter,
                      const QTransform& imageToDisplay,
                      const QTransform& thumbToDisplay) override;

 protected:
  void transformPixmap(QPixmap& pixmap) const override;

 private:
  std::shared_ptr<Settings> m_settings;
  PageId m_pageId;
};
}  // namespace finalize

#endif  // SCANTAILOR_FINALIZE_THUMBNAIL_H_
