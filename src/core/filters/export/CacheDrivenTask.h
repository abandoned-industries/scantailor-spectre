// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_
#define SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_

#include <memory>

#include "NonCopyable.h"

class QPolygonF;
class PageInfo;
class AbstractFilterDataCollector;
class ImageTransformation;

namespace output {
class CacheDrivenTask;
}

namespace export_ {

class CacheDrivenTask {
  DECLARE_NON_COPYABLE(CacheDrivenTask)

 public:
  explicit CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask);

  virtual ~CacheDrivenTask();

  void process(const PageInfo& pageInfo,
               AbstractFilterDataCollector* collector,
               const ImageTransformation& xform,
               const QPolygonF& contentRectPhys);

 private:
  std::shared_ptr<output::CacheDrivenTask> m_outputTask;
};
}  // namespace export_
#endif  // SCANTAILOR_EXPORT_CACHEDRIVENTASK_H_
