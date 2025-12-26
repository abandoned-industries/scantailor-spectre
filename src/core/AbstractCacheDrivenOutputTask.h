// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_ABSTRACTCACHEDRIVENOUTPUTTASK_H_
#define SCANTAILOR_ABSTRACTCACHEDRIVENOUTPUTTASK_H_

class QPolygonF;
class PageInfo;
class AbstractFilterDataCollector;
class ImageTransformation;

/**
 * Abstract base class for cache-driven output-stage tasks (output, ocr, export).
 * This allows finalize::CacheDrivenTask to polymorphically call whichever task
 * is at the end of the processing chain for thumbnails.
 */
class AbstractCacheDrivenOutputTask {
 public:
  virtual ~AbstractCacheDrivenOutputTask() = default;

  virtual void process(const PageInfo& pageInfo,
                       AbstractFilterDataCollector* collector,
                       const ImageTransformation& xform,
                       const QPolygonF& contentRectPhys) = 0;
};

#endif  // SCANTAILOR_ABSTRACTCACHEDRIVENOUTPUTTASK_H_
