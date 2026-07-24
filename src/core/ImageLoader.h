// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_CORE_IMAGELOADER_H_
#define SCANTAILOR_CORE_IMAGELOADER_H_

#include <cstdint>

class ImageId;
class QImage;
class QString;
class QIODevice;

class ImageLoader {
 public:
  struct Statistics {
    // Immediate lookups satisfied by an already-decoded cache entry.
    std::uint64_t cacheHits = 0;
    // Unique misses. Each miss elects exactly one decoding leader.
    std::uint64_t cacheMisses = 0;
    std::uint64_t leaderDecodes = 0;
    // Same-key callers that waited for an already-running leader.
    std::uint64_t coalescedWaiters = 0;
    std::uint64_t pdfRasterizations = 0;
    std::uint64_t decodedBytes = 0;
  };

  static QImage load(const QString& filePath, int pageNum = 0);

  static QImage load(const ImageId& imageId);

  static QImage load(QIODevice& ioDev, int pageNum);

  static void invalidate(const QString& filePath);

  static Statistics statistics();

  static void resetStatistics();
};


#endif
