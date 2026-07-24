// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_
#define SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_

#include <memory>

#include "AbstractCacheDrivenOutputTask.h"
#include "NonCopyable.h"

class QPolygonF;
class PageInfo;
class AbstractFilterDataCollector;
class ImageTransformation;

namespace ocr {
class CacheDrivenTask;
}

namespace export_ {

class CacheDrivenTask : public AbstractCacheDrivenOutputTask {
  DECLARE_NON_COPYABLE(CacheDrivenTask)

 public:
  explicit CacheDrivenTask(std::shared_ptr<ocr::CacheDrivenTask> ocrTask);

  ~CacheDrivenTask() override;

  void process(const PageInfo& pageInfo,
               AbstractFilterDataCollector* collector,
               const ImageTransformation& xform,
               const QPolygonF& contentRectPhys) override;

 private:
  std::shared_ptr<ocr::CacheDrivenTask> m_ocrTask;
};
}  // namespace export_
#endif  // SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_
