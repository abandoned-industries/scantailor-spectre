// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "ImageLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QtGui/QImageReader>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif

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

  QImage load(const CacheKey& key, const bool isPdf, const std::function<QImage()>& decoder) {
    std::shared_ptr<PendingDecode> pending;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      const auto cached = m_index.find(key);
      if (cached != m_index.end()) {
        m_cacheHits.fetch_add(1, std::memory_order_relaxed);
        m_lru.splice(m_lru.begin(), m_lru, cached->second);
        return cached->second->image;
      }

      const auto active = m_pendingDecodes.find(key);
      if (active != m_pendingDecodes.end()) {
        pending = active->second;
        m_coalescedWaiters.fetch_add(1, std::memory_order_relaxed);
        pending->completedCondition.wait(lock, [&pending] { return pending->completed; });
        if (pending->error) {
          std::rethrow_exception(pending->error);
        }
        return pending->image;
      }

      pending = std::make_shared<PendingDecode>();
      m_pendingDecodes.emplace(key, pending);
      m_cacheMisses.fetch_add(1, std::memory_order_relaxed);
      m_leaderDecodes.fetch_add(1, std::memory_order_relaxed);
    }

    QImage image;
    std::exception_ptr error;
    try {
      image = decoder();
    } catch (...) {
      error = std::current_exception();
    }

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (!error) {
        putUnlocked(key, image);
      }
      pending->image = image;
      pending->error = error;
      pending->completed = true;
      m_pendingDecodes.erase(key);
    }

    if (!image.isNull()) {
      m_decodedBytes.fetch_add(image.sizeInBytes(), std::memory_order_relaxed);
    }
    if (isPdf) {
      m_pdfRasterizations.fetch_add(1, std::memory_order_relaxed);
    }
    pending->completedCondition.notify_all();

    if (error) {
      std::rethrow_exception(error);
    }
    return image;
  }

  void put(const CacheKey& key, const QImage& image) {
    std::lock_guard<std::mutex> lock(m_mutex);
    putUnlocked(key, image);
  }

  void putUnlocked(const CacheKey& key, const QImage& image) {
    if (image.isNull() || m_maxBytes == 0) {
      return;
    }

    const quint64 imageBytes = image.sizeInBytes();
    if (imageBytes > m_maxBytes) {
      return;
    }

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

  ImageLoader::Statistics statistics() const {
    return {
        m_cacheHits.load(std::memory_order_relaxed),
        m_cacheMisses.load(std::memory_order_relaxed),
        m_leaderDecodes.load(std::memory_order_relaxed),
        m_coalescedWaiters.load(std::memory_order_relaxed),
        m_pdfRasterizations.load(std::memory_order_relaxed),
        m_decodedBytes.load(std::memory_order_relaxed)
    };
  }

  void resetStatistics() {
    m_cacheHits.store(0, std::memory_order_relaxed);
    m_cacheMisses.store(0, std::memory_order_relaxed);
    m_leaderDecodes.store(0, std::memory_order_relaxed);
    m_coalescedWaiters.store(0, std::memory_order_relaxed);
    m_pdfRasterizations.store(0, std::memory_order_relaxed);
    m_decodedBytes.store(0, std::memory_order_relaxed);
  }

 private:
  struct Entry {
    CacheKey key;
    QImage image;
    quint64 bytes;
  };

  struct PendingDecode {
    std::condition_variable completedCondition;
    QImage image;
    std::exception_ptr error;
    bool completed = false;
  };

  DecodedImageCache() : m_maxBytes(readMaxBytes()) {}

  static quint64 readMaxBytes() {
    constexpr int defaultMegabytes = 2048;
    constexpr int maximumMegabytes = 16384;
    bool ok = false;
    const int configured = qEnvironmentVariableIntValue("SCANTAILOR_IMAGE_CACHE_MB", &ok);
    if (ok) {
      return static_cast<quint64>(std::clamp(configured, 0, maximumMegabytes)) * 1024 * 1024;
    }

    int megabytes = defaultMegabytes;
#if defined(Q_OS_MACOS)
    // A fixed 2 GiB cache causes each whole-book stage to re-rasterize PDFs
    // even on high-memory Apple Silicon. Keep a modest fraction of RAM so the
    // source-page working set can survive between stages without pressuring
    // lower-memory Macs. The environment override remains authoritative.
    std::uint64_t physicalBytes = 0;
    size_t physicalBytesSize = sizeof(physicalBytes);
    if (sysctlbyname("hw.memsize", &physicalBytes, &physicalBytesSize, nullptr, 0) == 0) {
      constexpr std::uint64_t bytesPerMegabyte = 1024 * 1024;
      const auto adaptiveMegabytes = static_cast<int>(physicalBytes / 16 / bytesPerMegabyte);
      megabytes = std::clamp(adaptiveMegabytes, defaultMegabytes, maximumMegabytes);
    }
#endif
    return static_cast<quint64>(megabytes) * 1024 * 1024;
  }

  std::mutex m_mutex;
  std::list<Entry> m_lru;
  std::unordered_map<CacheKey, std::list<Entry>::iterator, CacheKeyHash> m_index;
  std::unordered_map<CacheKey, std::shared_ptr<PendingDecode>, CacheKeyHash> m_pendingDecodes;
  quint64 m_currentBytes = 0;
  const quint64 m_maxBytes;
  std::atomic<std::uint64_t> m_cacheHits{0};
  std::atomic<std::uint64_t> m_cacheMisses{0};
  std::atomic<std::uint64_t> m_leaderDecodes{0};
  std::atomic<std::uint64_t> m_coalescedWaiters{0};
  std::atomic<std::uint64_t> m_pdfRasterizations{0};
  std::atomic<std::uint64_t> m_decodedBytes{0};
};
}  // namespace

QImage ImageLoader::load(const ImageId& imageId) {
  return load(imageId.filePath(), imageId.zeroBasedPage());
}

QImage ImageLoader::load(const QString& filePath, const int pageNum) {
  const bool isPdf = PdfReader::canRead(filePath);
  const int pdfRenderDpi = isPdf ? PdfReader::getImportDpi(filePath) : 0;
  const CacheKey cacheKey = cacheKeyFor(filePath, pageNum, pdfRenderDpi);
  return DecodedImageCache::instance().load(cacheKey, isPdf, [&] {
    // Check for PDF first (requires file path, not QIODevice).
    if (isPdf) {
      return PdfReader::readImage(filePath, pageNum, pdfRenderDpi);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      return QImage();
    }
    return load(file, pageNum);
  });
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

ImageLoader::Statistics ImageLoader::statistics() {
  return DecodedImageCache::instance().statistics();
}

void ImageLoader::resetStatistics() {
  DecodedImageCache::instance().resetStatistics();
}
