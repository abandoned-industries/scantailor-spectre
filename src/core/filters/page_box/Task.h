// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_PAGE_BOX_TASK_H_
#define SCANTAILOR_PAGE_BOX_TASK_H_

#include <QRectF>
#include <memory>

#include "FilterResult.h"
#include "NonCopyable.h"
#include "PageId.h"

class TaskStatus;
class FilterData;
class DebugImages;

namespace select_content {
class Task;
}

namespace page_box {
class Filter;
class Settings;

class Task {
  DECLARE_NON_COPYABLE(Task)

 public:
  Task(std::shared_ptr<Filter> filter,
       std::shared_ptr<select_content::Task> nextTask,
       std::shared_ptr<Settings> settings,
       const PageId& pageId,
       bool batch,
       bool debug);

  virtual ~Task();

  FilterResultPtr process(const TaskStatus& status, const FilterData& data);

 private:
  class UiUpdater;

  std::shared_ptr<Filter> m_filter;
  std::shared_ptr<select_content::Task> m_nextTask;
  std::shared_ptr<Settings> m_settings;
  std::unique_ptr<DebugImages> m_dbg;
  PageId m_pageId;
  bool m_batchProcessing;
};
}  // namespace page_box
#endif
