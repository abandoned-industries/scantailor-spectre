// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfMetadataLoader.h"

#include "PdfReader.h"

ImageMetadataLoader::Status PdfMetadataLoader::loadMetadata(QIODevice& ioDevice,
                                                            const VirtualFunction<void, const ImageMetadata&>& out) {
  return PdfReader::readMetadata(ioDevice, out);
}
