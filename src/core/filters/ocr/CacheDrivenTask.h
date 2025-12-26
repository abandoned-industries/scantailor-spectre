// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_CACHEDRIVENTASK_H_
#define SCANTAILOR_OCR_CACHEDRIVENTASK_H_

#include <memory>

#include "AbstractCacheDrivenOutputTask.h"
#include "NonCopyable.h"

class QPolygonF;
class PageInfo;
class AbstractFilterDataCollector;
class ImageTransformation;

namespace output {
class CacheDrivenTask;
}

namespace ocr {
class Settings;

class CacheDrivenTask : public AbstractCacheDrivenOutputTask {
  DECLARE_NON_COPYABLE(CacheDrivenTask)

 public:
  CacheDrivenTask(std::shared_ptr<output::CacheDrivenTask> outputTask, std::shared_ptr<Settings> settings);

  ~CacheDrivenTask() override;

  void process(const PageInfo& pageInfo,
               AbstractFilterDataCollector* collector,
               const ImageTransformation& xform,
               const QPolygonF& contentRectPhys) override;

 private:
  std::shared_ptr<output::CacheDrivenTask> m_outputTask;
  std::shared_ptr<Settings> m_settings;
};
}  // namespace ocr
#endif  // SCANTAILOR_OCR_CACHEDRIVENTASK_H_
