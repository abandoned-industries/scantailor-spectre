// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_THUMBNAIL_H_
#define SCANTAILOR_OCR_THUMBNAIL_H_

#include <memory>

#include "PageId.h"
#include "ThumbnailBase.h"

class ThumbnailPixmapCache;
class QSizeF;

namespace ocr {
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

 private:
  std::shared_ptr<Settings> m_settings;
  PageId m_pageId;
};
}  // namespace ocr

#endif  // SCANTAILOR_OCR_THUMBNAIL_H_
