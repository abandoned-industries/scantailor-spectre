// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_EXPORT_TASK_H_
#define SCANTAILOR_EXPORT_TASK_H_

#include <memory>

#include "AbstractOutputTask.h"
#include "FilterResult.h"
#include "NonCopyable.h"
#include "PageId.h"

class TaskStatus;
class FilterData;
class QPolygonF;

namespace ocr {
class Task;
}

namespace export_ {
class Filter;

class Task : public AbstractOutputTask {
  DECLARE_NON_COPYABLE(Task)

 public:
  Task(std::shared_ptr<Filter> filter, std::shared_ptr<ocr::Task> ocrTask, const PageId& pageId, bool batch);

  ~Task() override;

  FilterResultPtr process(const TaskStatus& status, const FilterData& data, const QPolygonF& contentRectPhys) override;

 private:
  class UiUpdater;
  std::shared_ptr<Filter> m_filter;
  std::shared_ptr<ocr::Task> m_ocrTask;
  PageId m_pageId;
  bool m_batchProcessing;
};
}  // namespace export_
#endif  // SCANTAILOR_EXPORT_TASK_H_
