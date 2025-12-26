// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ImageView.h"

#include "ImagePresentation.h"

namespace ocr {

ImageView::ImageView(const QImage& image, const QImage& downscaledImage, const ImageTransformation& xform)
    : ImageViewBase(image, downscaledImage, ImagePresentation(xform.transform(), xform.resultingPreCropArea())) {}

ImageView::~ImageView() = default;

}  // namespace ocr
