// Copyright (C) 2024  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_FINALIZE_CACHEDRIVENTASK_H_
#define SCANTAILOR_FINALIZE_CACHEDRIVENTASK_H_

#include <memory>

#include "NonCopyable.h"

class QPolygonF;
class PageInfo;
class AbstractFilterDataCollector;
class ImageTransformation;

namespace output {
class CacheDrivenTask;
}

namespace finalize {
class Settings;

class CacheDrivenTask {
  DECLARE_NON_COPYABLE(CacheDrivenTask)

 public:
  CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> nextTask, std::shared_ptr<Settings> settings);

  virtual ~CacheDrivenTask();

  void process(const PageInfo& pageInfo,
               AbstractFilterDataCollector* collector,
               const ImageTransformation& xform,
               const QPolygonF& contentRectPhys);

 private:
  std::shared_ptr<output::CacheDrivenTask> m_nextTask;
  std::shared_ptr<Settings> m_settings;
};
}  // namespace finalize
#endif  // SCANTAILOR_FINALIZE_CACHEDRIVENTASK_H_
