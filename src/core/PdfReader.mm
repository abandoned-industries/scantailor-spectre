// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PdfReader.h"

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

#include <limits>
#include <cmath>
#include <map>
#include <vector>

// Static storage for PDF import DPI settings
static QMutex s_importDpiMutex;
static std::map<QString, int> s_importDpiMap;

#include "Dpi.h"
#include "ImageMetadata.h"

#ifdef Q_OS_MACOS
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Thread-safe PDF document manager using CGPDFDocument.
class PdfDocumentManager {
 public:
  static PdfDocumentManager& instance() {
    static PdfDocumentManager s_instance;
    return s_instance;
  }

  QMutex& mutex() { return m_mutex; }

  bool ensureLoaded(const QString& filePath) {
    if (m_loadedFilePath == filePath && m_doc) {
      return true;
    }

    if (m_doc) {
      CGPDFDocumentRelease(m_doc);
      m_doc = nullptr;
    }
    m_loadedFilePath.clear();

    @autoreleasepool {
      NSString* nsPath = filePath.toNSString();
      NSURL* url = [NSURL fileURLWithPath:nsPath];
      m_doc = CGPDFDocumentCreateWithURL((__bridge CFURLRef)url);
    }

    if (!m_doc) {
      qDebug() << "PdfReader: Failed to load PDF:" << filePath;
      return false;
    }

    m_loadedFilePath = filePath;
    return true;
  }

  CGPDFDocumentRef document() { return m_doc; }

  size_t pageCount() {
    return m_doc ? CGPDFDocumentGetNumberOfPages(m_doc) : 0;
  }

 private:
  PdfDocumentManager() = default;
  ~PdfDocumentManager() {
    if (m_doc) {
      CGPDFDocumentRelease(m_doc);
    }
  }

  PdfDocumentManager(const PdfDocumentManager&) = delete;
  PdfDocumentManager& operator=(const PdfDocumentManager&) = delete;

  QMutex m_mutex;
  CGPDFDocumentRef m_doc = nullptr;
  QString m_loadedFilePath;
};

// Get the effective box for a PDF page (cropBox if available, otherwise mediaBox)
static CGRect getEffectiveBox(CGPDFPageRef page) {
  CGRect cropBox = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
  if (CGRectIsEmpty(cropBox)) {
    return CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
  }
  return cropBox;
}

// Get the display size of a page accounting for rotation
static CGSize getDisplaySize(CGPDFPageRef page) {
  CGRect box = getEffectiveBox(page);
  int rotation = CGPDFPageGetRotationAngle(page);

  // 90 or 270 degree rotation swaps width and height
  if (rotation == 90 || rotation == 270) {
    return CGSizeMake(box.size.height, box.size.width);
  }
  return box.size;
}

static CGFloat getUserUnit(CGPDFPageRef page) {
  const CGPDFDictionaryRef dict = CGPDFPageGetDictionary(page);
  if (!dict) {
    return 1.0;
  }
  CGPDFReal userUnit = 1.0;
  if (CGPDFDictionaryGetNumber(dict, "UserUnit", &userUnit)) {
    if (userUnit > 0.0) {
      return static_cast<CGFloat>(userUnit);
    }
  }
  return 1.0;
}

// Callback context for scanning XObjects in a page's content stream
struct XObjectScanContext {
  CGPDFDictionaryRef pageResources;
  CGFloat pageWidth;   // Display width in points
  CGFloat pageHeight;  // Display height in points
  CGFloat userUnit;
  int maxDpi;
};

// Scan a Resources dictionary for image XObjects and calculate their effective DPI
static void scanResourcesForImages(CGPDFDictionaryRef resources, CGFloat pageWidth, CGFloat pageHeight,
                                    CGFloat userUnit, int& maxDpi) {
  if (!resources) return;

  CGPDFDictionaryRef xobjects = nullptr;
  if (!CGPDFDictionaryGetDictionary(resources, "XObject", &xobjects) || !xobjects) {
    return;
  }

  // Context to pass to the applier callback
  struct ApplyContext {
    CGFloat pageWidth;
    CGFloat pageHeight;
    CGFloat userUnit;
    int* maxDpi;
  };
  ApplyContext ctx = {pageWidth, pageHeight, userUnit, &maxDpi};

  // Iterate over all XObjects using block with 3 parameters
  CGPDFDictionaryApplyBlock(xobjects, ^bool(const char* key, CGPDFObjectRef value, void* info) {
    (void)key;  // Unused
    ApplyContext* ctxPtr = static_cast<ApplyContext*>(info);

    CGPDFStreamRef stream = nullptr;
    if (CGPDFObjectGetValue(value, kCGPDFObjectTypeStream, &stream) && stream) {
      CGPDFDictionaryRef streamDict = CGPDFStreamGetDictionary(stream);
      if (streamDict) {
        const char* subtype = nullptr;
        if (CGPDFDictionaryGetName(streamDict, "Subtype", &subtype) && subtype) {
          if (strcmp(subtype, "Image") == 0) {
            // Found an image! Get its dimensions
            CGPDFInteger imgWidth = 0, imgHeight = 0;
            if (CGPDFDictionaryGetInteger(streamDict, "Width", &imgWidth) &&
                CGPDFDictionaryGetInteger(streamDict, "Height", &imgHeight) &&
                imgWidth > 0 && imgHeight > 0) {
              // Calculate effective DPI
              // Assume image fills the page (conservative estimate)
              // DPI = image_pixels / (page_points / 72)
              const CGFloat pgWidthInches = (ctxPtr->pageWidth * ctxPtr->userUnit) / 72.0;
              const CGFloat pgHeightInches = (ctxPtr->pageHeight * ctxPtr->userUnit) / 72.0;

              int effectiveDpiX = (pgWidthInches > 0) ? static_cast<int>(imgWidth / pgWidthInches) : 0;
              int effectiveDpiY = (pgHeightInches > 0) ? static_cast<int>(imgHeight / pgHeightInches) : 0;

              // Use the larger dimension's DPI (more conservative)
              int effectiveDpi = std::max(effectiveDpiX, effectiveDpiY);
              if (effectiveDpi > *ctxPtr->maxDpi) {
                *ctxPtr->maxDpi = effectiveDpi;
              }
            }
          }
        }
      }
    }
    return true;  // Continue iterating
  }, &ctx);
}

// Detect effective DPI from embedded images in PDF
// Samples first few pages and returns maximum DPI found, rounded to standard values
static int detectEffectiveDpi(CGPDFDocumentRef document, int samplePages = 5) {
  if (!document) return PdfReader::DEFAULT_RENDER_DPI;

  const size_t pageCount = CGPDFDocumentGetNumberOfPages(document);
  if (pageCount == 0) return PdfReader::DEFAULT_RENDER_DPI;

  int maxDpi = 0;
  const int pagesToSample = std::min(samplePages, static_cast<int>(pageCount));

  for (int i = 1; i <= pagesToSample; ++i) {
    CGPDFPageRef page = CGPDFDocumentGetPage(document, i);
    if (!page) continue;

    CGRect mediaBox = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);
    CGFloat userUnit = getUserUnit(page);

    // Get page resources dictionary
    CGPDFDictionaryRef pageDict = CGPDFPageGetDictionary(page);
    if (pageDict) {
      CGPDFDictionaryRef resources = nullptr;
      if (CGPDFDictionaryGetDictionary(pageDict, "Resources", &resources) && resources) {
        scanResourcesForImages(resources, mediaBox.size.width, mediaBox.size.height, userUnit, maxDpi);
      }
    }
  }

  // Round to nearest standard DPI value
  if (maxDpi > 500) return 600;
  if (maxDpi > 350) return 400;
  if (maxDpi > 0) return 300;

  return PdfReader::DEFAULT_RENDER_DPI;  // Default fallback
}

#endif  // Q_OS_MACOS

bool PdfReader::checkMagic(const QByteArray& data) {
  return data.size() >= 5 && data.startsWith("%PDF-");
}

bool PdfReader::canRead(QIODevice& device) {
  if (!device.isOpen()) {
    if (!device.open(QIODevice::ReadOnly)) {
      return false;
    }
  }

  const qint64 origPos = device.pos();
  device.seek(0);
  const QByteArray header = device.read(5);
  device.seek(origPos);

  return checkMagic(header);
}

bool PdfReader::canRead(const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray header = file.read(5);
  return checkMagic(header);
}

#ifdef Q_OS_MACOS

ImageMetadataLoader::Status PdfReader::readMetadata(QIODevice& device,
                                                    const VirtualFunction<void, const ImageMetadata&>& out) {
  auto* file = qobject_cast<QFile*>(&device);
  if (!file) {
    return ImageMetadataLoader::FORMAT_NOT_RECOGNIZED;
  }
  return readMetadata(file->fileName(), out);
}

ImageMetadataLoader::Status PdfReader::readMetadata(const QString& filePath,
                                                    const VirtualFunction<void, const ImageMetadata&>& out) {
  if (!canRead(filePath)) {
    return ImageMetadataLoader::FORMAT_NOT_RECOGNIZED;
  }

  PdfDocumentManager& mgr = PdfDocumentManager::instance();

  // Use stored import DPI if available, otherwise default
  const int renderDpi = getImportDpi(filePath);

  // Collect page metadata under lock, then release before callbacks
  struct PageInfo {
    int widthPx;
    int heightPx;
  };
  std::vector<PageInfo> pages;

  {
    QMutexLocker lock(&mgr.mutex());

    if (!mgr.ensureLoaded(filePath)) {
      return ImageMetadataLoader::GENERIC_ERROR;
    }

    CGPDFDocumentRef doc = mgr.document();
    if (!doc) {
      qWarning() << "PdfReader: Document is null after load:" << filePath;
      return ImageMetadataLoader::GENERIC_ERROR;
    }

    const size_t pageCount = CGPDFDocumentGetNumberOfPages(doc);

    if (pageCount == 0) {
      qWarning() << "PdfReader: PDF has no pages:" << filePath;
      return ImageMetadataLoader::NO_IMAGES;
    }

    constexpr double maxDimension = static_cast<double>(std::numeric_limits<int>::max());
    pages.reserve(pageCount);

    for (size_t i = 1; i <= pageCount; ++i) {
      CGPDFPageRef page = CGPDFDocumentGetPage(doc, i);
      if (!page) {
        continue;
      }

      // Get display size (accounts for rotation)
      CGSize displaySize = getDisplaySize(page);
      const CGFloat userUnit = getUserUnit(page);

      const double calcWidth = displaySize.width * renderDpi / 72.0 * userUnit + 0.5;
      const double calcHeight = displaySize.height * renderDpi / 72.0 * userUnit + 0.5;

      if (calcWidth <= 0 || calcHeight <= 0 || calcWidth > maxDimension || calcHeight > maxDimension) {
        qWarning() << "PdfReader: Invalid page dimensions for page" << i << "- skipping";
        continue;
      }

      pages.push_back({static_cast<int>(calcWidth), static_cast<int>(calcHeight)});
    }
  }  // Lock released here before callbacks

  // Now invoke callbacks without holding the lock (prevents potential deadlocks)
  for (const PageInfo& info : pages) {
    out(ImageMetadata(QSize(info.widthPx, info.heightPx), Dpi(renderDpi, renderDpi)));
  }

  return ImageMetadataLoader::LOADED;
}

QImage PdfReader::readImage(const QString& filePath, int pageNum, int dpi) {
  PdfDocumentManager& mgr = PdfDocumentManager::instance();
  QMutexLocker lock(&mgr.mutex());

  if (!mgr.ensureLoaded(filePath)) {
    qWarning() << "PdfReader: Failed to load PDF for rendering:" << filePath;
    return QImage();
  }

  CGPDFDocumentRef doc = mgr.document();

  const size_t pageIndex = static_cast<size_t>(pageNum + 1);
  const size_t totalPages = CGPDFDocumentGetNumberOfPages(doc);

  if (pageIndex < 1 || pageIndex > totalPages) {
    qWarning() << "PdfReader: Invalid page number:" << pageNum << "for PDF with" << totalPages << "pages";
    return QImage();
  }

  CGPDFPageRef page = CGPDFDocumentGetPage(doc, pageIndex);
  if (!page) {
    qWarning() << "PdfReader: Failed to get page" << pageNum;
    return QImage();
  }

  const CGRect box = getEffectiveBox(page);
  const int rotation = CGPDFPageGetRotationAngle(page);

  // Get display size (accounts for rotation)
  CGSize displaySize = getDisplaySize(page);
  const CGFloat userUnit = getUserUnit(page);

  constexpr double maxDim = static_cast<double>(std::numeric_limits<int>::max());
  const CGFloat scale = static_cast<CGFloat>(dpi) / 72.0 * userUnit;

  const double calcWidth = displaySize.width * scale + 0.5;
  const double calcHeight = displaySize.height * scale + 0.5;

  if (calcWidth <= 0 || calcHeight <= 0 || calcWidth > maxDim || calcHeight > maxDim) {
    qWarning() << "PdfReader: Invalid render dimensions for page" << pageNum;
    return QImage();
  }

  const int widthPx = static_cast<int>(calcWidth);
  const int heightPx = static_cast<int>(calcHeight);

  QImage image(widthPx, heightPx, QImage::Format_RGB32);
  image.fill(Qt::white);

  // Use autoreleasepool to prevent memory buildup during batch operations
  @autoreleasepool {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        image.bits(),
        widthPx,
        heightPx,
        8,
        image.bytesPerLine(),
        colorSpace,
        kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(colorSpace);

    if (!context) {
      qWarning() << "PdfReader: Failed to create bitmap context for page" << pageNum;
      return QImage();
    }

    CGContextSetInterpolationQuality(context, kCGInterpolationHigh);

    // Scale to requested DPI in user space (72dpi points).
    CGContextScaleCTM(context, scale, scale);

    // Manual transform: translate to box origin and apply page rotation.
    CGContextTranslateCTM(context, -box.origin.x, -box.origin.y);
    if (rotation == 90) {
      CGContextTranslateCTM(context, 0, box.size.width);
      CGContextRotateCTM(context, -M_PI_2);
    } else if (rotation == 180) {
      CGContextTranslateCTM(context, box.size.width, box.size.height);
      CGContextRotateCTM(context, M_PI);
    } else if (rotation == 270) {
      CGContextTranslateCTM(context, box.size.height, 0);
      CGContextRotateCTM(context, M_PI_2);
    }

    CGContextDrawPDFPage(context, page);
    CGContextRelease(context);
  }

  // Set DPI metadata
  const int dotsPerMeter = static_cast<int>(dpi / 0.0254 + 0.5);
  image.setDotsPerMeterX(dotsPerMeter);
  image.setDotsPerMeterY(dotsPerMeter);

  return image;
}

PdfReader::PdfInfo PdfReader::readPdfInfo(const QString& filePath) {
  PdfInfo info;

  if (!canRead(filePath)) {
    return info;
  }

  PdfDocumentManager& mgr = PdfDocumentManager::instance();
  QMutexLocker lock(&mgr.mutex());

  if (!mgr.ensureLoaded(filePath)) {
    return info;
  }

  CGPDFDocumentRef doc = mgr.document();
  if (!doc) {
    return info;
  }

  info.pageCount = static_cast<int>(CGPDFDocumentGetNumberOfPages(doc));
  info.detectedDpi = detectEffectiveDpi(doc);

  qDebug() << "PdfReader: Detected DPI for" << filePath << ":" << info.detectedDpi
           << "pages:" << info.pageCount;

  return info;
}

void PdfReader::setImportDpi(const QString& filePath, int dpi) {
  QMutexLocker lock(&s_importDpiMutex);
  s_importDpiMap[filePath] = dpi;
  qDebug() << "PdfReader: Set import DPI for" << filePath << "to" << dpi;
}

int PdfReader::getImportDpi(const QString& filePath) {
  QMutexLocker lock(&s_importDpiMutex);
  auto it = s_importDpiMap.find(filePath);
  if (it != s_importDpiMap.end()) {
    return it->second;
  }
  return DEFAULT_RENDER_DPI;
}

#endif  // Q_OS_MACOS
