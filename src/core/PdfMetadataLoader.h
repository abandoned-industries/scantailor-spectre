// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_PDFMETADATALOADER_H_
#define SCANTAILOR_CORE_PDFMETADATALOADER_H_

#include "ImageMetadataLoader.h"
#include "VirtualFunction.h"

class QIODevice;
class ImageMetadata;

class PdfMetadataLoader : public ImageMetadataLoader {
 protected:
  Status loadMetadata(QIODevice& ioDevice, const VirtualFunction<void, const ImageMetadata&>& out) override;
};

#endif  // SCANTAILOR_CORE_PDFMETADATALOADER_H_
