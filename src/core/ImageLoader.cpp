// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ImageLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QtGui/QImageReader>

#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>

#include "ImageId.h"
#include "PdfReader.h"
#include "TiffReader.h"

namespace {
struct CacheKey {
  QString canonicalPath;
  int pageNum;
  qint64 fileSize;
  qint64 modificationTime;
  int pdfRenderDpi;

  bool operator==(const CacheKey& other) const {
    return canonicalPath == other.canonicalPath && pageNum == other.pageNum && fileSize == other.fileSize
        && modificationTime == other.modificationTime && pdfRenderDpi == other.pdfRenderDpi;
  }
};

struct CacheKeyHash {
  size_t operator()(const CacheKey& key) const noexcept {
    size_t seed = qHash(key.canonicalPath);
    seed ^= qHash(key.pageNum) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= qHash(key.fileSize) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= qHash(key.modificationTime) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= qHash(key.pdfRenderDpi) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

CacheKey cacheKeyFor(const QString& filePath, const int pageNum, const int pdfRenderDpi) {
  const QFileInfo info(filePath);
  const QString canonicalPath = info.canonicalFilePath();
  return {canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath, pageNum, info.size(),
          info.lastModified().toMSecsSinceEpoch(), pdfRenderDpi};
}

QString canonicalPathFor(const QString& filePath) {
  const QFileInfo info(filePath);
  const QString canonicalPath = info.canonicalFilePath();
  return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

class DecodedImageCache {
 public:
  static DecodedImageCache& instance() {
    static DecodedImageCache cache;
    return cache;
  }

  QImage get(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_index.find(key);
    if (found == m_index.end()) {
      return {};
    }
    m_lru.splice(m_lru.begin(), m_lru, found->second);
    return found->second->image;
  }

  void put(const CacheKey& key, const QImage& image) {
    if (image.isNull() || m_maxBytes == 0) {
      return;
    }

    const quint64 imageBytes = image.sizeInBytes();
    if (imageBytes > m_maxBytes) {
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = m_index.find(key);
    if (found != m_index.end()) {
      m_lru.splice(m_lru.begin(), m_lru, found->second);
      return;
    }

    m_lru.push_front({key, image, imageBytes});
    m_index.emplace(key, m_lru.begin());
    m_currentBytes += imageBytes;
    while (m_currentBytes > m_maxBytes && !m_lru.empty()) {
      const auto last = std::prev(m_lru.end());
      m_currentBytes -= last->bytes;
      m_index.erase(last->key);
      m_lru.erase(last);
    }
  }

  void invalidate(const QString& canonicalPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_lru.begin(); it != m_lru.end();) {
      if (it->key.canonicalPath == canonicalPath) {
        m_currentBytes -= it->bytes;
        m_index.erase(it->key);
        it = m_lru.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  struct Entry {
    CacheKey key;
    QImage image;
    quint64 bytes;
  };

  DecodedImageCache() : m_maxBytes(readMaxBytes()) {}

  static quint64 readMaxBytes() {
    constexpr int defaultMegabytes = 2048;
    constexpr int maximumMegabytes = 16384;
    bool ok = false;
    const int configured = qEnvironmentVariableIntValue("SCANTAILOR_IMAGE_CACHE_MB", &ok);
    const int megabytes = ok ? std::clamp(configured, 0, maximumMegabytes) : defaultMegabytes;
    return static_cast<quint64>(megabytes) * 1024 * 1024;
  }

  std::mutex m_mutex;
  std::list<Entry> m_lru;
  std::unordered_map<CacheKey, std::list<Entry>::iterator, CacheKeyHash> m_index;
  quint64 m_currentBytes = 0;
  const quint64 m_maxBytes;
};
}  // namespace

QImage ImageLoader::load(const ImageId& imageId) {
  return load(imageId.filePath(), imageId.zeroBasedPage());
}

QImage ImageLoader::load(const QString& filePath, const int pageNum) {
  const bool isPdf = PdfReader::canRead(filePath);
  const int pdfRenderDpi = isPdf ? PdfReader::getImportDpi(filePath) : 0;
  const CacheKey cacheKey = cacheKeyFor(filePath, pageNum, pdfRenderDpi);
  QImage image = DecodedImageCache::instance().get(cacheKey);
  if (!image.isNull()) {
    return image;
  }

  // Check for PDF first (requires file path, not QIODevice)
  if (isPdf) {
    image = PdfReader::readImage(filePath, pageNum, pdfRenderDpi);
  } else {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      return {};
    }
    image = load(file, pageNum);
  }

  DecodedImageCache::instance().put(cacheKey, image);
  return image;
}

QImage ImageLoader::load(QIODevice& ioDev, const int pageNum) {
  if (TiffReader::canRead(ioDev)) {
    return TiffReader::readImage(ioDev, pageNum);
  }

  if (pageNum != 0) {
    // Qt can only load the first page of multi-page images.
    return QImage();
  }

  QImage image;
  QImageReader(&ioDev).read(&image);
  return image;
}

void ImageLoader::invalidate(const QString& filePath) {
  DecodedImageCache::instance().invalidate(canonicalPathFor(filePath));
}
