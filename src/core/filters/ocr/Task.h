// Copyright (C) 2024  ScanTailor Spectre contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_OCR_TASK_H_
#define SCANTAILOR_OCR_TASK_H_

#include <memory>

#include "AbstractOutputTask.h"
#include "FilterResult.h"
#include "NonCopyable.h"
#include "OutputFileNameGenerator.h"
#include "PageId.h"

class TaskStatus;
class FilterData;
class QImage;
class QPolygonF;

namespace output {
class Task;
}

namespace ocr {
class Filter;
class Settings;

class Task : public AbstractOutputTask {
  DECLARE_NON_COPYABLE(Task)

 public:
  Task(std::shared_ptr<Filter> filter,
       std::shared_ptr<output::Task> outputTask,
       std::shared_ptr<Settings> settings,
       const PageId& pageId,
       const OutputFileNameGenerator& outFileNameGen,
       bool batch);

  ~Task() override;

  FilterResultPtr process(const TaskStatus& status, const FilterData& data, const QPolygonF& contentRectPhys) override;

 private:
  class UiUpdater;

  void performOcr(const QImage& image);

  std::shared_ptr<Filter> m_filter;
  std::shared_ptr<output::Task> m_outputTask;
  std::shared_ptr<Settings> m_settings;
  PageId m_pageId;
  OutputFileNameGenerator m_outFileNameGen;
};
}  // namespace ocr
#endif  // SCANTAILOR_OCR_TASK_H_
