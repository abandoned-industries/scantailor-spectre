// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_IMAGEVIEW_H_
#define SCANTAILOR_FINALIZE_IMAGEVIEW_H_

#include <QImage>

#include "ImageTransformation.h"
#include "ImageViewBase.h"

namespace finalize {
class ImageView : public ImageViewBase {
  Q_OBJECT

 public:
  ImageView(const QImage& image, const QImage& downscaledImage, const ImageTransformation& xform);

  ~ImageView() override;
};
}  // namespace finalize

#endif  // SCANTAILOR_FINALIZE_IMAGEVIEW_H_
